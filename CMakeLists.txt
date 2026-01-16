cmake_minimum_required(VERSION 3.15)

project(TitanKV
    VERSION 0.1.0
    DESCRIPTION "High-performance io_uring KV Store"
    LANGUAGES CXX
)

# ==========================================
# 1. 基础配置 (Common Setup)
# ==========================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) 

# 通用基础警告 (对所有版本生效)
add_compile_options(-Wall -Wextra -pedantic)
# add_compile_options(-Wall -Wextra -Werror -pedantic)
# io_uring O_DIRECT 必需宏
add_definitions(-D_GNU_SOURCE) 

# 默认关闭 MPSC，使用原来的 SPSC 架构
option(USE_MPSC_QUEUE "Use Lock-free MPSC queue instead of SPSC" OFF)

if(USE_MPSC_QUEUE)
    add_definitions(-DTITAN_USE_MPSC)
    message(STATUS "Build Strategy: MPSC (Lock-free, Multi-Producer)")
else()
    message(STATUS "Build Strategy: SPSC (Single-Producer, requires external locking)")
endif()

# ==========================================
# 2. 依赖查找 (LibUring)
# ==========================================
find_package(PkgConfig)
pkg_check_modules(LIBURING liburing)

if (NOT LIBURING_FOUND)
    message(STATUS "PkgConfig not found, trying find_library...")
    find_library(URING_LIB NAMES uring)
    find_path(URING_INCLUDE_DIR NAMES liburing.h)
    
    if (URING_LIB AND URING_INCLUDE_DIR)
        set(LIBURING_LIBRARIES ${URING_LIB})
        set(LIBURING_INCLUDE_DIRS ${URING_INCLUDE_DIR})
        message(STATUS "Found liburing: ${URING_LIB}")
    else()
        message(FATAL_ERROR "liburing not found! install: sudo apt install liburing-dev")
    endif()
endif()

# ==========================================
# 3. 定义源文件 (Source Set)
# ==========================================
# 将源文件保存到变量，方便给两个 Target 复用
set(KV_SOURCES
    src/io/raw_device.cpp
    src/io/io_uring_loop.cpp
    src/muti_thread/muti_thread.cpp
    src/muti_thread/core_worker.cpp
    src/storage/log_record.cpp
    src/storage/log_reader.cpp
)

# ==========================================
# 4. 构建核心库 (两个版本)
# ==========================================

# ------------------------------------------
# A. 正常版本 (Release / Optimized)
# ------------------------------------------
add_library(titankv_core STATIC ${KV_SOURCES})

# 编译选项：优化满上
target_compile_options(titankv_core PRIVATE -O0 -march=native)
# target_compile_options(titankv_core PRIVATE -O3 -march=native)

# 头文件路径
target_include_directories(titankv_core PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    ${LIBURING_INCLUDE_DIRS}
)
target_link_libraries(titankv_core PUBLIC ${LIBURING_LIBRARIES})

# ------------------------------------------
# B. Debug 版本 (With ASan)
# ------------------------------------------
add_library(titankv_core_debug STATIC ${KV_SOURCES})

# 编译选项：开启调试符号 + AddressSanitizer + 禁止优化帧指针
target_compile_options(titankv_core_debug PRIVATE 
    -g -O0 
    -fno-omit-frame-pointer 
    -fsanitize=address,undefined
)

# 链接选项：链接 ASan 运行时
target_link_options(titankv_core_debug PUBLIC -fsanitize=address,undefined)

# 头文件路径
target_include_directories(titankv_core_debug PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    ${LIBURING_INCLUDE_DIRS}
)
target_link_libraries(titankv_core_debug PUBLIC ${LIBURING_LIBRARIES})

# ==========================================
# 5. 子目录
# ==========================================
enable_testing()
add_subdirectory(tests)
