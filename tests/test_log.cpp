/**
 * @date    2024/8/11
 * @author  2mulin
 * @brief   日志模块的测试
 */

#include "log/log.h"
#include "util/util.h"

#include <time.h>
#include <unistd.h>


void test_root_logger()
{
    // 测试root日志器, LoggerManager默认构造时就会创建好一个root日志器;
    Logger::ptr logger = LOG_ROOT();

    LOG_DEBUG(logger) << "test LOG_DEBUG macro!";
    LOG_INFO(logger) << "test LOG_INFO macro!";
    LOG_WARN(logger) << "test LOG_WARN macro!";
    LOG_ERROR(logger) << "test LOG_ERROR macro!";
    LOG_FATAL(logger) << "test LOG_FATAL macro!";

    LOG_FMT_DEBUG(logger, "test LOG_FMT_DEBUG macro! %s", "success!");
    LOG_FMT_INFO(logger, "test LOG_FMT_INFO macro! %s", "success!");
    LOG_FMT_WARN(logger, "test LOG_FMT_WARN macro! %s", "success!");
    LOG_FMT_ERROR(logger, "test LOG_FMT_ERROR macro! %s", "success!");
    LOG_FMT_FATAL(logger, "test LOG_FMT_FATAL macro! %s", "success!");
}

void test_custom_logger()
{
    // 测试用户创建的日志器, 能否正常工作, 创建名为test_log_1的日志器进行测试;
    Logger::ptr logger = LoggerMgr::getInstance().getLogger("test_log_1");
    FileLogAppender::ptr fileLogAppender = std::make_shared<FileLogAppender>("../log/test/log.log");
    // 给文件输出不同的日志格式
    LogFormatter::ptr formatter = std::make_shared<LogFormatter>("%d%T%p%T%t%T%m%n");
    fileLogAppender->setFormatter(formatter);
    fileLogAppender->setLevel(LogLevel::INFO);
    logger->addAppender(fileLogAppender);

    LOG_DEBUG(logger) << "test_log_1 LOG_DEBUG macro!";
    LOG_INFO(logger) << "test_log_1 LOG_INFO macro!";
    LOG_WARN(logger) << "test_log_1 LOG_WARN macro!";
    LOG_ERROR(logger) << "test_log_1 LOG_ERROR macro!";
    LOG_FATAL(logger) << "test_log_1 LOG_FATAL macro!";

    LOG_FMT_DEBUG(logger, "test_log_1 LOG_FMT_DEBUG macro! %s", "success!");
    LOG_FMT_INFO(logger, "test_log_1 LOG_FMT_INFO macro! %s", "success!");
    LOG_FMT_WARN(logger, "test_log_1 LOG_FMT_WARN macro! %s", "success!");
    LOG_FMT_ERROR(logger, "test_log_1 LOG_FMT_ERROR macro! %s", "success!");
    LOG_FMT_FATAL(logger, "test_log_1 LOG_FMT_FATAL macro! %s", "success!");
}

void worker(int worker_id, Logger::ptr logger)
{
    srand(time(NULL));
    for(int i = 0; i < 10000; ++i)
    {
        int sleep_ms = random() % 1000;
        usleep(sleep_ms);
        LOG_FMT_INFO(logger, "worker%d loop %d times", worker_id, i);
    }
}

void test_multithread_logger()
{
    // 创建名为log_multithread的日志器进行测试;
    Logger::ptr logger = LoggerMgr::getInstance().getLogger("multithread_log");
    
    // 如果多线程使用同一个文件, 必须使用相同Appender对象去指定, 否则线程不安全
    FileLogAppender::ptr fileLogAppender = std::make_shared<FileLogAppender>("../log/test/multithread_log.log");
    LogFormatter::ptr formatter = std::make_shared<LogFormatter>("%d{%Y-%m-%d %H:%M:%S}%T%t%T%N%T[%p]%T%f:%l%T%m%n");
    fileLogAppender->setFormatter(formatter);
    fileLogAppender->setLevel(LogLevel::INFO);
    logger->addAppender(fileLogAppender);

    WebServer::Thread t1(std::bind(worker, 1, logger), "log_thread_1");
    WebServer::Thread t2(std::bind(worker, 2, logger), "log_thread_2");
    WebServer::Thread t3(std::bind(worker, 3, logger), "log_thread_3");
    t1.join();
    t2.join();
    t3.join();
}

int main()
{
    test_root_logger();
    test_custom_logger();
    test_multithread_logger();
    return 0;
}
