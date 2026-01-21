#pragma once

// ==========================================
// 全局配置与宏定义
// ==========================================

// 1. 定义核心命名空间名称（Single Source of Truth）
#define TITANKV_NS titankv

// 2. 命名空间开启宏
#define TITANKV_NAMESPACE_OPEN namespace TITANKV_NS {

// 3. 命名空间关闭宏
#define TITANKV_NAMESPACE_CLOSE } 


TITANKV_NAMESPACE_OPEN

struct IoCallback 
{
    void (*fn)(int res, void* ctx);
    void* ctx;
};

// 默认cache line字节数
constexpr unsigned CACHE_LINE_SIZE = 64;
// 批量处理的cq数量
constexpr unsigned URING_CQ_BATCH = 64;
// object pool 默认大小
constexpr unsigned DEFAULT_OBJECT_SIZE = 128;
// SPSC 队列容量
constexpr unsigned QUEUE_CAPACITY = 4096;

#ifdef __has_cpp_attribute
#if __has_cpp_attribute(nodiscard)
#define TITANKV_NODISCARD [[nodiscard]]
#endif
#endif


TITANKV_NAMESPACE_CLOSE
