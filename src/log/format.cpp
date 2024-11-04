#include "log/format.h"

#include <map>
#include <functional>
#include <cctype>
#include <time.h>


void DateTimeFormatItem::format(std::ostream& os, std::shared_ptr<Logger> logger __attribute__((unused)),
    LogEvent::ptr event)
{
    time_t time = event->getTime();
    struct tm t;
    localtime_r(&time, &t);
    char buf[64];
    strftime(buf, sizeof(buf), m_format.c_str(), &t);
    os << buf;
}


LogFormatter::LogFormatter(const std::string& pattern)
    : m_error(false), m_level(LogLevel::Level::DEBUG)
    , m_pattern(pattern)
{
    init();
}

std::string LogFormatter::format(std::shared_ptr<Logger> logger,
    LogLevel::Level level,LogEvent::ptr event)
{
    if (level < m_level)
        return "";

    std::stringstream ss;
    for(auto& item : m_items)
    {
        item->setLogLevel(level);
        item->format(ss, logger, event);
    }
    return ss.str();
}

std::ostream& LogFormatter::format(std::ostream& ofs,
    std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event)
{
    if (level < m_level)
        return ofs;
    
    for(auto& item : m_items)
    {
        item->setLogLevel(level);
        item->format(ofs, logger, event);
    }
    return ofs;
}

std::string LogFormatter::_get_item_format(size_t pos) const
{
    std::string format;
    if (pos > m_pattern.size())
        return format;
    if (m_pattern[pos] == '{')
    {
        size_t end_pos = pos + 1;
        while(end_pos < m_pattern.size() && m_pattern[end_pos] != '}')
        {
            ++end_pos;
        }
        if(end_pos < m_pattern.size() && m_pattern[end_pos] == '}')
            format = m_pattern.substr(pos + 1, end_pos - pos - 1);
    }
    // 要么返回空串, 要么返回不带{}的项格式
    return format;
}

void LogFormatter::init()
{
    // 字符串映射到创建对应FormatterItem对象的lambda; 如果是全局变量, 全局变量还需要考虑初始化顺序...
    #define init_map(ch, cls) \
        {ch, [](const std::string& fmt) { return LogFormatItem::ptr(new cls(fmt));}}
            
    static std::map<char, std::function<LogFormatItem::ptr(const std::string& fmt)> > g_format_items 
        = {
            init_map('m', MessageFormatItem),           //m:消息
            init_map('p', LevelFormatItem),             //p:日志级别
            init_map('t', ThreadIdFormatItem),          //t:线程id
            init_map('n', NewLineFormatItem),           //n:换行
            init_map('d', DateTimeFormatItem),          //d:日期时间
            init_map('f', FilenameFormatItem),          //f:文件名
            init_map('l', LineFormatItem),              //l:行号
            init_map('T', TabFormatItem),               //T:Tab
            init_map('N', ThreadNameFormatItem),        //N:线程名称
    };
    #undef init_map


    std::string item_string;
    for(size_t i = 0; i < m_pattern.size(); ++i)
    {
        const char ch = m_pattern[i];
        if (ch == '%')
        {
            if(!item_string.empty())
            {
                m_items.push_back(std::make_shared<StringFormatItem>(item_string));
                item_string.clear();
            }

            if (i + 1 < m_pattern.size())
            {
                const char next_ch = m_pattern[i + 1];
                if(std::isalpha(next_ch))
                {
                    // %后面接一个字母, 表示是合法格式; 此时继续判断该项是否有 子格式
                    std::string item_format = this->_get_item_format(i + 2);
                    if(!item_format.empty())
                        i = i + item_format.length() + 2;// i跳过项的format

                    auto iter = g_format_items.find(next_ch);
                    if(iter != g_format_items.end())
                    {
                        m_items.push_back(iter->second(item_format));
                    }
                    else
                    {
                        // 错误格式
                        std::string error_info = " ->unknown format: %";
                        error_info.push_back(next_ch);
                        error_info += item_format;
                        error_info += "<-";
                        m_items.push_back(std::make_shared<StringFormatItem>(error_info));
                        m_error = true;
                    }
                }
                else if(next_ch == '%')
                {
                    // 两个连续的 %% 转义为 %
                    item_string.push_back('%');
                }
                else
                {
                    // 错误格式, %后字母不支持
                    std::string error_info = " ->error format: %";
                    error_info.push_back(next_ch);
                    error_info += "<-";
                    m_items.push_back(std::make_shared<StringFormatItem>(error_info));
                    m_error = true;
                }

                // %后面的字母已读取过, 跳过下次循环;
                ++i;
            }
            else
            {
                // 错误format, %后面未接具体项
                std::string error_info = " ->error format: % <-";
                m_items.push_back(std::make_shared<StringFormatItem>(error_info));
                m_error = true;
            }
        }
        else
        {
            item_string.push_back(ch);
        }
    }
    if (!item_string.empty())
        m_items.push_back(std::make_shared<StringFormatItem>(item_string));
}
