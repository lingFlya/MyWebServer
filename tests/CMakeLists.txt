# cmake最低版本号要求
cmake_minimum_required(VERSION 3.18)

project(tests
        VERSION 0.1
        LANGUAGES C CXX
)


# 日志模块的测试
set(LOG_TEST_SRC_FILES
    ../src/util/util.cpp
    ../src/errmsg/my_errno.cpp
    # log模块用到了thread模块内容, 可以考虑修改, log模块还是直接使用自己的锁吧
    ../src/thread/semaphore.cpp
    ../src/thread/thread.cpp
    ../src/thread/threadpool.cpp
    ../src/log/appender.cpp
    ../src/log/event.cpp
    ../src/log/format.cpp
    ../src/log/level.cpp
    ../src/log/log.cpp
)
add_executable(log_test test_log.cpp ${LOG_TEST_SRC_FILES})
set_target_properties(log_test PROPERTIES COMPILE_FLAGS "-pthread" LINK_FLAGS "-pthread")


# 配置模块的测试
set(CONF_TEST_SRC_FILES
    ../src/util/util.cpp
    ../src/errmsg/my_errno.cpp
    ../src/thread/semaphore.cpp
    ../src/thread/thread.cpp
    ../src/thread/threadpool.cpp
    ../src/log/appender.cpp
    ../src/log/event.cpp
    ../src/log/format.cpp
    ../src/log/level.cpp
    ../src/log/log.cpp
    ../src/conf/conf.cpp
)
add_executable(conf_test test_conf.cpp ${CONF_TEST_SRC_FILES})
set_target_properties(conf_test PROPERTIES COMPILE_FLAGS "-pthread" LINK_FLAGS "-pthread")
target_link_libraries(conf_test yaml-cpp)


# 线程模块的测试
set(THREAD_TEST_SRC_FILES
    ../src/util/util.cpp
    ../src/errmsg/my_errno.cpp
    ../src/log/appender.cpp
    ../src/log/event.cpp
    ../src/log/format.cpp
    ../src/log/level.cpp
    ../src/log/log.cpp
    ../src/thread/semaphore.cpp
    ../src/thread/thread.cpp
    ../src/thread/threadpool.cpp
)
add_executable(thread_test test_thread.cpp ${THREAD_TEST_SRC_FILES})
set_target_properties(thread_test PROPERTIES COMPILE_FLAGS "-pthread" LINK_FLAGS "-pthread")


# 测试定时器模块
include_directories(${PROJECT_SOURCE_DIR}/tests/timer)
set(TIMER_TEST_SRC_FILES
    ../src/util/util.cpp
    ../src/errmsg/my_errno.cpp
    ../src/timer/queues.cpp
    ../src/timer/thr_timer.cpp
    ./timer/thread_init.cpp
)
add_executable(timer_test timer/main.cpp ${TIMER_TEST_SRC_FILES})
set_target_properties(timer_test PROPERTIES COMPILE_FLAGS "-pthread" LINK_FLAGS "-pthread")
