#pragma once
#include <liburing.h>
#include <functional>

#include "common/buffer.h"
#include "common/common.h"
#include "common/object_pool.h"

TITANKV_NAMESPACE_OPEN

using IoCompletionCallback = std::function<void(int bytes_transferred)>;

struct IoRequest
{
    struct iovec iov;
    off_t offset;
    IoCompletionCallback callback;
};

class IoContext
{
public:
    explicit IoContext(unsigned entries = 1024);

    ~IoContext();

    void SubmitWrite(int fd, 
                    const AlignedBuffer &buf, 
                    off_t offset, 
                    IoCompletionCallback cb);

    void RunOnce();

private:
    struct io_uring ring_;
    ObjectPool<IoRequest> request_pool_; 
};

TITANKV_NAMESPACE_CLOSE