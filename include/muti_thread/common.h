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
    std::string_view key;
    std::string_view val;
    off_t offset;
    LogOp type;
    std::function<void(int)> callback;

    // 仅是多线程测试留的接口
    WriteRequest(AlignedBuffer&& b, off_t o, std::function<void(int)> cb)
    :buf(std::move(b)), offset(o), callback(std::move(cb))
    {}

    WriteRequest(AlignedBuffer&& b, off_t o, LogOp op, std::function<void(int)> cb)
    : buf(std::move(b)), offset(o), type(op), callback(std::move(cb))
    {
        // 从已经序列化好的 buf 中通过指针计算还原出 Key 的视图
        const char* raw_ptr = reinterpret_cast<const char*>(buf.data());
        auto* header = reinterpret_cast<const LogHeader*>(raw_ptr);
        
        key = std::string_view(raw_ptr + sizeof(LogHeader), header->key_len);
    }

    WriteRequest(std::string_view k, std::string_view v, LogOp op, std::function<void(int)> cb)
    : key(k), val(v), type(op), callback(std::move(cb)) {}

    WriteRequest(WriteRequest&&) = default;
    WriteRequest(const WriteRequest&) = delete;
};

struct ReadRequest 
{
    std::string_view key;
    std::function<void(std::string)> callback; 

    ReadRequest(std::string_view &k, std::function<void(std::string)> cb)
    : key(k), callback(cb)
    {}

    ReadRequest(ReadRequest&&) = default;
    ReadRequest(const ReadRequest&) = delete;
};


TITANKV_NAMESPACE_CLOSE