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
    test_bench_io.cpp
    test_concurrency.cpp
)

# 1. io_uring 最基本的性能测试
add_executable(test_iouring test_iouring.cpp)
target_link_libraries(test_iouring PRIVATE titankv_core)
target_compile_options(test_iouring PRIVATE -g -O0 -fno-omit-frame-pointer)

# 2. 基于 io_uring + CPU binding 的多线程性能测试
add_executable(test_mutithread test_mutithread.cpp)
target_link_libraries(test_mutithread PRIVATE titankv_core)
target_compile_options(test_mutithread PRIVATE -g -O0 -fno-omit-frame-pointer)

# 3. 基于 SPSCQueue 的性能测试
add_executable(test_SPSCQueue test_SPSCQueue.cpp)
target_link_libraries(test_SPSCQueue PRIVATE titankv_core)
target_compile_options(test_SPSCQueue PRIVATE -g -O0 -fno-omit-frame-pointer)

# 4. log 类的功能性测试
add_executable(test_log_format test_log_format.cpp)
target_link_libraries(test_log_format PRIVATE titankv_core)
target_compile_options(test_log_format PRIVATE -g -O0 -fno-omit-frame-pointer)

# 5. engine 类的功能性测试
add_executable(test_engine test_engine.cpp)
target_link_libraries(test_engine PRIVATE titankv_core)
target_compile_options(test_engine PRIVATE -g -O0 -fno-omit-frame-pointer)

# 5. IO 类的功能性测试
add_executable(test_bench_io test_bench_io.cpp)
target_link_libraries(test_bench_io PRIVATE titankv_core)
target_compile_options(test_bench_io PRIVATE -g -O0 -fno-omit-frame-pointer)

# 6. IO 类的功能性压力测试
add_executable(test_concurrency test_concurrency.cpp)
target_link_libraries(test_concurrency PRIVATE titankv_core)
target_compile_options(test_concurrency PRIVATE -g -O0 -fno-omit-frame-pointer)

# 7. 异常恢复的功能性测试
add_executable(test_recover test_recover.cpp)
target_link_libraries(test_recover PRIVATE titankv_core)
target_compile_options(test_recover PRIVATE -g -O0 -fno-omit-frame-pointer)

# 8. 跟RocksDB的性能基准测试
add_executable(test_bench_rocks test_bench_rocks.cpp)
target_link_libraries(test_bench_rocks PRIVATE titankv_core rocksdb pthread dl z)
target_compile_options(test_bench_rocks PRIVATE -g -O0 -fno-omit-frame-pointer)

# 9. 压缩功能的功能性测试
add_executable(test_compaction test_compaction.cpp)
target_link_libraries(test_compaction PRIVATE titankv_core)
target_compile_options(test_compaction PRIVATE -g -O0 -fno-omit-frame-pointer)

# 10. 基于ebpf与rocksdb的性能基准测试
add_executable(bench_rocksdb_standalone bench_rocksdb_standalone.cpp)
target_link_libraries(bench_rocksdb_standalone PRIVATE titankv_core rocksdb pthread dl z)
target_compile_options(bench_rocksdb_standalone PRIVATE -g -O0 -fno-omit-frame-pointer)
add_executable(bench_titankv_standalone bench_titankv_standalone.cpp)
target_link_libraries(bench_titankv_standalone PRIVATE titankv_core)
target_compile_options(bench_titankv_standalone PRIVATE -g -O0 -fno-omit-frame-pointer)
add_executable(bench_titankv_standalone_v2 bench_titankv_standalone_v2.cpp)
target_link_libraries(bench_titankv_standalone_v2 PRIVATE titankv_core)
target_compile_options(bench_titankv_standalone_v2 PRIVATE -g -O0 -fno-omit-frame-pointer)