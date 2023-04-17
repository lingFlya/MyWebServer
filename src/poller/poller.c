#include "poller/poller.h"

#include <stdlib.h>

#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>

#include "poller/rbtree.h"
#include "poller/list.h"

#define POLLER_BUFSIZE      (256 * 1024)
#define POLLER_EVENT_MAX    256

struct __poller_node
{
    int state;
    int error;
    struct poller_data data;
#pragma pack(1)
    union
    {
        struct list_head list;
        struct rb_node rb;
    };
#pragma pack()
    char in_rbtrees;
    char removed;
    int event;
    struct timespec timeout;
    struct __poller_node* res;
};

struct __poller
{
    size_t max_open_files;
    struct poller_message_t *(*create_message)(void *);
    int (*partial_written)(size_t, void *);
    void (*cb)(struct poller_result *, void *);
    void *ctx;

    pthread_t tid;
    int pfd;
    int timerfd;
    int pipe_rd;
    int pipe_wr;
    int stopped;
    struct rb_root timeo_tree;
    struct rb_node *tree_first;
    struct rb_node *tree_last;
    struct list_head timeo_list;
    struct list_head no_timeo_list;
    struct __poller_node** nodes;
    pthread_mutex_t mutex;
    char buf[POLLER_BUFSIZE];
};

typedef struct epoll_event __poller_event_t;

static inline long __timeout_cmp(struct __poller_node* node1,struct __poller_node* node2)
{
    long ret = node1->timeout.tv_sec - node2->timeout.tv_sec;
    if(ret == 0)
        ret = node1->timeout.tv_nsec - node2->timeout.tv_nsec;
    return ret;
}

static inline int __poller_create_pfd()
{
    /**
     * Since Linux 2.6.8, the size argument is ignored, but must be greater than zero;
     * On  success,  these system calls return a non-negative file descriptor.  On error, -1 is returned, and errno is
     * set to indicate the error.
     */
    return epoll_create(1);
}


static inline int __poller_create_timerfd()
{
    // CLOCK_MONOTONIC指定一种不可设置的单调时钟，计量时间
    return timerfd_create(CLOCK_MONOTONIC, 0);
}


/**
 * @brief 给poller对象增加timerfd，并开启epoll对timerfd的轮询
 * @param timerfd 被增加的timerfd
 * @param poller poller对象指针
 * @return
 */
static inline int __poller_add_timerfd(int timerfd, poller_t* poller)
{
    struct epoll_event ev = {
            .events = EPOLLIN | EPOLLET,
            .data = {
                    .ptr = NULL
            }
    };
    // 使用epoll监听 timerfd的读事件 何时触发？读事件触发就表示定时器到期。
    // 疑问1：为什么是EPOLLET，边缘触发？
    return epoll_ctl(poller->pfd, EPOLL_CTL_ADD, timerfd, &ev);
}


static inline int __poller_set_timerfd(int timerfd, struct timespec* abstime, poller_t* poller)
{
    struct itimerspec timer = {
            .it_interval = {},
            .it_value = *abstime
    };
    return timerfd_settime(timerfd, 0, &timer, NULL);
}

static inline int __poller_add_fd(int fd, int event, void* data, poller_t* poller)
{
    struct epoll_event ev = {
            .events = event,
            .data = {
                    .ptr = data
            }
    };
    return epoll_ctl(poller->pfd, EPOLL_CTL_ADD, fd, &ev);
}

/**
 * @brief
 * @param [out] events 已就绪的事件
 * @param maxevents 限制epoll_wait最大能够返回已就绪的事件数量
 * @param poller
 * @return 正常返回就绪事件数量，发生错误则返回-1且设置errno
 */
static inline int __poller_wait(__poller_event_t* events, int maxevents, poller_t* poller)
{
    return epoll_wait(poller->pfd, events, maxevents, -1);
}


/**
 * @brief 返回触发事件的__poller_node
 * @param node
 * @return
 */
static inline void* __poller_event_data(const __poller_event_t* node)
{
    return node->data.ptr;
}


/**
 * @brief 给poller对象创建初始化一个timerfd
 */
static int __poller_create_timer(poller_t* poller)
{
    int timerfd = __poller_create_timerfd();

    if(timerfd >= 0)
    {
        if(__poller_add_timerfd(timerfd, poller) >= 0)
        {
            poller->timerfd = timerfd;
            return 0;
        }
        close(timerfd);
    }
    return -1;
}


/**
 * @brief 设置poller的定时器超时时间（从任务中找到最近一个要到期的任务，到期时间）
 * @param poller
 * @return
 */
static int __poller_set_timer(poller_t* poller)
{
    struct __poller_node *node = NULL;
    struct __poller_node *first = NULL;
    struct timespec abstime;

    pthread_mutex_lock(&poller->mutex);

    // 由于__poller_node结构体中有union,所以无法确定__poller_node对象是使用的list还是rbtree, 所以两种都支持

    // 通过timeo_list.next拿到其绑定的的__poller_node
    if (!ListEmpty(poller->timeo_list))
        node = list_entry(poller->timeo_list.next, struct __poller_node, list);

    /// 通过timeo_tree.first拿到其绑定的__poller_node, 选出其中最快要超时的一个node
    if(!poller->tree_first)
    {
        first = rb_entry(poller->tree_first, struct __poller_node, rb);
        if(!node || __timeout_cmp(first, node))
            node = first;
    }

    if(node)
    {
       abstime = node->timeout;
    }
    else
    {
        abstime.tv_sec = 0;
        abstime.tv_nsec = 0;
    }
    __poller_set_timerfd(poller->timerfd, &abstime, poller);
    pthread_mutex_unlock(&poller->mutex);
}


/**
 * @brief 初始化一个poller_t对象并返回其地址。
 * @param nodes_buf
 * @param params
 * @return 成功返回地址，失败NULL
 */
static inline poller_t* __poller_create(void** nodes_buf, const struct poller_params* params)
{
    poller_t* poller =  (poller_t*)malloc(sizeof(poller_t));
    if(!poller)
        return NULL;

    poller->pfd = __poller_create_pfd(); // 创建一个epoll的fd
    if(poller->pfd >= 0)
    {
        // 创建一个timerfd，并且与poller绑定
        if(__poller_create_timer(poller) >= 0)
        {
            int ret = pthread_mutex_init(&poller->mutex, NULL);
            if(ret == 0)
            {
                poller->nodes = (struct __poller_node**)nodes_buf;
                poller->max_open_files = params->max_open_files;
                poller->create_message = params->create_message;
                poller->partial_written = params->partial_written;
                poller->cb = params->callback;
                poller->ctx = params->context;

                poller->timeo_tree.rb_node = NULL;
                poller->tree_first = NULL;
                poller->tree_last = NULL;

                ListInit(&poller->timeo_list);
                ListInit(&poller->no_timeo_list);

                poller->stopped = 1;
                return poller;
            }
            errno = ret;
            close(poller->timerfd);
        }
    }
    free(poller);
    return NULL;
}

static int __poller_open_pipe(poller_t* poller)
{
    int pipefd[2];
    if(pipe(pipefd) >= 0)
    {
        // 疑问2： 为什么要打开一个匿名管道？
        if(__poller_add_fd(pipefd[0], EPOLLIN, (void*)1, poller) >= 0)
        {
            poller->pipe_rd = pipefd[0];
            poller->pipe_wr = pipefd[1];
            return 0;
        }
        close(pipefd[0]);
        close(pipefd[1]);
    }
    return -1;
}

static void* __poller_thread_routine(void* arg)
{
    poller_t *poller = (poller_t*)arg;
    __poller_event_t events[POLLER_EVENT_MAX];
    struct __poller_node time_node;
    struct __poller_node *node;

    int has_pipe_event;
    int nevents;

    while(1)
    {
        __poller_set_timer(poller);
        nevents = __poller_wait(events, POLLER_EVENT_MAX, poller);
        clock_gettime(CLOCK_MONOTONIC, &time_node.timeout);
        has_pipe_event = 0;
        for(int i = 0; i < nevents; ++i)
        {
            // 读取当前被触发的事件，根据不同的类型，进行处理
            node = (struct __poller_node*)__poller_event_data(&events[i]);
            if(node > (struct __poller_node*)1)
            {
                switch(node->data.operation)
                {
                    case PD_OP_READ:
                        __poller_handle_read(node, poller);
                    case PD_OP_WRITE:
                        __poller_handle_write(node, poller);
                    case PD_OP_LISTEN:
                        __poller_handle_listen(node, poller);
                    case PD_OP_CONNECT:
                        __poller_handle_connect(node, poller);
                    case PD_OP_EVENT:
                        __poller_handle_event(node, poller);
                    case PD_OP_NOTIFY:
                        __poller__handle_notify(node, poller);
                    case PD_OP_TIMER:
                        __poller__handle_timer(node, poller);
                }
            }
            else if (node == (struct __poller_node*)1)
            {
                has_pipe_event = 1;
            }
        }
        if(has_pipe_event)
        {
            if(__poller_handle_pipe(poller))
                break;
        }

        __poller_handle_timeout(&time_node, poller);
    }
}

poller_t* poller_create(const struct poller_params* params)
{
    // calloc函数是stdlib.h头文件中的，作用是分配内存，并且初始化所有位为0
    void** nodes_buf = (void**) calloc(params->max_open_files, sizeof(void*));
    poller_t* poller;

    if(nodes_buf)
    {
        poller = __poller_create(nodes_buf, params);
        if(poller)
            return poller;
        free(nodes_buf);
    }

    return NULL;
}

int poller_start(poller_t* poller)
{
    pthread_t tid;
    int ret;

    pthread_mutex_lock(&poller->mutex);
    if(__poller_open_pipe(poller) >= 0)
    {
        ret = pthread_create(&tid, NULL, __poller_thread_routine, poller);
        if(ret == 0)
        {
            poller->tid = tid;
            poller->stopped = 0;
        }
        else
        {
            errno = ret;
            close(poller->pipe_rd);
            close(poller->pipe_wr);
        }
    }
    pthread_mutex_unlock(&poller->mutex);
    return -poller->stopped;
}

