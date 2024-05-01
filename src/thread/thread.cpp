#include "thread/thread.h"

#include <cstring>

#include "errmsg/my_errno.h"
#include "log/log.h"
#include "util/util.h"

namespace WebServer
{
    static Logger::ptr g_logger = LOG_NAME("system");
    // 这里使用C++标准库提供的thread_local，没有使用POSIX的接口，还是有点奇怪.... 以后再改吧
    thread_local Thread* Thread::this_thread = nullptr;

    Thread::Thread(std::function<void()> thread_func, std::string thread_name)
        :m_thread(0), m_tid(-1), m_name(thread_name), m_cb(thread_func)
    {
        int ret = pthread_create(&m_thread, NULL, &Thread::start_routine, this);
        if(ret != 0)
        {
            LOG_ERROR(g_logger) << "pthread_create execute failed! , return " << ret << ", thread name: " << thread_name
                << ". possible reason: " << my_strerror(ret);
            throw std::logic_error("pthread_create execute failed!");
        }

        m_semaphore.wait();
    }

    Thread::~Thread()
    {
        if(m_thread)
        {
            pthread_detach(m_thread);
        }
    }

    void* Thread::start_routine(void* arg)
    {
        Thread* threadObj = (Thread*)arg;
        // 初始化所有thread_local变量
        Thread::this_thread = threadObj;

        threadObj->m_tid = util::getThreadID();
        // 设置线程名，便于调试。只能用pthread_self，因为可能pthread_create正在执行，m_thread是无效的。
        pthread_setname_np(pthread_self(), threadObj->m_name.substr(0, 15).c_str());

        std::function<void()> cb;
        cb.swap(threadObj->m_cb);

        threadObj->m_semaphore.notify();
        cb();
        return nullptr;
    }

    void Thread::join()
    {
        if(m_thread)
        {
            int ret = pthread_join(m_thread, NULL);
            if(ret)
            {
                LOG_ERROR(g_logger) << "pthread_join failed! return " << ret << ", thread name: " << m_name
                    << ". Possible reason: " << my_strerror(ret);
                throw std::logic_error("pthread_join error!");
            }
            m_thread = 0;
        }
    }
}


