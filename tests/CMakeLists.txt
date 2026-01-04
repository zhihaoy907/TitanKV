cmake_minimum_required(VERSION 3.16)
project(TitanKVTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -------------------------------
# 测试目录
# -------------------------------
set(TEST_SOURCES
    test_iouring.cpp
    test_mutithread.cpp
    test_SPSCQueue.cpp
    test_log_format.cpp
    test_engine.cpp
)

# 1. io_uring 最基本的性能测试
add_executable(test_iouring test_iouring.cpp)
target_link_libraries(test_iouring PRIVATE titankv_core)

# 2. 基于 io_uring + CPU binding 的多线程性能测试
add_executable(test_mutithread test_mutithread.cpp)
target_link_libraries(test_mutithread PRIVATE titankv_core)

# 3. 基于 SPSCQueue 的性能测试
add_executable(test_SPSCQueue test_SPSCQueue.cpp)
target_link_libraries(test_SPSCQueue PRIVATE titankv_core)

# 4. log 类的功能性测试
add_executable(test_log_format test_log_format.cpp)
target_link_libraries(test_log_format PRIVATE titankv_core)

# 5. engine 类的功能性测试
add_executable(test_engine test_engine.cpp)
target_link_libraries(test_engine PRIVATE titankv_core)
