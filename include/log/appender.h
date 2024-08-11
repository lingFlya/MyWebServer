#ifndef LOG_APPENDER_H
#define LOG_APPENDER_H

#include <string>
#include <memory>
#include <fstream>

#include "log/level.h"
#include "log/format.h"
#include "thread/mutex.h"

/**
 * @brief 日志输出地址(抽象类)
 */
class LogAppender
{
public:
    typedef std::shared_ptr<LogAppender> ptr;
    explicit LogAppender(LogLevel::Level level = LogLevel::Level::DEBUG)
        :m_level(level)
    {}

    virtual ~LogAppender()=default;

    /**
     * @brief 纯虚函数, 往指定地址写日志
     * @param logger    日志器
     * @param level     这条日志的级别
     * @param logStr    格式化后的日志
     */
    virtual void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0;

    /**
     * @brief 设置appender接受的日志级别
     */
    void setLevel(LogLevel::Level level)
    {
        WebServer::ScopedLock<WebServer::Mutex> lock(m_mutex);
        m_level = level;
    }

    /**
     * @brief 获取日志appender日志级别
     */
    LogLevel::Level getLevel()
    {
        WebServer::ScopedLock<WebServer::Mutex> lock(m_mutex);
        return m_level;
    }

    LogFormatter::ptr getFormatter();

    /**
     * @brief 设置新格式器
     * @param newFormatter 新格式器
     */
    void setFormatter(LogFormatter::ptr newFormatter);

protected:
    // 日志级别, 若传入的log事件级别小于该级别; 则appender不记录该事件
    LogLevel::Level         m_level;
    // 格式化器
    LogFormatter::ptr       m_formatter;
    // 互斥锁, 保证本对象的线程安全; 上层logger写日志会加锁, formatter没有线程安全问题;
    // 但Appender对象可能被多个logger在不同线程使用, 如不同logger的日志写入到同一文件, 需要保证安全;
    WebServer::Mutex        m_mutex;
};

/**
 * @brief 输出到控制台
 */
class StdOutLogAppender : public LogAppender
{
public:
    typedef std::shared_ptr<StdOutLogAppender> ptr;

    /**
     * @brief           构造函数
     * @param logger    日志器
     * @param level     指定父类m_level的值
     */

    void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override;
};

/**
 * @brief 输出到指定文件
 */
class FileLogAppender : public LogAppender
{
public:
    typedef std::shared_ptr<FileLogAppender> ptr;

    /**
     * @brief 构造函数
     * @param fileName  输出文件
     */
    explicit FileLogAppender(const std::string& fileName);
    virtual ~FileLogAppender();

    /**
     * @brief 重新打开日志文件
     * @return 成功返回true
     */
    bool reopen();

    void log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override;
private:
    std::string     m_fileName;        // 文件路径
    std::ofstream   m_fileStream;      // 文件流
    uint64_t        m_lastTime = 0;    // 上次打开时间
};

#endif // LOG_APPENDER_H
