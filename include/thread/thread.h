/**
 * @author  2mu
 * @date    2022/5/11
 * @brief   thread对象
 * 实际上GetID的实现可以使用gettid代替（util模块中以实现）, GetName的实现可以使用pthread_getname_np代替。
 * 但这里既然因为要提供GetThis接口，而保存了this_thread线程指针。所以直接读取this_thread返回，减少系统调用次数更好。
 */

#ifndef WEB_SERVER_THREAD_H
#define WEB_SERVER_THREAD_H

#include <memory>
#include <functional>
#include <string>

#include <pthread.h>
#include <boost/noncopyable.hpp>

#include "thread/semaphore.h"

namespace WebServer
{

    class Thread : boost::noncopyable
    {
    public:
        using ptr = std::shared_ptr<Thread>;

        /**
         * @brief 创建一个线程
         * @param thread_func 线程执行的函数
         * @param thread_name 线程名, 最多16字符, 多余16字符也只取16字符
         */
        Thread(std::function<void()> thread_func, std::string thread_name = "unknown");
        ~Thread();

        void join();

        /**
         * @brief 返回线程对象线程ID
         */
        pid_t getID()const
        {
            return m_tid;
        }

        /**
         * @brief 返回线程对象线程名
         */
        const std::string& getName()const
        {
            return m_name;
        }

        /**
         * @brief 当前执行线程指针
         */
        static Thread* GetThis()
        {
            return this_thread;
        }

        /**
         * @brief 返回执行线程(本线程自身)ID,
         * @return pid_t 非WebServer::Thread线程调用, 返回0; 其余返回线程ID
         */
        static pid_t GetID()
        {
            if(this_thread == nullptr)
                return 0;
            return this_thread->m_tid;
        }

        /**
         * @brief 返回执行线程(本线程自身)名字
         * @return string 非WebServer::Thread线程调用,返回"main", 其余返回线程名.
         */
        static const std::string& GetName()
        {
            if(this_thread == nullptr)
            {
                // 只要是用WebServer::Thread构造函数创建的线程, this_thread不为NULL, 一定有自己的名字; 否则就可能是main主线程
                static std::string name = "main";
                return name;
            }
            return this_thread->m_name;
        }

    private:
        static void* start_routine(void*);

    private:
        static thread_local Thread*     this_thread;// 当前线程, 在非WebServer线程中, 这个指针是NULL

        pthread_t                       m_thread;   // posix线程标识(id)，只保证在同一个进程中唯一
        pid_t                           m_tid;      // 操作系统线程标识id

        WebServer::Semaphore            m_semaphore;
        std::string                     m_name;     // 线程名字
        std::function<void()>           m_cb;
    };

}

#endif //WEB_SERVER_THREAD_H
