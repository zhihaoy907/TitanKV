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

private:
    struct io_uring ring_;
    ObjectPool<IoRequest> request_pool_; 
};

TITANKV_NAMESPACE_CLOSE