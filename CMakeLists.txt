# cmake最低版本号要求
cmake_minimum_required(VERSION 3.18)

# 项目名
project(WebServer)

add_compile_options(-std=c++11 -g -ggdb -Wall -Wextra)

include_directories(${PROJECT_SOURCE_DIR}/include/third_lib)
include_directories(${PROJECT_SOURCE_DIR}/include)

link_directories(${PROJECT_SOURCE_DIR}/lib)

# find_library执行完毕后, 会将结果缓存到CMakeCache.txt中, 这里取消yaml-cpp缓存结果
unset(yaml-cpp CACHE)
# 第一个yaml-cpp是结果, 第二个是库名, HINTS用来指定额外查找路径, REQUIRED表示执行没找到就停止运行
find_library(yaml-cpp yaml-cpp HINTS ${PROJECT_SOURCE_DIR}/lib REQUIRED)
# EXISTS后面是一个文件或者目录路径，只有合法存在，就返回ture
IF(EXISTS ${yaml-cpp})
    message(STATUS "libyaml-cpp.a located in ${yaml-cpp}")
ELSE()
    message(FATAL_ERROR "libyaml-cpp.a not found!")
ENDIF()

aux_source_directory(${PROJECT_SOURCE_DIR}/src SRC_FILE)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/log SRC_FILE)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/util SRC_FILE)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/conf SRC_FILE)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/thread SRC_FILE)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build)


add_executable(WebServer main.cpp ${SRC_FILE})
set_target_properties(WebServer PROPERTIES COMPILE_FLAGS "-pthread" LINK_FLAGS "-pthread")
target_link_libraries(WebServer yaml-cpp)

# 测试用例:
# 测试配置模块
add_executable(conf_test tests/test_conf.cpp ${SRC_FILE})
set_target_properties(conf_test PROPERTIES COMPILE_FLAGS "-pthread" LINK_FLAGS "-pthread")
target_link_libraries(conf_test yaml-cpp)

# 测试日志模块
add_executable(log_test tests/test_log.cpp ${SRC_FILE})
set_target_properties(log_test PROPERTIES COMPILE_FLAGS "-pthread" LINK_FLAGS "-pthread")
target_link_libraries(log_test yaml-cpp)

# 测试线程模块
add_executable(thread_test tests/test_thread.cpp ${SRC_FILE})
set_target_properties(thread_test PROPERTIES COMPILE_FLAGS "-pthread" LINK_FLAGS "-pthread")
target_link_libraries(thread_test yaml-cpp)
