/***********************************************************
 *@author RedDragon
 *@date 2020/9/5
 *@brief 
***********************************************************/

#ifndef WEBSERVER_THREADPOOL_H
#define WEBSERVER_THREADPOOL_H

#include <pthread.h>
#include "requestData.h"

const int THREADPOOL_INVALID = -1;// 线程不合法

const int MAX_THREADS = 1024;
const int MAX_QUEUE = 65535;


typedef enum
{
    immediate_shutdown = 1,// 立即
    graceful_shutdown  = 2// 优美
}threadpool_shutdown_t;


/**************************************
 * @struct threadpool_task
 * @brief 任务
 *************************************/
typedef struct
{
    void (*function)(void*);// 函数指针指向执行任务的函数
    void *argument;// 传递给函数的参数
} threadpool_task_t;

/*****************************************
 * @struct threadpool
 * @brief the threadpool struct
 ****************************************/
struct threadpool_t
{
    pthread_mutex_t lock;// 互斥量
    pthread_cond_t notify;// 条件变量，用来通知worker thread
    pthread_t *threads;// 包含工作线程的ID

    int thread_count;// 线程个数
    threadpool_task_t *queue;// 任务队列
    int queue_size;// 任务队列大小
    int head;
    int tail;
    int count;// 挂起任务数
    int shutdown;// 线程池是否正在关闭的标志
    int started;// 启动的线程数
};

// 新建一个线程池
threadpool_t* threadpool_create(int thread_count, int queue_size, int flags);
// 往线程池中增加一个任务
int threadpool_add(threadpool_t *pool, void (*function)(void*));
// 摧毁线程池
int threadpool_destory(threadpool_t *pool, int flags);
// 释放线程池
int threadpool_free(threadpool_t *pool);

// 将被pthread_create创建的线程
static void *threadpool_thread(void *threadpool);


#endif //WEBSERVER_THREADPOOL_H