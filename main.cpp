#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unistd.h>

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


// void task(void *arg)
// {
//     if(arg == nullptr)
//         exit(-1);// 线程中使用, 整个进程直接退出
//     httpData* data = (httpData*)arg;
//     ParseRequest ret = data->handleRequest();

//     struct epoll_event ev;
//     ev.data.ptr = data;
//     ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;

//     if(ret == ParseRequest::KEEPALIVE)
//     {// 是长连接, 重置
//         data->reset();
//         // 重新添加计时器和回调函数
//         data->timer = Singleton<TimerManager>::getInstance().addTimer(TIMEOUT, [data, &ev](){
//             epoll_ctl(epoll_fd, EPOLL_CTL_DEL,data->getFd(), &ev);
//             close(data->getFd());
//             delete data;
//         });
//         // 先添加定时器在激活, 否则可能激活EPOLLONESHOT, 马上就触发了, timer还没加上去
//         epoll_ctl(epoll_fd, EPOLL_CTL_MOD, data->getFd(), &ev);        // 重新激活EPOLLONESHOT
//     }
//     if(ret == ParseRequest::FINISH || ret == ParseRequest::ERROR)
//     {//不能放到timer回调中,必须马上epoll_del, 否则回调还没执行, 可能又触发了
//         epoll_ctl(epoll_fd, EPOLL_CTL_DEL, data->getFd(), &ev);
//         close(data->getFd());
//         delete data;
//     }
// }

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

    // 创建epoll实例
    int epoll_fd = epoll_create(10);
    if(epoll_fd == -1)
    {
        LOG_FATAL(LOG_ROOT()) << "Create epoll instance failed: " << my_strerror(errno);
        exit(EXIT_FAILURE);
    }

    // 添加 listen socket 到epoll实例中去
    struct epoll_event ev;
    ev.data.fd = listening_socket;
    ev.events = EPOLLIN | EPOLLET;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listening_socket, &ev);

    // 存放epoll_wait返回的事件
    struct epoll_event epoll_event_list[MAX_EVENTS];
    while(true)
    {
        int event_count = epoll_wait(epoll_fd, epoll_event_list, MAX_EVENTS, -1);
        if (event_count == -1 && errno != EINTR)
        {
            LOG_ERROR(LOG_ROOT()) << "epoll_wait failed: " << my_strerror(errno);
            break;
        }

        // 处理已经触发的事件
        
    }

    // while(true)
    // {
    //     int events_num = epoll_wait(epoll_fd, epevList, MAXEVENTS, -1);
    //     if(events_num == -1 && errno != EINTR)
    //     {
    //         perror("epoll_wait");
    //         break;
    //     }
    //     else
    //     {
    //         for(int i = 0; i < events_num; ++i)
    //         {
    //             if(epevList[i].data.fd == listen_fd)
    //             {// 监听socket触发
    //                 struct sockaddr_in clientAddr;
    //                 socklen_t socklen = sizeof(clientAddr);
    //                 int fd = -1;
    //                 // 由于是边缘触发, listen_fd读就绪时, 可能有多个请求到达

    //                 // 最终 accept 会阻塞... 需要先设置非阻塞, 或者专门留一个线程去做accept

    //                 // 记得nginx对于listening socket是水平触发
    //                 // mariadb 使用poll, 压根不支持边缘触发, 然后设置listening socket为非阻塞, 再进行accept; 不是很理解, 因为水平触发返回, accept肯定可以成功才对...

    //                 while((fd = accept(listen_fd, (struct sockaddr* )&clientAddr, &socklen)) > -1){
    //                     util::set_nonblock(fd);
    //                     httpData* data = new httpData(fd, ConfigManager::lookup<std::string>("htdocs")->getValue());
    //                     struct epoll_event ev;
    //                     ev.data.ptr = data;
    //                     ev.events = EPOLLET | EPOLLIN | EPOLLONESHOT;
    //                     epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
    //                     // 绑定一个计时器, 回调函数的任务就是 delete httpdata
    //                     data->timer = Singleton<TimerManager>::getInstance().addTimer(TIMEOUT, [data, epoll_fd, &ev](){
    //                         epoll_ctl(epoll_fd, EPOLL_CTL_DEL, data->getFd(), &ev);
    //                         close(data->getFd());
    //                         delete data;
    //                     });
    //                 }
    //             }
    //             else
    //             {// 客户端socket触发
    //                 httpData* data = (httpData*) epevList[i].data.ptr;
    //                 if(epevList[i].events & EPOLLIN)
    //                 {// 添加任务时断开计时器
    //                     data->timer->cancel();// 取消timer的任务
    //                     data->timer = nullptr;
    //                     struct Task tk{task, data};
    //                     switch(pool.addTask(tk))
    //                     {
    //                         case ThreadPoolStatus::LOCK_FAILURE:
    //                             printf("mutex或cond错误!\n");
    //                             break;
    //                         case ThreadPoolStatus::QUEUE_FULL:
    //                             printf("任务队列满了!\n");
    //                             break;
    //                         case ThreadPoolStatus::SHUTDOWN:
    //                             printf("线程池已关闭!\n");
    //                             break;
    //                         default:
    //                             break;// 任务添加成功
    //                     }
    //                 }
    //                 else if(epevList[i].events & EPOLLERR)
    //                     printf("EPOLLERR!\n");
    //                 else if(epevList[i].events & EPOLLHUP)
    //                     printf("EPOLLHUP!\n");
    //             }
    //         }
    //     }
    //     epoll_timeout = Singleton<TimerManager>::getInstance().getMinTO();
    //     if(epoll_timeout < -1)
    //     {// 已经有定时器到期了
    //         Singleton<TimerManager>::getInstance().takeAllTimeout();
    //         epoll_timeout = Singleton<TimerManager>::getInstance().getMinTO();
    //     }
    // }

    strerror_destroy();
    return 0;
}
