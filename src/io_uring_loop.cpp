#include <cstring>

#include "io/io_uring_loop.h"

using namespace titankv;

IoContext::IoContext(unsigned entries)
{
    struct io_uring_params params;
    memset(&params, 0, sizeof(params));

    // 开启内核轮询(SQPOLL),如果机器内核不支持，先注释
    params.flags |= IORING_SETUP_SQPOLL;

    int ret = io_uring_queue_init_params(entries, &ring_, &params);
    if(ret < 0)
        throw std::runtime_error("io_uring init failed");
}

IoContext::~IoContext()
{
    io_uring_queue_exit(&ring_);
}


void IoContext::SubmitWrite(int fd, const AlignedBuffer& buf, off_t offset, IoCompletionCallback cb) 
{
    // 尝试获取 SQE
    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
    
    // 忙等待处理：如果队列满了 (返回 nullptr)
    while (!sqe) 
    {
        // 让 io_uring自己的 sqpoll 来自动处理
        // io_uring_submit(&ring_);

        // 尝试释放 sq
        RunOnce();
        
        // 再次尝试获取
        sqe = io_uring_get_sqe(&ring_);
    }

    auto* req = request_pool_.alloc();
    req->iov = {};
    req->offset = offset;
    req->callback = std::move(cb);
    req->iov.iov_base = buf.data();
    req->iov.iov_len = buf.size();

    io_uring_prep_writev(sqe, fd, &req->iov, 1, offset);
    io_uring_sqe_set_data(sqe, req);
    
    // 同上
    // io_uring_submit(&ring_);
}

void IoContext::RunOnce()
{
    struct io_uring_cqe* cqe;
    
    while(io_uring_peek_cqe(&ring_, &cqe) == 0)
    {
        auto* req = reinterpret_cast<IoRequest*>(io_uring_cqe_get_data(cqe));

        if(req && req->callback)
            req->callback(cqe->res);

        request_pool_.free(req);
        io_uring_cqe_seen(&ring_, cqe);
    }
}
