#ifndef WEB_SERVER_CONF_H
#define WEB_SERVER_CONF_H

#include <memory>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <map>

#include "yaml-cpp/yaml.h"

#include "conf/lexicalcast.h"
#include "log/log.h"
#include "util/util.h"
#include "thread/mutex.h"


/**
 * @brief 配置项的基类
 */
class ConfigItemBase
{
public:
    typedef std::shared_ptr<ConfigItemBase> ptr;

    /**
     * @brief 构造函数
     * @param[in] name 配置参数名称[0-9a-z_.]
     * @param[in] description 配置参数描述
     */
    ConfigItemBase(const std::string &name, const std::string &description = "")
        : m_name(name), m_description(description)
    {
        // std::transform是<algorithm>文件的, tolower是c库函数,将字符转化为小写
        std::transform(m_name.begin(), m_name.end(), m_name.begin(), ::tolower);
    }

    virtual ~ConfigItemBase() = default;

    /**
     * @brief 返回配置项名称
     */
    const std::string &getName() const { return m_name; }

    /**
     * @brief 返回配置项的描述
     */
    const std::string &getDescription() const { return m_description; }

    /**
     * @brief 将配置项转化为string
     */
    virtual std::string toString() = 0;

    /**
     * @brief 从指定string初始化配置项
     */
    virtual bool fromString(const std::string &val) = 0;

    /**
     * @brief 返回配置项{值}的类型名称
     */
    virtual std::string getTypeName() const = 0;

protected:
    std::string m_name;        // 配置项名称
    std::string m_description; // 配置项描述
};


template<class T, class FromStr = LexicalCast<std::string, T>
        ,class ToStr =  LexicalCast<T, std::string> >
class ConfigItem: public ConfigItemBase
{
public:
    typedef std::shared_ptr<ConfigItem> ptr;
    typedef std::function<void (const T& old_value, const T& new_value)> value_change_cb;

    /**
     * @brief 构造配置项
     * @param[in] name 配置项名,有效字符[0-9a-z_.]
     * @param[in] default_value 配置项默认值
     * @param[in] description 配置项描述
     */
    ConfigItem(const std::string& name,
               const T& default_value,
               const std::string& description = "")
        : ConfigItemBase(name, description), m_val(default_value)
    {}
    /**
     * @brief 将配置项的值转化为string
     * @return string
     */
    std::string toString() override
    {
        try{
            WebServer::ScopedLock<WebServer::Mutex> lk(m_mtx);
            return ToStr()(m_val);
        }
        catch(std::exception& exp)
        {
            LOG_ERROR(LOG_ROOT()) << "ConfigItem::toString exception "
                << exp.what() << " convert: " << util::typeToName<T>() << " to string"
                << " name=" << m_name;
        }
        return "";
    }

    /**
     * @brief 根据val的字符串,转化为相应配置
     * @param val 对应值(string类型)
     * @return 成功true,失败false
     */
    bool fromString(const std::string& val)override
    {
        try{
            setValue(FromStr()(val));
            return true;
        }
        catch (const std::exception& e)
        {
            LOG_ERROR(LOG_ROOT()) << "ConfigItem::fromString exception "
                << e.what() << "convert: string to " << util::typeToName<T>() << " name="
                << m_name << " - " << m_val;
        }
        return false;
    }

    /**
     * @brief 获取当前配置值
     */
    T getValue()
    {
        WebServer::ScopedLock<WebServer::Mutex> lk(m_mtx);
        return m_val;
    }

    /**
     * @brief 设置配置项的值
     * @param val 值
     */
    void setValue(const T& val)
    {
        WebServer::ScopedLock<WebServer::Mutex> lk(m_mtx);
        if(val == m_val)
            return;
        // 调用所有回调函数
        for(auto& cb : m_cbs)
        {
            cb.second(m_val, val);
        }
        m_val = val;
    }

    /**
     * @brief 返回配置{值}的类型名称
     * @return std::string 类型名称string
     */
    std::string getTypeName()const override
    {
        return util::typeToName<T>();
    }

    /**
     * @brief 增加配置值变化时的回调函数
     * @param cb value_change_cb
     * @return uint64_t 唯一ID
     */
    uint64_t addCallBack(value_change_cb cb)
    {
        /**
         * @brief C++11及以上, 能够保证多线程调用情况下, static局部变量只初始化一次
         * 官方文档链接: <https://zh.cppreference.com/w/cpp/language/storage_duration#.E9.9D.99.E6.80.81.E5.B1.80.E9.83.A8.E5.8F.98.E9.87.8F>
         */
        static uint64_t funcID = 0;
        WebServer::ScopedLock<WebServer::Mutex> lk(m_mtx);
        funcID++;
        m_cbs.insert(std::pair<uint64_t, value_change_cb>(funcID, cb));
        return funcID;
    }

    /**
     * @brief 删除指定回调函数
     * @param funcID 当初插入时对应的唯一ID
     */
    void delCallBack(uint64_t funcID)
    {
        WebServer::ScopedLock<WebServer::Mutex> lk(m_mtx);
        m_cbs.erase(funcID);
    }

    /**
     * @brief 获取回调函数
     * @param funcID 唯一ID
     * @return value_change_cb 回调函数
     */
    value_change_cb getCallBack(uint64_t funcID)
    {
        WebServer::ScopedLock<WebServer::Mutex> lk(m_mtx);
        auto it = m_cbs.find(funcID);
        return it == m_cbs.end() ? nullptr : it->second;
    }

    /**
     * @brief 清除所有回调函数
     */
    void clearCallBacks()
    {
        WebServer::ScopedLock<WebServer::Mutex> lk(m_mtx);
        m_cbs.clear();
    }

private:
    WebServer::Mutex                    m_mtx;
    T                                   m_val;
    /// 配置值发生变化时, 调用的回调函数
    std::map<uint64_t, value_change_cb> m_cbs;
};


class ConfigManager
{
public:
    typedef std::unordered_map<std::string, ConfigItemBase::ptr> ConfigItemMap;

    ConfigManager()
        : m_show_version(0),
        m_show_help(0)
    {}

    /**
     * @brief 获取参数名为name的配置项名,如果存在直接返回,如果不存在,创建参数配置并用default_value赋值
     * @tparam T 配置项的类型(如int，string)
     * @param[in] name 配置项名
     * @param[in] default_value 配置项默认值
     * @param[in] description 配置项描述
     * @return 返回对应的配置项,如果参数名存在但是类型不匹配则返回nullptr
     * @exception std::invalid_argument 如果参数名包含非法字符[^0-9a-z_.] 抛出异常 std::invalid_argument
     */
    template<class T>
    typename ConfigItem<T>::ptr lookup(const std::string& name,
        const T& default_value, const std::string& description = "")
    {
        WebServer::ScopedLock<WebServer::Mutex> lk(m_mtx);
        auto it = m_mapConfigs.find(name);
        if(it != m_mapConfigs.end())
        {
            auto tmp = std::dynamic_pointer_cast<ConfigItem<T>>(it->second);
            if(tmp)
            {
                LOG_INFO(LOG_ROOT()) << "Lookup name=" << name << "exists";
                return tmp;
            }
            else
            {// name对应的配置存在,但是类型不对
                LOG_ERROR(LOG_ROOT()) << "Lookup name=" << name << " exists but type not "
                    << util::typeToName<T>() << " real_type=" << it->second->getTypeName()
                    << " " << it->second->toString();
                return nullptr;
            }
        }
        // name含有非法字符
        if(name.find_first_not_of("abcdefghijklmnopqrstuvwxyz._0123456789") != std::string::npos)
        {
            LOG_ERROR(LOG_ROOT()) << "Lookup name invalid " << name;
            throw std::invalid_argument(name);
        }

        // 创建一个新的配置项
        typename ConfigItem<T>::ptr v(new ConfigItem<T>(name, default_value, description));
        m_mapConfigs[name] = v;
        return v;
    }

    /**
     * @brief 重载
     * @tparam T 配置项的类型(如int，string)
     * @param name 配置项名
     * @return 存在，返回配置项指针，否则返回nullptr
     */
    template<class T>
    typename ConfigItem<T>::ptr lookup(const std::string& name)
    {
        WebServer::ScopedLock<WebServer::Mutex> lk(m_mtx);
        auto it = m_mapConfigs.find(name);
        if(it != m_mapConfigs.end())
        {
            return std::dynamic_pointer_cast<ConfigItem<T> >(it->second);
        }
        return nullptr;
    }

    /**
     * @brief 使用YAML::Node初始化配置模块(Node实际就是读取一个yaml文件)
     */
    void loadFromYaml(const YAML::Node& root);

    /**
     * @brief 读取控制台传入的选项
     */
    bool loadFromCmd(int argc, char* argv[]);

    /**
     * @brief 查找配置项,返回配置项指针,没找到返回nullptr
     * @param[in] name 配置参数名称
     */
    ConfigItemBase::ptr lookupBase(const std::string& name);

    /**
     * @brief 使用配置模块里面所有配置项,调用一次cb函数
     * @param[in] cb 配置项回调函数
     */
    void visit(std::function<void(ConfigItemBase::ptr)> cb);

    int show_version() const
    {
        return m_show_version;
    }

    int show_help() const
    {
        return m_show_help;
    }

private:
    int                 m_show_version; // 展示程序版本号后, 直接退出;
    int                 m_show_help;    // 展示程序帮助文档后, 直接退出;

    WebServer::Mutex    m_mtx;
    ConfigItemMap       m_mapConfigs;   // 配置项映射
};

#endif // WEB_SERVER_CONF_H
