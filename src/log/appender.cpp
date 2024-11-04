#include "log/appender.h"

#include <iostream>
#include <unistd.h>
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
    if (level < m_level)
        return;

    ScopedLock<WebServer::Mutex> lock(m_mutex);
    std::string format_result = m_formatter->format(logger, level, event);
    // std::cout可能不是异步信号安全的(因为stdio函数都不是), 所以使用异步信号安全的write系统调用写入到标准输出
    write(fileno(stdout), format_result.c_str(), format_result.length());
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
        std::string dir_path = util::getDir(m_fileName);
        if (!util::createDir(dir_path))
            std::cout << "create " << dir_path << " dir failed!" << std::endl;
        m_fileStream.open(m_fileName, std::ios::app);
    }
    return m_fileStream.is_open();
}

void FileLogAppender::log(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event)
{
    if(level >= m_level) {
        uint64_t now = event->getTime();
        // 超过3秒就会刷新一次文件缓存
        ScopedLock<WebServer::Mutex> lock(m_mutex);
        if(now >= (m_lastTime + 3)) {
            m_fileStream << std::flush;
            m_lastTime = now;
        }
        if(!m_formatter->format(m_fileStream, logger, level, event)) {
            std::cout << "FileLogAppender::log error!" << std::endl;
        }
    }
}
