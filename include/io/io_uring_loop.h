#pragma once
#include <liburing.h>
#include <functional>

#include "common/buffer.h"
#include "common/common.h"
#include "common/object_pool.h"

TITANKV_NAMESPACE_OPEN

using WriteCompletionCallback = std::function<void(int bytes_transferred)>;
using ReadCompletionCallback = std::function<void(int, AlignedBuffer&)>;

struct IoRequest
{
    struct iovec iov;
    off_t offset;
    WriteCompletionCallback write_cb;
    ReadCompletionCallback read_cb;
    AlignedBuffer held_buffer;

    IoRequest() : held_buffer() 
    {}
};

struct IoArena 
{
    void* ptr;
    size_t size;
    std::atomic<size_t> offset;

    IoArena(size_t sz) : size(sz), offset(0) 
    {
        // 申请 HugePage
        ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
        if (ptr == MAP_FAILED) throw std::runtime_error("Arena mmap failed");
        
        // 预热内存
        memset(ptr, 0, size); 
    }

    void* alloc(size_t sz) 
    {
        size_t off = offset.fetch_add(sz);
        if (off + sz > size) return nullptr;
        return (char*)ptr + off;
    }
};

class IoContext
{
public:
    explicit IoContext(unsigned entries = 1024);

    ~IoContext();

    void SubmitWrite(int fd, 
                    AlignedBuffer&& buf, 
                    off_t offset, 
                    WriteCompletionCallback cb);

    void SubmitRead(int fd, 
                    AlignedBuffer&& buf, 
                    off_t offset, 
                    size_t len, 
                    ReadCompletionCallback cb);

    void RunOnce();

    void Submit();

    void Drain();

    void RegisterFiles(const std::vector<int>& fds);

private:
    struct io_uring ring_;
    ObjectPool<IoRequest> request_pool_; 
    void* arena_ptr_ = nullptr;
    size_t arena_size_ = 0;
};

TITANKV_NAMESPACE_CLOSE