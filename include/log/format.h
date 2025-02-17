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
        :m_format(format), m_level(LogLevel::Level::DEBUG)
    {}

    virtual ~LogFormatItem() {}

    LogLevel::Level getLogLevel()const
    {
        return m_level;
    }

    void setLogLevel(LogLevel::Level log_level)
    {
        m_level = log_level;
    }

    /**
     * @brief 格式化该项内容到日志中
     * @param[in, out] os 日志输出流
     * @param[in] logger 日志器
     * @param[in] event 日志事件
     */
    virtual void format(std::ostream &os, std::shared_ptr<Logger> logger, 
        LogEvent::ptr event) = 0;


protected:
    // 日志项的格式(子格式, 目前只有日期时间项有格式)
    std::string     m_format;
    LogLevel::Level m_level;
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

    void format(std::ostream &os, std::shared_ptr<Logger> logger __attribute__((unused)),
        LogEvent::ptr event) override
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
    /**
     * @brief: 和其它子类不同, 日志等级须要知道日志等级, 因为它要将其转化为字符串
     */
    LevelFormatItem(const std::string& format = "")
        : LogFormatItem(format)
    {}

    void format(std::ostream& os, std::shared_ptr<Logger> logger __attribute__((unused)),
        LogEvent::ptr event __attribute__((unused))) override 
    {
        os << LogLevel::ToString(m_level);
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

    void format(std::ostream& os, std::shared_ptr<Logger> logger __attribute__((unused)),
        LogEvent::ptr event) override 
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

    void format(std::ostream& os, std::shared_ptr<Logger> logger __attribute__((unused)), 
        LogEvent::ptr event) override 
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

    void format(std::ostream& os, std::shared_ptr<Logger> logger __attribute__((unused)), 
        LogEvent::ptr event __attribute__((unused))) override 
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

    void format(std::ostream& os, std::shared_ptr<Logger> logger __attribute__((unused)), 
        LogEvent::ptr event) override 
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

    void format(std::ostream& os, std::shared_ptr<Logger> logger __attribute__((unused)),
        LogEvent::ptr event) override 
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

    void format(std::ostream& os, std::shared_ptr<Logger> logger __attribute__((unused)),
        LogEvent::ptr event __attribute__((unused))) override 
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

    void format(std::ostream& os, std::shared_ptr<Logger> logger,
        LogEvent::ptr event) override;
};


/**
 * @brief 格式中不需要转义的项, 直接原样输出
 */
class StringFormatItem : public LogFormatItem 
{
public:
    StringFormatItem(const std::string& str)
        :m_string(str)
    {}

    void format(std::ostream& os, std::shared_ptr<Logger> logger __attribute__((unused)),
        LogEvent::ptr event __attribute__((unused))) override 
    {
        os << m_string;
    }

private:
    std::string m_string;
};


/**
 * @brief 格式化器, 将LogEvent内容格式化, 转化成string或者直接输入到ostream对象中
 */
class LogFormatter
{
public:
    typedef std::shared_ptr<LogFormatter> ptr;
    /**
     * @brief LogFormatter构造函数
     * @param[in] pattern 日志格式模板, 可以设置的项如下
     * @details 
     *  %m 消息内容
     *  %p 日志级别
     *  %t 线程id
     *  %n 换行
     *  %d 日期时间, 后面加{}指定子格式, 也就是strftime的格式
     *  %f 文件名
     *  %l 行号
     *  %T 制表符
     *  %N 线程名称
     */
    LogFormatter(const std::string& pattern);

    std::string format(std::shared_ptr<Logger> logger, LogLevel::Level level,
            LogEvent::ptr event);
    std::ostream& format(std::ostream& ofs, std::shared_ptr<Logger> logger, 
            LogLevel::Level level, LogEvent::ptr event);

    /**
     * @brief 初始化, 即解析日志格式字符串m_pattern
     */
    void init();

    const std::string& getPattern() const
    {
        return m_pattern;
    }

    /**
     * @brief 解析是否有错误发生
     */
    bool isError() const
    {
        return m_error;
    }

    void setLevel(LogLevel::Level log_level)
    {
        m_level = log_level;
    }

    LogLevel::Level getLevel()const
    {
        return m_level;
    }


private:
    /**
     * @brief 从m_pattern指定位置开始, 获取{}子格式项
     * @param pos 开始搜索子格式位置
     * @return sub_format 搜索到的子格式, 若没有, 则返回空字符串;
     */
    std::string _get_item_format(size_t pos) const;
    

    bool                            m_error;
    LogLevel::Level                 m_level;    // formatter对象也允许设置日志等级
    std::string                     m_pattern;  // 日志格式模板
    std::vector<LogFormatItem::ptr> m_items;    // 具体的日志格式项
};

#endif // LOG_FORMAT_H
