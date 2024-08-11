#include "log/appender.h"

#include <iostream>

#include "util/util.h"

using WebServer::ScopedLock;

LogFormatter::ptr LogAppender::getFormatter()
{
    ScopedLock<WebServer::Mutex> lock(m_mutex);
    return m_formatter;
}

void LogAppender::setFormatter(LogFormatter::ptr newFormatter)
{
    ScopedLock<WebServer::Mutex> lock(m_mutex);
    m_formatter = newFormatter;
}

void StdOutLogAppender::log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event)
{
    if(level >= m_level)
    {
        ScopedLock<WebServer::Mutex> lock(m_mutex);
        m_formatter->format(std::cout, logger, level, event);
    }
}

FileLogAppender::FileLogAppender(const std::string &fileName)
    :m_fileName(fileName)
{
    reopen();
}

FileLogAppender::~FileLogAppender()
{
    if(m_fileStream.is_open())
        m_fileStream.close();
}

bool FileLogAppender::reopen()
{
    ScopedLock<WebServer::Mutex> lock(m_mutex);
    if(m_fileStream)
        m_fileStream.close();
    m_fileStream.open(m_fileName, std::ios::app);
    if(!m_fileStream.is_open())
    {
        // 可能是目录不存在,所以无法创建文件打开, 创建目录试一次
        std::string dir = util::getDir(m_fileName);
        if(!util::createDir(dir))
            std::cout << "create dir failed! dirname: " << dir << std::endl;
        else
            std::cout << "create dir success! dirname: " << dir << std::endl;
        // 再试一次
        m_fileStream.open(m_fileName, std::ios::app);
    }
    return m_fileStream.is_open();
}

void FileLogAppender::log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event)
{
    if(level >= m_level) {
        uint64_t now = event->getTime();
        // 超过3秒就会刷新一次文件缓存（关闭文件重新打开，会刷新文件缓存）
        if(now >= (m_lastTime + 3)) {
            reopen();
            m_lastTime = now;
        }
        ScopedLock<WebServer::Mutex> lock(m_mutex);
        if(!m_formatter->format(m_fileStream, logger, level, event)) {
            std::cout << "FileLogAppender::log error!" << std::endl;
        }
    }
}
