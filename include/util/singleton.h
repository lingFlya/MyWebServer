/**
 * @author  2mu
 * @date    2022/5/22
 * @brief   实现的单例模板类, 需要实现单例的class直接传参实例化该模板即可
*/

#ifndef WEB_SERVER_SINGLETON_H
#define WEB_SERVER_SINGLETON_H

#include <boost/noncopyable.hpp>
#include <pthread.h>


/**
 * @brief 直接返回指定class的单例, 缺点是该class必须支持默认构造函数;
 * @tparam T 要使用单例模式的class
 */
template<class T>
class Singleton : boost::noncopyable
{
public:
    /**
     * @brief 返回唯一单例的引用，为什么不是指针？防止误操作对指针delete。
     */
    static T& getInstance()
    {
        pthread_once(&m_once, init);
        return *m_instance;
    }

private:
    Singleton() = default;
    /// 私有化析构函数，这样就只能在heap上创建实例
    ~Singleton() = default;

    static void init()
    {
        m_instance = new T();
    }

private:
    static T* m_instance;
    static pthread_once_t m_once;
};

// 类中静态变量类中申明，类外初始化

template<typename T>
T* Singleton<T>::m_instance = nullptr;

template<typename T>
pthread_once_t Singleton<T>::m_once = PTHREAD_ONCE_INIT;

#endif // WEB_SERVER_SINGLETON_H
