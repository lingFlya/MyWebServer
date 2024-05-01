#ifndef THREAD_INIT_H
#define THREAD_INIT_H

#include <pthread.h>

#include <cstdio>
#include <cstdint>


#define THREAD_NAME_SIZE 10
#define STACK_DIRECTION -1

// mysql定义的全局线程属性对象
#ifdef PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP
    extern pthread_mutexattr_t my_fast_mutexattr;
    #define MY_MUTEX_INIT_FAST &my_fast_mutexattr
#else
    #define MY_MUTEX_INIT_FAST NULL
#endif
#ifdef PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
    extern pthread_mutexattr_t my_errorcheck_mutexattr;
    #define MY_MUTEX_INIT_ERRCHK &my_errorcheck_mutexattr
#else
    #define MY_MUTEX_INIT_ERRCHK NULL
#endif

/**
 * @brief 删除了一些元素, mysql源码中还有其它一些成员
 */
struct st_my_thread_var
{
    int thr_errno;
    pthread_cond_t suspend;
    pthread_mutex_t mutex;
    
    pthread_mutex_t * volatile current_mutex;
    pthread_cond_t * volatile current_cond;
    pthread_t pthread_self;
    
    uint64_t id;
    int volatile abort;
    bool init;
    
    struct st_my_thread_var *next,**prev;
    unsigned int lock_type; /* used by conditional release the queue */
    void  *stack_ends_here;
    
    char name[THREAD_NAME_SIZE+1];
};

struct st_my_thread_var* _my_thread_var();

int set_thread_var(struct st_my_thread_var *var);

#define my_thread_var (_my_thread_var())

/**
 * @brief 返回当前线程名
 */
const char *my_thread_name();


void my_mutex_init();
void my_mutex_end();

bool my_thread_init();
bool my_thread_end();

/**
 * @brief 必须在最开始的时候调用, 初始化全局内容;
 * 比如 pthread_key_t 类型的全局变量
 */
bool my_thread_global_init();

#endif // THREAD_INIT_H
