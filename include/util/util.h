/**
 * @author  2mu
 * @date    2021/5/23
 * @brief   工具，一些函数,类
 */

#ifndef WEB_SERVER_UTIL_H
#define WEB_SERVER_UTIL_H

#include <string>
#include <cstdint>
#include <cxxabi.h>
#include <unistd.h>

namespace util
{
    /**
     * @brief 设置sig信号的处理方式为忽视
     * @param sig 指定的信号
     * @return 0表示成功, -1表示失败;
     */
    int set_signal_ignore(int sig);

    /**
     * @brief 根据指定端口开启监听
     * @param port 指定端口
     * @return 成功返回listening_fd, 失败返回-1
     */
    int socket_bind_listen(short port);

    /**
     * @brief 设置指定fd为非阻塞模式
     * @param fd 指定fd
     * @return 成功返回0, 失败返回-1
     */
    int set_nonblock(int fd);

    /**
     * @brief 多次调用read，读size个字节，直到对端关闭或者出现错误
     */
    int readn(int fd, char *buf, size_t size);

    /**
     * @brief 多次调用write，写size个字节，直到对端关闭或者出现错误。
     */
    int writen(int fd, const char *buf, size_t size);

    /**
     * @brief 根据文件后最得到文件类型名
     * @param suffix 文件后缀形式
     * @return 文件类型
     */
    std::string getMimeType(const std::string &suffix);

    /**
     * @brief 获得当前时间(系统时间, 毫秒级)
     */
    std::uint64_t get_real_time();

    /**
     * @brief 获取当前时间(系统时间, 纳秒级)
     */
    std::uint64_t get_real_time_nsec();

    /**
     * @brief 将类型名转化为字符串(typeinfo)
     * @tparam T
     * @return char* 类型名字符串
     */
    template<typename T>
    const char *typeToName()
    {
        static const char *name = abi::__cxa_demangle(typeid(T).name(), nullptr,
                                                      nullptr, nullptr);
        return name;
    }

    /**
     * @brief   返回当前执行线程id
     */
    pid_t getThreadID();

    /**
     * @brief 从filePath中抽离, 目录路径出来
     */
    std::string getDir(const std::string& filePath);

    /**
     * @brief 创建目录
     * @param dirname 目录名
     * @return 成功true, 失败false
     */
    bool createDir(const std::string& dirname);
}

#endif //WEB_SERVER_UTIL_H
