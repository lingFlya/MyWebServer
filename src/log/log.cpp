#include "log/log.h"

using WebServer::ScopedLock;

void Logger::addAppender(LogAppender::ptr appender)
{
    ScopedLock<WebServer::Mutex> lk(m_mtx);
    // 如果该appender的formatter为空(Appender没有指定自己的格式), 那就使用logger的formatter
    if(!appender->getFormatter())
        appender->setFormatter(m_formatter);
    m_listAppender.push_back(appender);
}

void Logger::delAppender(LogAppender::ptr appender)
{
    ScopedLock<WebServer::Mutex> lk(m_mtx);
    for(auto it = m_listAppender.begin(); it != m_listAppender.end(); ++it)
    {
        if(*it == appender)
        {
            m_listAppender.erase(it);
            break;
        }
    }
}

void Logger::clearAppender()
{
    ScopedLock<WebServer::Mutex> lk(m_mtx);
    m_listAppender.clear();
}

/// 子函数
void Logger::log(LogLevel::Level level, LogEvent::ptr event)
{
    // 将要打印的内容格式化
    if(level >= m_level)
    {
        /// this智能指针
        auto self = shared_from_this();
        ScopedLock<WebServer::Mutex> lk(m_mtx);
        if(!m_listAppender.empty()) {
            for(auto& item : m_listAppender) {
                item->log(self, level, event);
            }
        } else if(m_root) {
            m_root->log(level, event);
        }
    }
}

void Logger::debug(LogEvent::ptr event)
{
    log(LogLevel::DEBUG, event);
}
void Logger::info(LogEvent::ptr event)
{
    log(LogLevel::INFO, event);
}
void Logger::warn(LogEvent::ptr event)
{
    log(LogLevel::WARN, event);
}
void Logger::error(LogEvent::ptr event)
{
    log(LogLevel::ERROR, event);
}
void Logger::fatal(LogEvent::ptr event)
{
    log(LogLevel::FATAL, event);
}

LoggerManager::LoggerManager()
    :m_root(nullptr)
{
    // 默认先创建一个root日志器, 将日志输出到stdout
    m_root = std::make_shared<Logger>("root");
    LogAppender::ptr appender = std::make_shared<StdOutLogAppender>();
    m_root->addAppender(appender);

    // 维护map映射
    m_loggers[m_root->m_name] = m_root;
}

Logger::ptr LoggerManager::getLogger(const std::string& name)
{
    ScopedLock<WebServer::Mutex> lk(m_mtx);
    auto it = m_loggers.find(name);
    if(it != m_loggers.end())
        return it->second;

    // 指定name的Logger不存在，就创建一个并返回。
    Logger::ptr logger(new Logger(name));
    logger->m_root = m_root;
    m_loggers[name] = logger;
    return logger;
}


void log_init()
{
    Logger::ptr logger = LOG_ROOT();
    LogAppender::ptr appender = std::make_shared<FileLogAppender>("../log/server.log");
    logger->addAppender(appender);
}
