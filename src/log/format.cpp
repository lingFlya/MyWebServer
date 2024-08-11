#include "log/format.h"

#include <iostream>
#include <sstream>
#include <map>
#include <functional>
#include <cctype>
#include <time.h>

void DateTimeFormatItem::format(std::ostream& os, std::shared_ptr<Logger> logger __attribute__((unused)), LogLevel::Level level __attribute__((unused)), LogEvent::ptr event)
{
    time_t time = event->getTime();
    struct tm t;
    localtime_r(&time, &t);
    char buf[64];
    strftime(buf, sizeof(buf), m_format.c_str(), &t);
    os << buf;
}

LogFormatter::LogFormatter(const std::string& pattern)
    :m_pattern(pattern)
{
    init();
}

std::string LogFormatter::format(std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event)
{
    std::stringstream ss;
    for(auto& item : m_items)
    {
        item->format(ss, logger, level, event);
    }
    return ss.str();
}

std::ostream& LogFormatter::format(std::ostream& ofs, std::shared_ptr<Logger> logger, LogLevel::Level level, LogEvent::ptr event)
{
    for(auto& item : m_items)
    {
        item->format(ofs, logger, level, event);
    }
    return ofs;
}

void LogFormatter::init()
{
    // 字符串映射到创建对应FormatterItem对象的lambda
    static std::map<std::string, std::function<LogFormatItem::ptr(const std::string& str)> > s_format_items = {
#define XX(str, C) \
        {#str, [](const std::string& fmt) { return LogFormatItem::ptr(new C(fmt));}}

            XX(m, MessageFormatItem),           //m:消息
            XX(p, LevelFormatItem),             //p:日志级别
            XX(t, ThreadIdFormatItem),          //t:线程id
            XX(n, NewLineFormatItem),           //n:换行
            XX(d, DateTimeFormatItem),          //d:日期时间
            XX(f, FilenameFormatItem),          //f:文件名
            XX(l, LineFormatItem),              //l:行号
            XX(T, TabFormatItem),               //T:Tab
            XX(N, ThreadNameFormatItem),        //N:线程名称
#undef XX
    };


    // @todo 找机会优化下, 以下这些逻辑不太好阅读...


    // 元组(项, 子格式, 是否需要转义)数组
    std::vector<std::tuple<std::string, std::string, int>> pattern_info;
    std::string sub_str;// pattern中的某项
    for(size_t i = 0; i < m_pattern.size(); ++i)
    {
        if(m_pattern[i] != '%')
        {
            sub_str.append(1, m_pattern[i]);
            continue;
        }

        // 如果最后一个字符是%，那就不是转义
        if((i + 1) < m_pattern.size()) 
        {
            if(m_pattern[i + 1] == '%') 
            {
                sub_str.append(1, '%');
                continue;
            }
        }

        int fmt_status = 0;
        size_t fmt_begin = 0;

        std::string str;
        std::string fmt;
        size_t n = i + 1;
        // 解读{}子格式，主要%d（时间）可以指定子格式
        while(n < m_pattern.size()) {
            if(!fmt_status && (!isalpha(m_pattern[n]) && m_pattern[n] != '{'
                    && m_pattern[n] != '}')) {
                str = m_pattern.substr(i + 1, n - i - 1);
                break;
            }
            if(fmt_status == 0) {
                if(m_pattern[n] == '{') {
                    str = m_pattern.substr(i + 1, n - i - 1);
                    fmt_status = 1; // 需要解析格式
                    fmt_begin = n;
                    ++n;
                    continue;
                }
            } else if(fmt_status == 1) {
                if(m_pattern[n] == '}') {
                    fmt = m_pattern.substr(fmt_begin + 1, n - fmt_begin - 1);
                    fmt_status = 0; // 不需要解析格式，原样输出就行
                    ++n;
                    break;
                }
            }
            ++n;
            if(n == m_pattern.size()) {
                if(str.empty()) {
                    str = m_pattern.substr(i + 1);
                }
            }
        }

        if(fmt_status == 0) 
        {
            if(!sub_str.empty()) {
                pattern_info.push_back(std::make_tuple(sub_str, std::string(), 0));
                sub_str.clear();
            }
            pattern_info.push_back(std::make_tuple(str, fmt, 1));
            i = n - 1;
        } 
        else if(fmt_status == 1) 
        {
            std::cout << "pattern parse error: " << m_pattern << " - " << m_pattern.substr(i) << std::endl;
            m_error = true;
            pattern_info.push_back(std::make_tuple("<<pattern_error>>", fmt, 0));
        }
    }
    if(!sub_str.empty()) {
        pattern_info.push_back(std::make_tuple(sub_str, "", 0));
    }

    for(auto& tup : pattern_info)
    {
        if(std::get<2>(tup) == 0)
        {
            // 如果是不需要转义的, 原样输出string
            m_items.push_back(LogFormatItem::ptr(new StringFormatItem(std::get<0>(tup))));
        }
        else
        {
            auto it = s_format_items.find(std::get<0>(tup));
            if(it == s_format_items.end())
            {
                std::string error_info = "<< unknown format: %" + std::get<0>(tup) + ">>";
                m_items.push_back(LogFormatItem::ptr(new StringFormatItem(error_info )));
                m_error = true;
            } 
            else
                m_items.push_back(it->second(std::get<1>(tup)));
        }
    }
}
