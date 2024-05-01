/**
 * @author  2mu
 * @date    2023/6/23
 * @brief   时间堆实现定时器, 参照MySQL的thr_timer设计;
 * 微秒级别的定时器, 因为是pthread_cond_timedwait实现定时等待, 该函数第二个参数struct_timespec支持到纳秒
 */

#ifndef THR_TIMER_H
#define THR_TIMER_H

#include <ctime>
#include <cstdint>
#include <boost/noncopyable.hpp>

#include <pthread.h>

#include "queues.h"

#ifndef cmp_timespec
/**
 * @brief   Compare two timespec structs.
 * @return  1 If TS1 ends after TS2. 0 If TS1 is equal to TS2. -1 If TS1 ends before TS2.
 */
#define cmp_timespec(TS1, TS2) \
  ((TS1.tv_sec > TS2.tv_sec || \
    (TS1.tv_sec == TS2.tv_sec && TS1.tv_nsec > TS2.tv_nsec)) ? 1 : \
   ((TS1.tv_sec < TS2.tv_sec || \
     (TS1.tv_sec == TS2.tv_sec && TS1.tv_nsec < TS2.tv_nsec)) ? -1 : 0))
#endif /* !cmp_timespec */

struct thr_timer
{
    bool expired;
    int index_in_queue;     // 该定时器在时间堆中的idx
    void (*func)(void*);
    void* func_args;
    struct timespec expire_time;
};

typedef struct thr_timer thr_timer_t;

class TimerManager
{
public:
    TimerManager(unsigned int init_size_for_timer_queue = 128);
    ~TimerManager();

    /**
     * @brief 新增定时器, 微秒级别的...
     * 相当于是mysql源码中的 thr_timer_settime 函数
     */
    bool addTimer(thr_timer_t *timer_data, unsigned long long micro_seconds);

    /**
     * @brief 移除已经存在的定时器
     * 相当于是mysql源码中的 thr_timer_end 函数
     */
    void removeTimer(thr_timer_t *timer_data);

private:
    bool _init();
    
    void _processTimers(struct timespec* now);
    static void* _timerHandler(void*);


private:
    bool            m_inited;       // 是否已经初始化
    pthread_t       m_thread;
    pthread_mutex_t m_timerMtx;
    pthread_cond_t  m_timerCond;
    struct timespec m_nextTimerExpireTime;
    QUEUE           m_timerQueue;
};


// Functions for handling one timer

/**
 * @brief: 初始化thr_timer_t结构体
 */
void thr_timer_init(thr_timer_t *timer_data, void(*function)(void*), void *arg);


#endif // THR_TIMER_H
