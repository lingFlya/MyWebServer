/**
 * @author  2mu
 * @date    2023/10/28
 * @brief   mysql timer对应的测试, 大部分直接从mysql源码中拉下来;
 * 进行部分修改, 去除mysql特有的东西; 比如锁呀...
 * 
 * 还有语法的改变, 因为本来是用C语言写的, C++语法上是有差别的, 需注意
 */
#include <stdio.h>
#include <ctype.h>

#include <pthread.h>
#include <signal.h>
#include <time.h>

#include "timer/thr_timer.h"
#include "util/singleton.h"
#include "util/util.h"

#include "thread_init.h"


static pthread_cond_t COND_thread_count;
static pthread_mutex_t LOCK_thread_count;
static unsigned int thread_count, benchmark_runs;
// 测试需要调用的具体函数...
static unsigned int test_to_run= 1;

// mysql用的全局static对象, 我这里用全局对象
TimerManager gTimerManager;


// 定时任务是发送信号...
static void send_signal(void *arg)
{
    struct st_my_thread_var *current_my_thread_var = (struct st_my_thread_var*)arg;
    printf("sending signal(wake condition variable)\n");
    fflush(stdout);

    // 加锁 唤醒条件变量, 保证顺序
    pthread_mutex_lock(&current_my_thread_var->mutex);
    pthread_cond_signal(&current_my_thread_var->suspend);
    pthread_mutex_unlock(&current_my_thread_var->mutex);
}


static void run_thread_test(int param)
{
    struct st_my_thread_var *current_my_thread_var = my_thread_var;

    thr_timer_t timer_data;
    thr_timer_init(&timer_data, send_signal, current_my_thread_var);
    
    for (int i = 1; i <= 10; i++)
    {
        // 所有定时器的等待时间都没有超过10秒, 测试会创建两个线程执行该函数;
        // 一个线程添加的定时器到期时间慢慢增加, 一个线程添加的定时器到期时间慢慢减少
        int wait_time = param ? 11-i : i;
        
        struct timespec start_time;
        clock_gettime(CLOCK_REALTIME, &start_time);
        
        pthread_mutex_lock(&current_my_thread_var->mutex);
        if (!gTimerManager.addTimer(&timer_data, wait_time * 1e6))
        {
            printf("Thread %s: add timers failed!\n", my_thread_name());
            break;
        }
        if (wait_time == 3)
        {
            // 翻译: 不需要定时器的模拟; 即wait_time=3时, 不阻塞等待定时器唤醒该线程;
            printf("Thread %s: Simulation of no timer needed\n", my_thread_name());
            fflush(stdout);
        }
        else
        {
            // 防止虚假唤醒, for循环等待定时器到期
            for (int retry=0; !timer_data.expired && retry < 10 ; retry++)
            {
                printf("Thread %s: Waiting %d sec\n", my_thread_name(), wait_time);
                pthread_cond_wait(&current_my_thread_var->suspend, &current_my_thread_var->mutex);
            }
            
            // 条件变量被唤醒10次, 定时任务却没有到期, 被执行完成; 说明出错了
            if (!timer_data.expired)
            {
                printf("Thread %s: didn't get an timer. Aborting!\n", my_thread_name());
                break;
            }
        }
        pthread_mutex_unlock(&current_my_thread_var->mutex);
        struct timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        int micro_second = (now.tv_sec * 1000000ULL + now.tv_nsec / 1000ULL) 
            - (start_time.tv_sec * 1000000ULL + start_time.tv_nsec / 1000ULL);
        // 计算实际等待时间, 和预设时间; wait_time=3时, 没有等待是正常的;
        printf("Thread %s: Slept for %f (%d) sec\n", 
            my_thread_name(), (int) (micro_second)/1000000.0, wait_time);
        fflush(stdout);
        gTimerManager.removeTimer(&timer_data);
        fflush(stdout);
    }
    return;
}


static void run_thread_benchmark(int param)
{
    struct st_my_thread_var *current_my_thread_var = my_thread_var;
    thr_timer_t timer_data;
    
    thr_timer_init(&timer_data, send_signal, current_my_thread_var);

    // param是1000000, (多线程)添加1百万个1秒的定时器, 且立马删除...
    for (int i = 1 ; i <= param ; i++)
    {
        if (!gTimerManager.addTimer(&timer_data, 1000000))
        {
            printf("Thread %s: add timers failed!\n",my_thread_name());
            break;
        }
        gTimerManager.removeTimer(&timer_data);
    }
}


static void *start_thread(void *arg)
{
    // 创建线程特有的内存, 变量等
    my_thread_init();
    printf("(param=%d) Thread %s started\n",*((int*) arg), my_thread_name());
    fflush(stdout);
    
    switch (test_to_run) {
        case 1:
            run_thread_test(*((int*) arg));
            break;
        case 2:
            run_thread_benchmark(benchmark_runs);
            break;
        case 3:
            #ifdef HAVE_TIMER_CREATE
                // 系统是否支持 timer_create 函数 ? 看起来是只是单纯在测试 timer_create, timer_settime, timer_delete的性能
                run_timer_benchmark(benchmark_runs);
            #endif
            break;
    }
    free((unsigned char*) arg);
    pthread_mutex_lock(&LOCK_thread_count);
    thread_count--;
    pthread_cond_broadcast(&COND_thread_count); /* Tell main we are ready */
    pthread_mutex_unlock(&LOCK_thread_count);
    my_thread_end();
    return 0;
}


/* Start a lot of threads that will run with timers */

static void run_test()
{
    // mysql代码这里会判断是否初始化成功, 内存足够, 必然成功, 所以这里不判断了;
    TimerManager timer_manager(5);

    // 初始化互斥锁
    pthread_mutex_init(&LOCK_thread_count, MY_MUTEX_INIT_FAST);
    // 初始化条件变量
    pthread_cond_init(&COND_thread_count, NULL);

    pthread_t tid;
    pthread_attr_t thr_attr;
    pthread_attr_init(&thr_attr);
    pthread_attr_setscope(&thr_attr,PTHREAD_SCOPE_PROCESS);
    
    printf("Main thread: %s\n", my_thread_name());

    // 每次测试过程, 都固定创建2个线程...
    for (int i=0 ; i < 2; i++)
    {
        int* param=(int*) malloc(sizeof(int));
        *param= i;
        pthread_mutex_lock(&LOCK_thread_count);
        
        int error = 0;
        if ((error= pthread_create(&tid, &thr_attr, start_thread, (void*) param)))
        {
            printf("Can't create thread %d, error: %d\n",i,error);
            exit(1);
        }
        thread_count++;
        pthread_mutex_unlock(&LOCK_thread_count);
    }
    pthread_attr_destroy(&thr_attr);
    
    pthread_mutex_lock(&LOCK_thread_count);
    while (thread_count)
    {
        pthread_cond_wait(&COND_thread_count, &LOCK_thread_count);
    }
    pthread_mutex_unlock(&LOCK_thread_count);
    // 此时定时器 肯定 只剩余一个定时器了...
    // ASSERT(timer_queue.elements == 1);
    printf("Test succeeded\n");
}


int main(int argc __attribute__((unused)), char **argv __attribute__((unused)))
{
    my_thread_global_init();

    my_thread_init(); // 每个线程都要初始化线程特有变量
    my_mutex_init();// 初始化全局mutex attr对象
    if(argc > 1 && argv[1][0] == '-')
    {
        switch(argv[1][1])
        {
            case '#':
                test_to_run = 1;
                break;
            case 'b':
                test_to_run = 2;
                benchmark_runs = atoi(argv[1] + 2);
                break;
            case 't':
                test_to_run = 3;
                benchmark_runs = atoi(argv[1] + 2);
                break;
            default:
                printf("param error!\n");
        }
    }
    // 默认压测一百万个定时器
    if(!benchmark_runs)
        benchmark_runs = 1000000;

    run_test();
    my_mutex_end();// 析构全局mutex attr对象
    my_thread_end();
    return 0;
}
