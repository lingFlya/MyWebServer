#include "conf/conf.h"

#include "log/log.h"


int show_version;
int show_help;


static void listAllMember(const std::string& prefix, const YAML::Node& node,
                          std::list<std::pair<std::string, const YAML::Node> >& output)
{
    /// 如果出现了"abcdefghijklmnopqrstuvwxyz._0123456789"字符之外的字符, 说明配置名格式不正确
    if(prefix.find_first_not_of("abcdefghijklmnopqrstuvwxyz._0123456789")
       != std::string::npos)
    {
        LOG_ERROR(LOG_ROOT()) << "ConfigItem invalid name: " << prefix << " : " << node;
        return;
    }
    output.push_back(std::make_pair(prefix, node));
    if(node.IsMap()) {
        for(auto it = node.begin(); it != node.end(); ++it) {
            // 这个scalar表示yaml纯量, 最基本,不可再分的值.
            listAllMember(prefix.empty() ? it->first.Scalar()
                                         : prefix + "." + it->first.Scalar(), it->second, output);
        }
    }
}


ConfigItemBase::ptr ConfigManager::lookupBase(const std::string &name)
{
    WebServer::ScopedLock<WebServer::Mutex> lk(m_mtx);
    auto it = m_mapConfigs.find(name);
    if(it != m_mapConfigs.end())
        return it->second;
    return nullptr;
}

void ConfigManager::loadFromYaml(const YAML::Node& root)
{
    std::list<std::pair<std::string, const YAML::Node> > allNodes;
    listAllMember("", root, allNodes);

    for(auto& i : allNodes)
    {
        std::string key = i.first;
        if(key.empty())
            continue;

        // std::transform将一个函数应用于某范围的各个元素，并在目标范围存储结果。这里是将key中的字符全部转化为小写
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        ConfigItemBase::ptr item = lookupBase(key);

        if(item)
        {
            if(i.second.IsScalar())
            {
                item->fromString(i.second.Scalar());
            }
            else
            {
                std::stringstream ss;
                ss << i.second;
                item->fromString(ss.str());
            }
        }
        else
        {
            if(i.second.IsScalar())
                LOG_WARN(LOG_ROOT()) << "{" << key << "} exists Invalid configuration item!";
        }
    }
}

bool ConfigManager::loadFromCmd(int argc, char **argv)
{
    // Linux提供的getopt或者getopt_long接口用起来太复杂, 且出错会去控制台输出, 代码可读性也不好, 需要反复去看文档;
    // 自己实现个简单版本的, 能用就行, 暂时只支持 -h, -v, -? 选项
    for(int i = 1; i < argc; ++i)
    {
        char* p = argv[i];

        if(*p++ != '-')
        {
            LOG_ERROR(LOG_ROOT()) << "invalid option: " << argv[i];
            return false;
        }
        while(*p)
        {
            switch (*p++)
            {
            case '?':
            case 'h':
                m_show_help = 1;
                break;
            case 'v':
                m_show_version = 1;
                break;
            default:
                LOG_FMT_ERROR(LOG_ROOT(), "invalid option: \"%c\"", *(p - 1));
                return false;
            }
        }
    }
    return true;
}

void ConfigManager::visit(std::function<void(ConfigItemBase::ptr)> cb)
{
    WebServer::ScopedLock<WebServer::Mutex> lk(m_mtx);
    for(auto iter = m_mapConfigs.begin(); iter != m_mapConfigs.end(); ++iter)
    {
        cb(iter->second);
    }
}
