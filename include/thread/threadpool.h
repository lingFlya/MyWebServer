/**
 *@author 2mu
 *@date 2022/5/10
 *@brief 线程池重构,固定线程数的线程池
 */

#ifndef WEB_SERVER_THREAD_POOL_H
#define WEB_SERVER_THREAD_POOL_H

#include <queue>
#include <functional>
#include <future>
#include <stdexcept>

#include <boost/noncopyable.hpp>

#include "thread/thread.h"
#include "thread/semaphore.h"
#include "thread/mutex.h"

class ThreadPool : boost::noncopyable
{
public:
    explicit ThreadPool(int thread_count = 4);
    ~ThreadPool();

    template<typename Func, typename... Args>
    auto addTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))>
    {
        /// 构建一个std::packaged_task(异步任务)
        auto taskPtr = std::make_shared<std::packaged_task<decltype(func(args...))()>>(
            std::bind(std::forward<Func>(func), std::forward<Args>(args)...)
        );

        {
            WebServer::ScopedLock<WebServer::Mutex> lk(m_mtx);
            if(m_isStop)
                throw std::logic_error("thread pool stopping! push task failed!");
            /**
             * @todo 这里为啥再封装一个std::function对象, 不如直接把 taskPtr 保存到 队列中
             * 存不了, std::packaged_task<decltype(func(args...) 是可变类型, 没办法保存到queue中...
             */
            m_taskQueue.emplace([taskPtr](){(*taskPtr)();});
        }
        m_semaphore.notify();
        return taskPtr->get_future();
    }

    int getThreadCount() const
    {
        return m_threadCount;
    }

private:
    bool                                    m_isStop;
    int                                     m_threadCount;  // 线程数目
    std::queue<std::function<void()>>       m_taskQueue;    // 任务队列
    std::vector<WebServer::Thread::ptr>     m_vctThreads;

    WebServer::Semaphore                    m_semaphore;
    WebServer::Mutex                        m_mtx;          // 保证任务队列的线程安全
};

#endif //WEB_SERVER_THREAD_POOL_H
