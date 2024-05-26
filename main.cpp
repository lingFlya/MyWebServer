#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h> 
#include <poll.h>

#include <csignal>

#include "errmsg/my_errno.h"
#include "conf/conf.h"
#include "thread/threadpool.h"
#include "util/util.h"
#include "util/singleton.h"
#include "httpData.h"
#include "timer/thr_timer.h"


#define WEB_SERVER_VERSION "0.1"
#define MAX_EVENTS 4096


bool volatile g_abort_loop;


void show_help_info()
{
    const char* fmt = 
        "web server version: %s\n"
        "Usage: WebServer [-?hv]\n"
        "   -?,-h   : show help information and exit.\n"
        "   -v      : show version and exit.\n";
    printf(fmt, WEB_SERVER_VERSION);
}


int config_init(int argc, char* argv[])
{
    ConfigManager& configManager = Singleton<ConfigManager>::getInstance();

    // 设置服务器默认配置
    configManager.lookup<unsigned short>("server.port", 6666, "Port");
    configManager.lookup<int>("server.thread_count", 4, "thread count");
    configManager.lookup<std::string>("server.htdocs", "/home/test", "web file dir");

    if (false == configManager.loadFromCmd(argc, argv))
    {
        LOG_FATAL(LOG_ROOT()) << "load cmd options failed, exit!" << my_strerror(errno);
        exit(EXIT_FAILURE);
    }

    // 相对路径还是不太方便, 容易出错.... 最好换为绝对路径
    YAML::Node root = YAML::LoadFile("../conf/config.yml");
    configManager.loadFromYaml(root);
    return 0;
}


int listening_socket_init()
{
    // 进程忽视SIGPIPE信号, 如果客户端关闭tcp连接, server还继续写就会导致SIGPIPE信号产生, 默认行为是退出进程, 这里改为忽视该信号;
    int rc = util::set_signal_ignore(SIGPIPE);
    if(rc == -1)
    {
        LOG_ERROR(LOG_ROOT()) << "Ignore SIGPIPE failed: " << my_strerror(errno);
        // exit(EXIT_FAILURE); // 不一定要退出, 不退出运行试试
    }

    ConfigManager& configManager = Singleton<ConfigManager>::getInstance();
    ConfigItem<unsigned short>::ptr port = configManager.lookup<unsigned short>("port");
    rc = util::socket_bind_listen(port->getValue());
    if(rc == -1)
    {
        LOG_FATAL(LOG_ROOT()) << "Create listen socket failed: " << my_strerror(errno);
        exit(EXIT_FAILURE);
    }
    return rc;
}


int main(int argc, char* argv[])
{
    strerror_init();
    log_init();
    config_init(argc, argv);
    ConfigManager& configManager = Singleton<ConfigManager>::getInstance();
    if(configManager.show_help())
    {
        show_help_info();
        return 0;
    }
    if(configManager.show_version())
    {
        printf("web server version: %s\n", WEB_SERVER_VERSION);
        return 0;
    }
    int listening_socket = listening_socket_init();
    if(listening_socket == -1)
    {
        LOG_FATAL(LOG_ROOT()) << "Create listening socket failed: " << my_strerror(errno);
        printf("Create listening socket failed: %s\n", my_strerror(errno));
        return 1;
    }

    struct pollfd fds[1];
    fds[0].fd = listening_socket;
    fds[0].events = POLLIN;

    g_abort_loop = false;
    int rc = -1;
    while(!g_abort_loop)
    {
        rc = poll(fds, 1, -1);
        if(rc < 0)
        {
            if(errno == EINTR)
            {
                // 可能是被信号中断, 检查程序是否准备退出
                if(!g_abort_loop)
                    LOG_WARN(LOG_ROOT()) << "poll failed: " << my_strerror(errno);
            }
            continue;
        }
        if(g_abort_loop)
        {
            break;
        }
        int flags = fcntl(listening_socket, F_GETFL, 0);
        fcntl(listening_socket, F_SETFL, flags | O_NONBLOCK);
        // 一直重复尝试接收新请求, 直到没有新请求为止
        bool try_accept = true;
        while(try_accept)
        {
            struct sockaddr_storage client_addr;
            socklen_t len = sizeof(sockaddr_storage);
            int client_sock = accept4(listening_socket, (struct sockaddr*)&client_addr, &len, SOCK_CLOEXEC);
            if(client_sock == -1)
            {
                if(errno != EAGAIN && errno != EWOULDBLOCK)
                    LOG_WARN(LOG_ROOT()) << "accept failed: " << my_strerror(errno);
                // 当前没有连续需要accept, 进入下次poll阻塞等待
                try_accept = false;
                fcntl(listening_socket, F_SETFL, flags);
                continue;
            }
            // 处理新的客户端连接....
            
            // ... 待补充...
        }
    }

    strerror_destroy();
    return 0;
}
