#pragma once
#include <functional>
#include <utility>

#include "common/common.h"

TITANKV_NAMESPACE_OPEN

class AlignedBuffer;

// 最大线程数量
static unsigned default_thread_num = std::thread::hardware_concurrency();

struct WriteRequest 
{
    AlignedBuffer buf;
    std::string key;
    off_t offset;
    std::function<void(int)> callback;

    // 仅是多线程测试留的接口
    WriteRequest(AlignedBuffer&& b, off_t o, std::function<void(int)> cb)
    :buf(std::move(b)), offset(o), callback(std::move(cb))
    {}

    WriteRequest(AlignedBuffer&& b, std::string k, off_t o, std::function<void(int)> cb)
    :buf(std::move(b)), key(std::move(k)), offset(o), callback(std::move(cb))
    {}

    WriteRequest() = default;
};

struct ReadRequest 
{
    std::string key;
    std::function<void(std::string)> callback; 
};


TITANKV_NAMESPACE_CLOSE