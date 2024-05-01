#include "timer/thr_timer.h"

#include <cstddef>
#include <cstring>
#include <climits>
#include <errno.h>

#include "util/util.h"

#define set_timespec_time_nsec(ABSTIME, NSEC) do{     \
    unsigned long long _now_ = (NSEC);          \
    (ABSTIME).tv_sec = (_now_ / 1000000000ULL); \
    (ABSTIME).tv_nsec = (_now_ % 1000000000ULL);\
}while(0)

/**
 *@brief 获取当前纳秒绝对时间(系统时间), 并且设置到 ABSTIME 对象中去, 是一个 timespec 结构体
 * 注意: 这里使用的是系统时间, 也就是说修改系统时间, 可能影响定时器;
 */
#define set_timespec_nsec(ABSTIME, NSEC) set_timespec_time_nsec(ABSTIME, util::get_real_time_nsec() + (NSEC))

#define set_timespec(ABSTIME,SEC) set_timespec_nsec((ABSTIME),(SEC)*1000000000ULL)

#define set_max_time(abs_time) \
  { (abs_time)->tv_sec= INT_MAX; (abs_time)->tv_nsec= 0; }

static int compare_timespec(void* not_used __attribute__((unused)),
    unsigned char* left, unsigned char* right)
{
    return cmp_timespec((*(struct timespec*)left), (*(struct timespec*)right));
}

static thr_timer_t max_timer_data;

TimerManager::TimerManager(unsigned int init_size_for_timer_queue)
{
    init_queue(&m_timerQueue, init_size_for_timer_queue + 2, offsetof(thr_timer_t, expire_time),
        0, compare_timespec, NULL, offsetof(thr_timer_t, index_in_queue) + 1, 1);
    
    // Set dummy element with max time into the queue to simplify usage
    bzero(&max_timer_data, sizeof(max_timer_data));
    set_max_time(&max_timer_data.expire_time);
    queue_insert(&m_timerQueue, (unsigned char*) &max_timer_data);
    m_nextTimerExpireTime = max_timer_data.expire_time;

    _init();
}

TimerManager::~TimerManager()
{
    if(m_inited)
    {
        pthread_mutex_lock(&m_timerMtx);
        m_inited = false;
        pthread_cond_signal(&m_timerCond); // 唤醒工作线程准备退出
        pthread_mutex_unlock(&m_timerMtx); // 别忘记解锁, 不然下面的pthread_mutex_destroy能成功吗?

        pthread_join(m_thread, NULL);

        pthread_mutex_destroy(&m_timerMtx);
        pthread_cond_destroy(&m_timerCond);
        delete_queue(&m_timerQueue);
    }
}

bool TimerManager::addTimer(thr_timer_t* timer_data, unsigned long long micro_seconds)
{
    set_timespec_nsec(timer_data->expire_time, micro_seconds * 1000);
    timer_data->expired = 0;

    pthread_mutex_lock(&m_timerMtx);
    if(queue_insert_safe(&m_timerQueue, (unsigned char*)timer_data))
    {
        /**
         * @todo: 加错误日志
         */
        timer_data->expired = 1;
        pthread_mutex_unlock(&m_timerMtx);
        return false;
    }

    // 是否需要重新设置条件变量等待时间? 
    int reSchedule = cmp_timespec(m_nextTimerExpireTime, timer_data->expire_time);
    pthread_mutex_unlock(&m_timerMtx);
    if(reSchedule > 0)
    {
        pthread_cond_signal(&m_timerCond);
    }

    return true;
}

void TimerManager::removeTimer(thr_timer_t *timer_data)
{
    pthread_mutex_lock(&m_timerMtx);
    // 已经过期的定时器就不用管了, 否则就去移除它;
    if(!timer_data->expired)
    {
        /**
         * @todo: mysql源码这里有断言, 先忽略吧, 我没有实现动态断言函数
         * assert( queue_element(&m_timerQueue, timer_data->index_in_queue) == (unsigned char*)timer_data);
         */
        queue_remove(&m_timerQueue, timer_data->index_in_queue);
        timer_data->expired = true;
    }
    pthread_mutex_unlock(&m_timerMtx);
}


bool TimerManager::_init()
{
    // 自适应锁, 网上说效果类似自旋锁
    pthread_mutexattr_t fast_mutex;
    pthread_mutexattr_init(&fast_mutex);
    pthread_mutexattr_settype(&fast_mutex, PTHREAD_MUTEX_ADAPTIVE_NP);
    pthread_mutex_init(&m_timerMtx, &fast_mutex);
    pthread_mutexattr_destroy(&fast_mutex);

    pthread_cond_init(&m_timerCond, NULL);

    pthread_attr_t thr_attr;
    pthread_attr_init(&thr_attr);
    pthread_attr_setscope(&thr_attr, PTHREAD_SCOPE_PROCESS);
    /**
     * 最新版mariadb已经把这里线程栈变大了, mariadb的线程池有用到这个定时器; 线程栈8K不够 <https://github.com/MariaDB/server/commit/32b0b6434b368d722c21861888e18f393f7af11b>
     * 我这里暂时还是8K吧, 不够了再改
     * pthread_attr_setstacksize(&thr_attr, 64 * 1024);
     */
    m_inited = true;
    if(0 != pthread_create(&m_thread, &thr_attr, &_timerHandler, this))
    {
        m_inited = false;
        pthread_mutex_destroy(&m_timerMtx);
        pthread_cond_destroy(&m_timerCond);
        delete_queue(&m_timerQueue);
    }
    pthread_attr_destroy(&thr_attr);
    return m_inited;
}

void TimerManager::_processTimers(struct timespec* now)
{
    while(true)
    {
        thr_timer_t* timer_data = (thr_timer_t*)queue_top(&m_timerQueue);
        void (*func)(void*) = timer_data->func;
        void *func_arg = timer_data->func_args;
        timer_data->expired = true;

        queue_remove_top(&m_timerQueue);
        (*func)(func_arg);

        timer_data = (thr_timer_t*) queue_top(&m_timerQueue);
        // cmp_timespec是个宏, 所以 *now 必须加上括号, * 的优先级低
        if(cmp_timespec(timer_data->expire_time, (*now)) > 0)
            break;
    }
}

void* TimerManager::_timerHandler(void* arg)
{
    TimerManager* manager = (TimerManager*)arg;
    pthread_mutex_lock(&manager->m_timerMtx);
    while(manager->m_inited)
    {
        struct timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        struct timespec *topTime = &(((thr_timer_t*)queue_top(&manager->m_timerQueue))->expire_time);
        if(cmp_timespec((*topTime), now) <= 0)
        {
            // 堆顶定时器到期时间点 <= 当前时间点; 说明任务已到期, 需要去执行任务
            manager->_processTimers(&now);
            topTime = &(((thr_timer_t*)queue_top(&manager->m_timerQueue))->expire_time);
        }
        manager->m_nextTimerExpireTime = *topTime;
        /**
         * @brief 不确定pthread_cond_timedwait会不会修改第三个参数内容, 所以创建临时变量传进入; 而不是直接传 m_nextTimerExpireTime
         * 反过来说, cond已经阻塞, 结果 m_nextTimerExpireTime 在其它地方被修改了, 此时会发生什么?? 也很危险; 所以不要干这种事
         */
        struct timespec abstime = *topTime;
        int error = pthread_cond_timedwait(&manager->m_timerCond, &manager->m_timerMtx, &abstime);
        //  不清楚mariadb为什么要判断 ETIME, pthread_cond_timedwait文档中没说会返回 ETIME
        if(error && /*error != ETIME &&*/ error != ETIMEDOUT)
        {
            // 记录日志报错
        }
    }
    pthread_mutex_unlock(&manager->m_timerMtx);
    pthread_exit(0);
    return 0;
}


void thr_timer_init(thr_timer* timer_data, void(*function)(void*), void* arg)
{
    bzero(timer_data, sizeof(*timer_data));
    timer_data->func = function;
    timer_data->func_args = arg;
    timer_data->expired= 1;                       /* Not active */
}
