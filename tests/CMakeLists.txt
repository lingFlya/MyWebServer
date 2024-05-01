# cmake最低版本号要求
cmake_minimum_required(VERSION 3.18)

project(WebServer_TEST
        VERSION 0.1
        LANGUAGES C CXX
)

# 重置上层CMakeLists.txt继承的 SRC_FILE 变量
unset(SRC_FILE CACHE)
set(SRC_FILE
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
# 日志模块
add_executable(log_test test_log.cpp ${SRC_FILE})
set_target_properties(log_test PROPERTIES COMPILE_FLAGS "-pthread" LINK_FLAGS "-pthread")




unset(SRC_FILE CACHE)
set(SRC_FILE
    ../src/util/util.cpp
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

# 配置模块
add_executable(conf_test test_conf.cpp ${SRC_FILE})
set_target_properties(conf_test PROPERTIES COMPILE_FLAGS "-pthread" LINK_FLAGS "-pthread")
target_link_libraries(conf_test yaml-cpp)





unset(SRC_FILE CACHE)
set(SRC_FILE
    ../src/util/util.cpp
    ../src/log/appender.cpp
    ../src/log/event.cpp
    ../src/log/format.cpp
    ../src/log/level.cpp
    ../src/log/log.cpp
    ../src/thread/semaphore.cpp
    ../src/thread/thread.cpp
    ../src/thread/threadpool.cpp
)

# 测试线程模块
add_executable(thread_test test_thread.cpp ${SRC_FILE})
set_target_properties(thread_test PROPERTIES COMPILE_FLAGS "-pthread" LINK_FLAGS "-pthread")


include_directories(${PROJECT_SOURCE_DIR}/tests/timer)
unset(SRC_FILE CACHE)
set(SRC_FILE
    ../src/util/util.cpp
    ../src/timer/queues.cpp
    ../src/timer/thr_timer.cpp
    ./timer/thread_init.cpp
)

# 测试定时器模块
add_executable(timer_test timer/main.cpp ${SRC_FILE})
set_target_properties(timer_test PROPERTIES COMPILE_FLAGS "-pthread" LINK_FLAGS "-pthread")
