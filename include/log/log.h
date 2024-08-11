#ifndef WEB_SERVER_LOG_H
#define WEB_SERVER_LOG_H

#include <string>
#include <cstdint>
#include <memory>
#include <list>
#include <map>
#include <utility>

#include "log/level.h"
#include "log/event.h"
#include "log/appender.h"
#include "util/singleton.h"
#include "util/util.h"
#include "thread/mutex.h"
#include "thread/thread.h"

class LogEvent;
class LogEventWrap;


/**
 * @brief 日志器
 */
class Logger : public std::enable_shared_from_this<Logger>
{
friend class LoggerManager;

public:
    typedef std::shared_ptr<Logger> ptr;

    Logger(const std::string& name)
        : m_level(LogLevel::DEBUG),
        m_root(nullptr),
        m_formatter(std::make_shared<LogFormatter>("%d{%Y-%m-%d %H:%M:%S}%T%t%T%N%T[%p]%T%f:%l%T%m%n")),
        m_name(name)
    {}

    /**
     * @brief 增加观察者
     * @param appender 指定日志输出地
     */
    void addAppender(LogAppender::ptr appender);
    
    /**
     * @brief 减少观察者
     * @param appender 要删除的观察者
     */
    void delAppender(LogAppender::ptr appender);

    /**
     * @brief 清空日志所有appender
     */
    void clearAppender();

    // 通过相应函数输出日志.

    void debug(LogEvent::ptr event);
    void info(LogEvent::ptr event);
    void warn(LogEvent::ptr event);
    void error(LogEvent::ptr event);
    void fatal(LogEvent::ptr event);
    
    /**
     * @brief 返回日志器名称
     */
    const std::string& getName() const { return m_name;}

    /**
     * @brief 返回日志级别
     */
    LogLevel::Level getLevel() const { return m_level;}

    /**
     * @brief 设置日志级别
     */
    void setLevel(LogLevel::Level val) { m_level = val;}

    /**
     * @brief 设置日志格式器, 会将m_listAppender的所有appender都重置;
     * @param formatter 新日志格式器
     */
    void setFormatter(LogFormatter::ptr formatter);

    /**
     * @brief 设置日志格式模板, 会将m_listAppender的所有appender都重置;
     * @param format_str 新日志格式
     */
    void setFormatter(const std::string& format_str);

    /**
     * @brief 获取日志格式器
     */
    LogFormatter::ptr getFormatter()
    {
        WebServer::ScopedLock<WebServer::Mutex> lock(m_mtx);
        return m_formatter;
    }

    /**
     * @brief 写日志
     */
    void log(LogLevel::Level level, LogEvent::ptr event);

private:
    LogLevel::Level                 m_level;
    // root日志器, LoggerManager在管理时会赋初值
    Logger::ptr                     m_root;
    LogFormatter::ptr               m_formatter;
    WebServer::Mutex                m_mtx;
    std::string                     m_name;
    std::list<LogAppender::ptr>     m_listAppender;
};


class LoggerManager
{
public:
    LoggerManager();

    /**
     * @brief 根据name获取对应的Logger
     * @param name Logger的名字
     */
    Logger::ptr getLogger(const std::string& name);

    /**
     * @brief 返回主日志器
     */
    Logger::ptr getRoot() { return m_root;}

private:
    WebServer::Mutex                    m_mtx;
    std::map<std::string, Logger::ptr>  m_loggers; /// 日志器映射
    Logger::ptr                         m_root;    /// 主日志器
};

/// 日志器管理器单例类型
typedef Singleton<LoggerManager> LoggerMgr;

/**
 * @brief 获取主日志器
 */
#define LOG_ROOT() LoggerMgr::getInstance().getRoot()

/**
 * @brief 获取指定name的日志器
 */
#define LOG_NAME(name) LoggerMgr::getInstance().getLogger(name)

/**
 * @brief Event事件包装器，就是把LogEvent和Logger包装在一起。
 */
class LogEventWrap
{
public:

    /**
     * @brief 构造函数
     * @param event 日志事件
     */
    explicit LogEventWrap(LogEvent::ptr event)
        :m_event(std::move(event))
    {}

    ~LogEventWrap()
    {
        m_event->getLogger()->log(m_event->getLevel(), m_event);
    }

    /**
     * @brief 获取LogEvent对象，日志事件
     */
    LogEvent::ptr getEvent()
    {
        return m_event;
    }

    /**
     * @brief 获取日志内容流
     */
    std::stringstream& getSS()
    {
        return m_event->getSS();
    }

private:
    LogEvent::ptr m_event;
};

/**
 * @brief 使用流方式将日志按照指定level写入到logger
 */
#define LOG_LEVEL(logger, level)                                                    \
    if((logger)->getLevel() <= (level))                                             \
        LogEventWrap(std::make_shared<LogEvent>(logger, level, __FILE__, __LINE__,  \
            util::getThreadID(), time(NULL), WebServer::Thread::GetName())).getSS()

/**
 * @brief 使用流式方式将日志级别debug的日志写入到logger
 */
#define LOG_DEBUG(logger) LOG_LEVEL(logger, LogLevel::DEBUG)

/**
 * @brief 使用流式方式将日志级别info的日志写入到logger
 */
#define LOG_INFO(logger) LOG_LEVEL(logger, LogLevel::INFO)

/**
 * @brief 使用流式方式将日志级别warn的日志写入到logger
 */
#define LOG_WARN(logger) LOG_LEVEL(logger, LogLevel::WARN)

/**
 * @brief 使用流式方式将日志级别error的日志写入到logger
 */
#define LOG_ERROR(logger) LOG_LEVEL(logger, LogLevel::ERROR)

/**
 * @brief 使用流式方式将日志级别fatal的日志写入到logger
 */
#define LOG_FATAL(logger) LOG_LEVEL(logger, LogLevel::FATAL)

/**
 * @brief 使用格式化方式将日志级别level的日志写入到logger
 */
#define LOG_FMT_LEVEL(logger, level, fmt, ...)                                      \
    if(logger->getLevel() <= level)                                                 \
        LogEventWrap(std::make_shared<LogEvent>(logger, level, __FILE__, __LINE__,  \
            util::getThreadID(), time(NULL), WebServer::Thread::GetName())).getEvent()->format(fmt, __VA_ARGS__)

/**
 * @brief 使用格式化方式将日志级别debug的日志写入到logger
 */
#define LOG_FMT_DEBUG(logger, fmt, ...) LOG_FMT_LEVEL(logger, LogLevel::DEBUG, fmt, __VA_ARGS__)

/**
 * @brief 使用格式化方式将日志级别info的日志写入到logger
 */
#define LOG_FMT_INFO(logger, fmt, ...)  LOG_FMT_LEVEL(logger, LogLevel::INFO, fmt, __VA_ARGS__)

/**
 * @brief 使用格式化方式将日志级别warn的日志写入到logger
 */
#define LOG_FMT_WARN(logger, fmt, ...)  LOG_FMT_LEVEL(logger, LogLevel::WARN, fmt, __VA_ARGS__)

/**
 * @brief 使用格式化方式将日志级别error的日志写入到logger
 */
#define LOG_FMT_ERROR(logger, fmt, ...) LOG_FMT_LEVEL(logger, LogLevel::ERROR, fmt, __VA_ARGS__)

/**
 * @brief 使用格式化方式将日志级别fatal的日志写入到logger
 */
#define LOG_FMT_FATAL(logger, fmt, ...) LOG_FMT_LEVEL(logger, LogLevel::FATAL, fmt, __VA_ARGS__)


/**
 * @brief 初始化log模块
 */
void log_init();

#endif //WEB_SERVER_LOG_H
