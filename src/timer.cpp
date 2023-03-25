#include "timer.h"

#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <cerrno>
#include <cstring>

#include "log/log.h"

static Logger::ptr g_logger = LOG_NAME("system");

bool Timer::Comparator::operator()(const Timer::ptr& lhs
        ,const Timer::ptr& rhs) const {
    if(!lhs && !rhs) {
        return false;
    }
    if(!lhs) {
        return true;
    }
    if(!rhs) {
        return false;
    }
    if(lhs->m_timeout < rhs->m_timeout) {
        return true;
    }
    if(rhs->m_timeout < lhs->m_timeout) {
        return false;
    }
    /// 到期时间点相同，比较地址
    return lhs.get() < rhs.get();
}

TimerManager::TimerManager()
{
    m_tfd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
    if(m_tfd == -1){
        LOG_FATAL(g_logger) << "timerfd_create() error: " << strerror(errno);
        exit(EXIT_FAILURE);
    }
}

TimerManager::~TimerManager()
{
    close(m_tfd);
}

Timer::ptr TimerManager::AddTimer(uint64_t interval, std::function<void()>&& cb, bool recurring)
{
    Timer::ptr newTimer = std::make_shared<Timer>(Timer(interval, cb, recurring));
    AddTimer(newTimer);
    return newTimer;
}

void TimerManager::AddTimer(Timer::ptr timer)
{
    WebServer::ScopedLock<WebServer::Mutex> lk(m_mtx);
    if(timer->GetTimeOut() < m_container.top()->GetTimeOut())
    {
        /// 新的最近要执行的任务，需要打断当前计时的sleep，设置新的计时时间

        /// 还是不用epoll了，这里就是只有一个timerfd需要监听，根本没必要使用io多路复用。自己控制就行了。
        /// 单独开一个线程，read timerfd， 如果时间没到，就会阻塞。如果时间到了，就返回。
        /// 析构时，settime重置为当前时间，马上唤醒，然后线程推出，join thread对象。
    }
    m_container.push(timer);
}

void TimerManager::DelTimer(Timer::ptr timer)
{
    WebServer::ScopedLock<WebServer::Mutex> lk(m_mtx);
    /// 由于堆只能删除栈顶元素, 所以这里使用延迟删除, 只有该节点到达堆顶才会被删除。
    timer->Cancel();
    /**
     *    需要检测是否是顶部元素取消？以此打断计时，重新设置计时？
     *    完全没必要，计时到期被唤醒时判断一下timer是否被取消了就行了。从效率上看，都是唤醒，再睡眠
     */
}
