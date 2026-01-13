#pragma once
#include <functional>
#include <utility>
#include <string>
#include <thread>

#include "common/buffer.h"
#include "common/common.h"
#include "storage/log_entry.h"

TITANKV_NAMESPACE_OPEN

class AlignedBuffer;

// 最大线程数量
static unsigned default_thread_num = std::thread::hardware_concurrency();

struct WriteRequest 
{
    AlignedBuffer buf;
    std::string key;
    off_t offset;
    LogOp type;
    std::function<void(int)> callback;

    // 仅是多线程测试留的接口
    WriteRequest(AlignedBuffer&& b, off_t o, std::function<void(int)> cb)
    :buf(std::move(b)), offset(o), callback(std::move(cb))
    {}

    // 委托构造函数，委托下面的构造函数来初始化
    WriteRequest(AlignedBuffer&& b, std::string k, off_t o, std::function<void(int)> cb)
    :WriteRequest(std::move(b), std::move(k), o, LogOp::PUT, std::move(cb))
    {}

    WriteRequest(AlignedBuffer&& b, std::string k, off_t o, LogOp op, std::function<void(int)> cb)
    :buf(std::move(b)), key(std::move(k)), offset(o), type(op), callback(std::move(cb))
    {}

    WriteRequest() = default;
};

struct ReadRequest 
{
    std::string key;
    std::function<void(std::string)> callback; 

    ReadRequest(std::string &k, std::function<void(std::string)> cb)
    : key(k), callback(cb)
    {}
};


TITANKV_NAMESPACE_CLOSE