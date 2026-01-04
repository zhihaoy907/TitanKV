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
    off_t offset;
    std::function<void(int)> callback;

    WriteRequest(AlignedBuffer&& b, off_t o, std::function<void(int)> cb)
    :buf(std::move(b)), offset(o), callback(std::move(cb))
    {}

    WriteRequest() = default;
};


TITANKV_NAMESPACE_CLOSE