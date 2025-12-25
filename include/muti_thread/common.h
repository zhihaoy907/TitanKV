#pragma once
#include <functional>

#include "common/common.h"

TITANKV_NAMESPACE_OPEN

class AlignedBuffer;

// 最大线程数量
static unsigned default_thread_num = std::thread::hardware_concurrency();

struct WriteRequest 
{
    const AlignedBuffer& buf;
    off_t offset;
    std::function<void(int)> callback;
};


TITANKV_NAMESPACE_CLOSE