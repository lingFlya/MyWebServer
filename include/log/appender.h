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
     * 
     */
    void setLevel(LogLevel::Level level)
    {
        m_level = level;
    }

    /**
     * @brief 获取日志appender日志级别
     */
    LogLevel::Level getLevel() const
    {
        return m_level;
    }

    LogFormatter::ptr getFormatter();

    /**
     * @brief 设置新格式器
     * @param newFormatter 新格式器
     */
    void setFormatter(LogFormatter::ptr newFormatter);

protected:
    LogLevel::Level         m_level;          // 默认日志级别，小于该级别，该appender不记录。
    LogFormatter::ptr       m_formatter;      // 日志格式器, 默认为NULL
    // std::shared_ptr的不同副本在不同线程操作是安全的, 在多个线程操作同一std::shared_ptr实例, 且有一个线程是执行std::shared_ptr的非const成员函数, 则不安全;
    WebServer::Mutex        m_formatterMtx;   // 互斥锁, 防止多个线程操作this->m_formatter
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
    /**
     * @todo 多线程操作 m_fileStream 也是线程不安全的, 需要加锁; sylar加了, 但是和m_formatter共用一把锁; 我单独给它加把锁吧
     */
    std::ofstream   m_fileStream;      // 文件流
    uint64_t        m_lastTime = 0;    // 上次打开时间
};

#endif // LOG_APPENDER_H
