/**
 * @author 2mu
 * @date 2022/7/31
 * @brief 定时器
 */

#ifndef WEBSERVER_TIMER_H
#define WEBSERVER_TIMER_H

#include <queue>
#include <functional>
#include <memory>

#include "thread/mutex.h"
#include "util/util.h"

class Timer : public std::enable_shared_from_this<Timer>
{
    friend class TimerManager;

public:
    typedef std::shared_ptr<Timer> ptr;

    Timer(uint64_t interval, std::function<void()> cb, bool recurring)
            :m_recurring(recurring) ,m_interval(interval) ,m_cb(cb)
    {
        m_timeout = util::current_time() + m_interval;
    }

    /**
     * @brief 定时器比较器
     */
    struct Comparator{
        bool operator()(const Timer::ptr& lhs, const Timer::ptr& rhs)const;
    };

    /**
     * @brief 取消定时器回调函数
     */
    void Cancel()
    {
        if(m_cb)
        {
            m_cb = nullptr;
        }
    }

    /**
     * @brief 返回到期时间点
     */
    uint64_t GetTimeOut()const
    {
        return m_timeout;
    }

private:
    bool                    m_recurring;/// 是否循环定时器
    uint64_t                m_interval;/// 时间间隔
    uint64_t                m_timeout;/// 精确的执行时间点
    std::function<void()>   m_cb;/// 回调函数, 若为nullptr，则该定时器已被标记待删除。
};

/**
 * @brief 底层容器是堆，删除顺序一定是有序的。
 */
class TimerManager
{
public:
    typedef std::shared_ptr<TimerManager> ptr;
    using Container = std::priority_queue<Timer::ptr, std::vector<Timer::ptr>, Timer::Comparator>;

    TimerManager();
    ~TimerManager();

    /**
     * @brief 添加定时器
     * @param interval 多久之后触发，单位：毫秒
     * @param cb       到期时候的回调函数
     * @param recurring 是否周期性触发
     * @return Timer::ptr 创建的定时器
     */
    Timer::ptr AddTimer(uint64_t interval, std::function<void()>&& cb, bool recurring);
    void AddTimer(Timer::ptr timer);

    // 删除定时器(实际上没有删除,只是将cb置为null)
    void DelTimer(Timer::ptr timer);

    int GetTimerfd()const
    {
        return m_tfd;
    }

private:
    int                     m_tfd;      /// 对应的timerfd
    WebServer::Mutex        m_mtx;
    Container               m_container;/// 存储定时器的底层容器
};

#endif //WEBSERVER_TIMER_H
