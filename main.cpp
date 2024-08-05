#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h> 
#include <poll.h>
#include <sys/time.h>
#include <sys/resource.h>

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


extern "C" void print_signal_warning(int sig)
{
    LOG_FMT_INFO(LOG_ROOT(), "Got signal %d from thread %d", sig, util::getThreadID());
}

extern "C" void handle_fatal_signal(int sig)
{
    // 暂时就是记录下日志, 不会有其它处理
    g_abort_loop = true;
    LOG_FMT_FATAL(LOG_ROOT(), "Got signal %d from thread %d", sig, util::getThreadID());

    // 设置信号默认处理函数, coredump或者是宕机...
    signal(sig, SIG_DFL);
    kill(getpid(), sig);
}


static void* signal_handler_thread(void* arg)
{
    void** args = (void**)arg;
    pthread_mutex_t* mtx = (pthread_mutex_t*)args[0];
    pthread_cond_t* cond = (pthread_cond_t*)args[1];

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGHUP);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGTSTP);// 和SIGSTOP差不多，也是让进程停止的信号，但是该信号可以捕获

    // 加锁保证执行顺序, 确保主线程cond_wait之后, 该线程再去通知主线程, 否则通知可能漏掉...
    pthread_mutex_lock(mtx);
    pthread_cond_broadcast(cond);
    pthread_mutex_unlock(mtx);

    pthread_sigmask(SIG_BLOCK, &set, NULL);
    while(true)
    {
        int rc = 0;
        int sig = 0;
        rc = sigwait(&set, &sig);
        if(rc == EINTR)
            continue;
        switch(sig)
        {
            case SIGHUP:
                // 该信号和终端异常断开有关, 应该不会收到该信号
                LOG_WARN(LOG_ROOT()) << "recv signal SIGHUP...";
                break;
            case SIGINT:
            case SIGQUIT:
            case SIGTERM:
                g_abort_loop = true; // 退出程序
                break;
            default:
                LOG_FMT_ERROR(LOG_ROOT(), "Unexpected signals signal %d...", sig);
                break;
        }
    }
    return NULL;
}


int init_signals()
{
    sigset_t set;
    struct sigaction sa;

    // 影响比较严重的信号
    sigemptyset(&set);
    sa.sa_mask = set;
    sa.sa_flags = 0;
    sa.sa_handler = handle_fatal_signal;
    
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);  // SIGABRT默认终止进程, 并且coredump
#ifdef SIGBUS
    sigaction(SIGBUS, &sa, NULL);   // 总线错误
#endif
    sigaction(SIGILL, &sa, NULL);   // 尝试执行非法机器指令
    sigaction(SIGFPE, &sa, NULL);   // 浮点计算错误

    sigemptyset(&set);
    sa.sa_mask = set;
    sa.sa_flags = 0;
    sa.sa_handler = print_signal_warning;
    sigaction(SIGALRM, &sa, NULL);
    
    // 进程忽视SIGPIPE信号, 如果客户端关闭tcp连接, server还继续写就会导致SIGPIPE信号产生;
    // 默认行为是退出进程, 这里改为忽视该信号;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, NULL);

    sigemptyset(&set);
    // sigaddset(&set, SIGPIPE); 已经忽视掉了...
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGHUP);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTSTP);// 和SIGSTOP差不多，也是让进程停止的信号，但是该信号可以捕获

    sigprocmask(SIG_SETMASK, &set, NULL);
    pthread_sigmask(SIG_SETMASK, &set, NULL);
    
    // 开启信号处理线程...
    pthread_attr_t thread_attr;
    pthread_attr_init(&thread_attr);
    pthread_attr_setscope(&thread_attr, PTHREAD_SCOPE_SYSTEM); // 该属性表示该线程会与系统上所有线程竞争资源...
    pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
    pthread_attr_setstacksize(&thread_attr, 65536);


    pthread_mutex_t handler_mtx;
    pthread_mutex_init(&handler_mtx, NULL);
    pthread_cond_t handler_cond;
    pthread_cond_init(&handler_cond, NULL);
    void* arg[2] = {NULL};
    arg[0] = &handler_mtx;
    arg[1] = &handler_cond;

    pthread_mutex_lock(&handler_mtx);
    pthread_t pid;
    int rc = pthread_create(&pid, &thread_attr, signal_handler_thread, arg);
    if(rc)
    {
        LOG_ERROR(LOG_ROOT()) << "create signal handle thread failed " << my_strerror(errno);
        pthread_mutex_unlock(&handler_mtx);
    }
    else
    {
        pthread_cond_wait(&handler_cond, &handler_mtx);
        pthread_mutex_unlock(&handler_mtx);
    }

    pthread_attr_destroy(&thread_attr);
    pthread_cond_destroy(&handler_cond);
    pthread_mutex_destroy(&handler_mtx);
    return rc;
}


int listening_socket_init()
{
    int rc = 0;
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
    if (init_signals() != 0)
        return 1;

    StdOutLogAppender::ptr out = std::make_shared<StdOutLogAppender>();
    out->setLevel(LogLevel::Level::ERROR);// 让标准输出默认输出ERROR级别以上的错误日志
    LOG_ROOT()->addAppender(out);

    struct rlimit rl;
    rl.rlim_cur = rl.rlim_max = (rlim_t) RLIM_INFINITY;
    if(setrlimit(RLIMIT_CORE, &rl))
        LOG_WARN(LOG_ROOT()) << "setrlimit could not change the size of core files to 'infinity';"
            "We may not be able to generate a core file on crash";

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
        // printf("Create listening socket failed: %s\n", my_strerror(errno));
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
                // 可能是被信号中断, 那表示可能通过信号要退出了; 检查下
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
            
            // 处理请求等等... 待补充...
        }
    }

    strerror_destroy();
    return 0;
}
