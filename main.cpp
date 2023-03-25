#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <csignal>

#include "conf/conf.h"

#include "thread/threadpool.h"
#include "util/util.h"
#include "util/singleton.h"
#include "httpData.h"
#include "timer.h"

void task(void *arg)
{
    if(arg == nullptr)
        exit(-1);// 线程中使用, 整个进程直接退出
    httpData* data = (httpData*)arg;
    ParseRequest ret = data->handleRequest();

    struct epoll_event ev;
    ev.data.ptr = data;
    ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;

    if(ret == ParseRequest::KEEPALIVE)
    {// 是长连接, 重置
        data->reset();
        // 重新添加计时器和回调函数
        data->timer = Singleton<TimerManager>::getInstance().addTimer(TIMEOUT, [data, &ev](){
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL,data->getFd(), &ev);
            close(data->getFd());
            delete data;
        });
        // 先添加定时器在激活, 否则可能激活EPOLLONESHOT, 马上就触发了, timer还没加上去
        epoll_ctl(epoll_fd, EPOLL_CTL_MOD, data->getFd(), &ev);        // 重新激活EPOLLONESHOT
    }
    if(ret == ParseRequest::FINISH || ret == ParseRequest::ERROR)
    {//不能放到timer回调中,必须马上epoll_del, 否则回调还没执行, 可能又触发了
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, data->getFd(), &ev);
        close(data->getFd());
        delete data;
    }
}

int main(int argc, char* argv[])
{
    ConfigManager& configManager = Singleton<ConfigManager>::getInstance();

    /// 先设置默认配置
    configManager.lookup<short>("webserver.port", 6666, "端口号");
    configManager.lookup<int>("webserver.thread_count", 4, "线程数");
    configManager.lookup<std::string>("webserver.htdocs", "/home/test", "静态web文件");

    /// 从配置文件中读取配置
    YAML::Node root = YAML::LoadFile("../conf/config.yml");
    configManager.loadFromYaml(root);

    /// 从启动参数中读取配置
    configManager.loadFromCmd(argc, argv);

    // 忽视SIGPIPE信号(如果客户端突然关闭读端, 那么服务器write就会碰到一个SIGPIPE信号)
    util::setSigIgn(SIGPIPE);
    ConfigItem<uint16_t>::ptr portConfig = configManager.lookup<uint16_t>("port");
    int listen_fd = util::socket_bind_listen(portConfig->getValue());
    if(listen_fd == -1)
    {
        perror("socket_bind_listen");
        return -1;
    }
    // 将监听socket设置为非阻塞搭配边缘触发, 这样while(1)accept();就不会阻塞了
    if(util::setNonBlock(listen_fd) == -1)
    {
        perror("setNonBlock");
        return -1;
    }
    // 线程池初始化
    threadPool pool;
    printf("创建线程池成功!\n");

    // 获取epoll实例
    int epoll_fd = epoll_create(10);
    if(epoll_fd == -1)
    {
        perror("epoll_create");
        _exit(EXIT_FAILURE);
    }
    // 添加listen_fd到epoll实例中去
    struct epoll_event ev;
    ev.data.fd = listen_fd;
    ev.events = EPOLLIN | EPOLLET;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev);

    const int MAXEVENTS = 4096;
    struct epoll_event epevList[MAXEVENTS];         // 存放epoll_wait返回的事件
    int64_t epoll_timeout = -1;                     // 利用epoll_wait实现定时器
    while(true)
    {
        int events_num = epoll_wait(epoll_fd, epevList, MAXEVENTS, epoll_timeout);
        // EINTR中断不算错误, 比如被信号中断
        if(events_num == -1 && errno != EINTR)
        {
            perror("epoll_wait");
            break;
        }
        else if(events_num == 0)
        {
            auto& timerManager = Singleton<TimerManager>::getInstance();
            timerManager.takeAllTimeout();
        }
        else
        {
            for(int i = 0; i < events_num; ++i)
            {
                if(epevList[i].data.fd == listen_fd)
                {// 监听socket触发
                    struct sockaddr_in clientAddr;
                    socklen_t socklen = sizeof(clientAddr);
                    int fd = -1;
                    // 由于是边缘触发, listen_fd读就绪时, 可能有多个请求到达.
                    while((fd = accept(listen_fd, (struct sockaddr* )&clientAddr, &socklen)) > -1){
                        util::setNonBlock(fd);
                        httpData* data = new httpData(fd, ConfigManager::lookup<std::string>("htdocs")->getValue());
                        struct epoll_event ev;
                        ev.data.ptr = data;
                        ev.events = EPOLLET | EPOLLIN | EPOLLONESHOT;
                        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
                        // 绑定一个计时器, 回调函数的任务就是 delete httpdata
                        data->timer = Singleton<TimerManager>::getInstance().addTimer(TIMEOUT, [data, epoll_fd, &ev](){
                            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, data->getFd(), &ev);
                            close(data->getFd());
                            delete data;
                        });
                    }
                }
                else
                {// 客户端socket触发
                    httpData* data = (httpData*) epevList[i].data.ptr;
                    if(epevList[i].events & EPOLLIN)
                    {// 添加任务时断开计时器
                        data->timer->cancel();// 取消timer的任务
                        data->timer = nullptr;
                        struct Task tk{task, data};
                        switch(pool.addTask(tk))
                        {
                            case ThreadPoolStatus::LOCK_FAILURE:
                                printf("mutex或cond错误!\n");
                                break;
                            case ThreadPoolStatus::QUEUE_FULL:
                                printf("任务队列满了!\n");
                                break;
                            case ThreadPoolStatus::SHUTDOWN:
                                printf("线程池已关闭!\n");
                                break;
                            default:
                                break;// 任务添加成功
                        }
                    }
                    else if(epevList[i].events & EPOLLERR)
                        printf("EPOLLERR!\n");
                    else if(epevList[i].events & EPOLLHUP)
                        printf("EPOLLHUP!\n");
                }
            }
        }
        epoll_timeout = Singleton<TimerManager>::getInstance().getMinTO();
        if(epoll_timeout < -1)
        {// 已经有定时器到期了
            Singleton<TimerManager>::getInstance().takeAllTimeout();
            epoll_timeout = Singleton<TimerManager>::getInstance().getMinTO();
        }
    }
    return 0;
}
