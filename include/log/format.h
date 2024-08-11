#ifndef LOG_FORMAT_H
#define LOG_FORMAT_H

#include <string>
#include <vector>
#include <memory>

#include "log/level.h"
#include "log/event.h"

/**
 * @brief 日志格式化内容项(基类)
 */
class LogFormatItem
{
public:
    typedef std::shared_ptr<LogFormatItem> ptr;

    LogFormatItem(const std::string& format = "")
        :m_format(format)
    {}

    virtual ~LogFormatItem() {}

    /**
     * @brief 格式化日志到流
     * @param[in, out] os 日志输出流
     * @param[in] logger 日志器
     * @param[in] level 日志等级
     * @param[in] event 日志事件
     */
    virtual void format(std::ostream &os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) = 0;

protected:
    std::string     m_format;/// 日志项的格式(目前只有日期时间项有格式)
};

/**
 * @brief 消息内容项
 */
class MessageFormatItem : public LogFormatItem
{
public:
    MessageFormatItem(const std::string& format = "")
        : LogFormatItem(format)
    {}

    void format(std::ostream &os, std::shared_ptr<Logger> logger __attribute__((unused)), LogLevel::Level level __attribute__((unused)), LogEvent::ptr event) override
    {
        os << event->getContent();
    }
};

/**
 * @brief 日志等级项
 */
class LevelFormatItem : public LogFormatItem 
{
public:
    LevelFormatItem(const std::string& format = "")
        : LogFormatItem(format)
    {}

    void format(std::ostream& os, std::shared_ptr<Logger> logger __attribute__((unused)), LogLevel::Level level, LogEvent::ptr event __attribute__((unused))) override 
    {
        os << LogLevel::ToString(level);
    }
};

/**
 * @brief 线程id项
 */
class ThreadIdFormatItem : public LogFormatItem 
{
public:
    ThreadIdFormatItem(const std::string& format = "")
        : LogFormatItem(format)
    {}

    void format(std::ostream& os, std::shared_ptr<Logger> logger __attribute__((unused)), LogLevel::Level level __attribute__((unused)), LogEvent::ptr event) override 
    {
        os << event->getThreadID();
    }
};

/**
 * @brief 线程名
 */
class ThreadNameFormatItem : public LogFormatItem
{
public:
    ThreadNameFormatItem(const std::string& format = "")
        : LogFormatItem(format)
    {}

    void format(std::ostream& os, std::shared_ptr<Logger> logger __attribute__((unused)), LogLevel::Level level __attribute__((unused)), LogEvent::ptr event) override 
    {
        os << event->getThreadName();
    }
};

/**
 * @brief 换行符项
 */
class NewLineFormatItem : public LogFormatItem 
{
public:
    NewLineFormatItem(const std::string& format = "")
        : LogFormatItem(format)
    {}

    void format(std::ostream& os, std::shared_ptr<Logger> logger __attribute__((unused)), LogLevel::Level level __attribute__((unused)), LogEvent::ptr event __attribute__((unused))) override 
    {
        os << std::endl;
    }
};

/**
 * @brief 行号项
 */
class LineFormatItem : public LogFormatItem 
{
public:
    LineFormatItem(const std::string& format = "")
        : LogFormatItem(format)
    {}

    void format(std::ostream& os, std::shared_ptr<Logger> logger __attribute__((unused)), LogLevel::Level level __attribute__((unused)), LogEvent::ptr event) override 
    {
        os << event->getLine();
    }
};

/**
 * @brief 文件名项
 */
class FilenameFormatItem : public LogFormatItem 
{
public:
    FilenameFormatItem(const std::string& format = "")
        : LogFormatItem(format)
    {}

    void format(std::ostream& os, std::shared_ptr<Logger> logger __attribute__((unused)), LogLevel::Level level __attribute__((unused)), LogEvent::ptr event) override 
    {
        os << event->getFile();
    }
};

/**
 * @brief Tab键项
 */
class TabFormatItem : public LogFormatItem
{
public:
    TabFormatItem(const std::string& format = "")
        : LogFormatItem(format)
    {}

    void format(std::ostream& os, std::shared_ptr<Logger> logger __attribute__((unused)), LogLevel::Level level __attribute__((unused)), LogEvent::ptr event __attribute__((unused))) override 
    {
        os << '\t';
    }
};

/**
 * @brief 时间日期项，内部调用strftime设定子格式
 */
class DateTimeFormatItem : public LogFormatItem
{
public:
    DateTimeFormatItem(const std::string& format = "%Y-%m-%d %H:%M:%S")
        : LogFormatItem(format)
    {
        // 防止用户传进来的format为空
        if(m_format.empty())
            m_format = "%Y-%m-%d %H:%M:%S";
    }

    void format(std::ostream& os, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event) override;
};

/**
 * @brief 格式中不需要转义的项，原封不动，直接输出。
 */
class StringFormatItem : public LogFormatItem 
{
public:
    StringFormatItem(const std::string& str)
        :m_string(str)
    {}

    void format(std::ostream& os, std::shared_ptr<Logger> logger __attribute__((unused)), LogLevel::Level level __attribute__((unused)), LogEvent::ptr event __attribute__((unused))) override 
    {
        os << m_string;
    }

private:
    std::string m_string;
};

/**
 * @brief 将LogEvent内容格式化，转化成string
 */
class LogFormatter
{
public:
    typedef std::shared_ptr<LogFormatter> ptr;
    /**
     * @brief 构造函数
     * @param[in] pattern 日志格式模板,可以设置的项如下
     * @details 
     *  %m 消息内容
     *  %p 日志级别
     *  %t 线程id
     *  %n 换行
     *  %d 日期时间,后面加{}指定子格式，strftime的格式
     *  %f 文件名
     *  %l 行号
     *  %T 制表符
     *  %N 线程名称
     */
    LogFormatter(const std::string& pattern);

    std::string format(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event);
    std::ostream& format(std::ostream& ofs, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event);

    /**
     * @brief 初始化，解析日志模板（重点）
     */
    void init();

    /**
     * @brief 返回日志格式
     */
    const std::string& getPattern() const
    {
        return m_pattern;
    }

    /**
     * @brief 是否有错误
     */
    bool isError() const
    {
        return m_error;
    }


private:
    // 日志格式模板
    std::string                     m_pattern;
    std::vector<LogFormatItem::ptr> m_items;
    bool                            m_error = false;
};

#endif // LOG_FORMAT_H
