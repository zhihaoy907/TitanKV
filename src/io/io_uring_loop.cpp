#include <cstring>
#include <thread>

#include "io/io_uring_loop.h"

using namespace titankv;


IoContext::IoContext(unsigned entries)
{
    struct io_uring_params params;
    memset(&params, 0, sizeof(params));

    // 开启内核轮询(SQPOLL),如果机器内核不支持，先注释
    // 针对资源比较紧张的情况下先注释掉，
    // 减少内核线程抢占用户态线程，增加测试稳定性
    // params.flags |= IORING_SETUP_SQPOLL;

    int ret = io_uring_queue_init_params(entries, &ring_, &params);
    if(ret < 0) [[unlikely]]
        throw std::runtime_error("io_uring init failed");

    request_pool_.reserve(entries); 
}

IoContext::~IoContext()
{
    io_uring_queue_exit(&ring_);
}

void IoContext::SubmitRead(int fd, AlignedBuffer&& buf, off_t offset, size_t len, ReadCompletionCallback cb)
{
    // 尝试获取 SQE
    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
    
    // 忙等待处理：如果队列满了 (返回 nullptr)
    while (!sqe) 
    {
        // 尝试释放 sq
        RunOnce();
        
        // 再次尝试获取
        sqe = io_uring_get_sqe(&ring_);
    }

    auto* req = request_pool_.alloc();
    req->held_buffer = std::move(buf);
    req->iov.iov_base = req->held_buffer.data();
    req->iov.iov_len  = len;
    assert(req->iov.iov_base != nullptr);

    req->offset = offset;
    req->read_cb = std::move(cb);

    io_uring_prep_readv(sqe, fd, &req->iov, 1, offset);
    io_uring_sqe_set_data(sqe, req);
    
    io_uring_submit(&ring_);
}


void IoContext::SubmitWrite(int fd, AlignedBuffer&& buf, off_t offset, WriteCompletionCallback cb) 
{
    // 尝试获取 SQE
    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
    
    // 忙等待处理：如果队列满了 (返回 nullptr)
    while (!sqe) 
    {
        // 尝试释放 sq
        RunOnce();
        
        // 再次尝试获取
        sqe = io_uring_get_sqe(&ring_);
    }

    auto* req = request_pool_.alloc();
    req->iov = {};
    req->offset = offset;
    req->write_cb = std::move(cb);
    req->iov.iov_base = buf.data();
    req->iov.iov_len = buf.size();
    req->held_buffer = std::move(buf);
    assert(req->iov.iov_base != nullptr);

    io_uring_prep_writev(sqe, fd, &req->iov, 1, offset);
    io_uring_sqe_set_data(sqe, req);
    
    // 还是需要提交
    io_uring_submit(&ring_);
}

// 批处理
void IoContext::RunOnce()
{
    io_uring_cqe* cqes[URING_CQ_BATCH];

    unsigned count = io_uring_peek_batch_cqe(&ring_, cqes, URING_CQ_BATCH);

    for(unsigned i = 0; i < count; ++i)
    {
        io_uring_cqe* rqe = cqes[i];

        auto* req = static_cast<IoRequest*>(io_uring_cqe_get_data(rqe));

        if(req) 
        {
            if (req->read_cb) 
            {
                req->read_cb(rqe->res, req->held_buffer);
            } 
            else if (req->write_cb) 
            {
                req->write_cb(rqe->res);
            }
        }

        request_pool_.free(req);
    }

    if(count > 0)
        io_uring_cq_advance(&ring_, count);
}

// 手动触发系统调用
void IoContext::Submit() 
{
    io_uring_submit(&ring_);
}

void IoContext::Drain() 
{
    while (request_pool_.in_use() > 0) 
    {
        io_uring_submit(&ring_);
        RunOnce();
        std::this_thread::yield();
    }
}
