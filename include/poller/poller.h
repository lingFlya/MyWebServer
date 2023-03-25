/**
 * @author 2mu
 * @date 2022/7/26
 * @brief 封装IO多路复用模型，Linux下封装epoll，macos下封装kqueue
 * 1. 暂时只封装了linux下的epoll
 */

#ifndef WEBSERVER_POLLER_H
#define WEBSERVER_POLLER_H

#include <sys/types.h>


struct poller_message_t
{
    int (*append)(const void *, size_t *, struct poller_message_t *);
    char data[0];
};

struct poller_data
{
#define PD_OP_READ          1
#define PD_OP_WRITE         2
#define PD_OP_LISTEN        3
#define PD_OP_CONNECT		4
#define PD_OP_EVENT			8
#define PD_OP_NOTIFY		9
#define PD_OP_TIMER			10
    short operation;
    unsigned short iovcnt;
    int fd;
    void* context;
    union
    {
        struct poller_message_t *message;
        struct iovec* write_iov;
        void* result;
    };
};

struct poller_result
{
#define PR_ST_SUCCESS		0
#define PR_ST_FINISHED		1
#define PR_ST_ERROR			2
#define PR_ST_DELETED		3
#define PR_ST_MODIFIED		4
#define PR_ST_STOPPED		5
    int state;
    int error;
    struct poller_data data;
};

struct poller_params
{
    size_t max_open_files;
    struct poller_message_t *(*create_message)(void *);
    int (*partial_written)(size_t, void *);
    void (*callback)(struct poller_result *, void *);
    void *context;
};

// 前向声明
typedef struct __poller poller_t;
#ifdef __cplusplus
extern "C"
{
#endif
    poller_t *poller_create(const struct poller_params* params);
    int poller_start(poller_t* poller);
    int poller_add(const struct poller_data* data, int timeout, poller_t* poller);
    int poller_del(int fd, poller_t* poller);
    int poller_mod(const struct poller_data* data, int timeout, poller_t* poller);
    int poller_set_timeout(int fd, int timeout, poller_t *poller);
    int poller_add_timer(const struct timespec *value, void *context, poller_t *poller);
    void poller_stop(poller_t *poller);
    void poller_destroy(poller_t *poller);
#ifdef __cplusplus
}
#endif

#endif //WEBSERVER_POLLER_H
