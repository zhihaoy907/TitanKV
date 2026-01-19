#include <cstring>
#include <thread>
#include <iostream>

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
    // params.flags |= IORING_SETUP_IOPOLL;

    int ret = io_uring_queue_init_params(entries, &ring_, &params);
    if(ret < 0) throw std::runtime_error("io_uring init failed");

    // 分配统一的 Registered Arena
    arena_size_ = 128 * 1024 * 1024; 
    arena_ptr_ = mmap(nullptr, arena_size_, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
    
    if (arena_ptr_ == MAP_FAILED) 
    {
        // 如果大页失败，用普通对齐内存
        if (posix_memalign(&arena_ptr_, 4096, arena_size_) != 0)
            throw std::runtime_error("Arena allocation failed");
    }

    // 向内核注册这块内存
    struct iovec iov;
    iov.iov_base = arena_ptr_;
    iov.iov_len = arena_size_;
    
    // 这里的 1 代表我们注册了一个大块。以后在 SQE 里传 index 0。
    ret = io_uring_register_buffers(&ring_, &iov, 1);
    if (ret < 0) throw std::runtime_error("io_uring_register_buffers failed");

    // 初始化 request_pool_
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


void IoContext::SubmitWrite(int /* fd */, AlignedBuffer&& buf, off_t offset, WriteCompletionCallback cb) 
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
    req->write_cb = std::move(cb);
    req->held_buffer = std::move(buf);

    io_uring_prep_write_fixed(sqe, 0, req->held_buffer.data(), req->held_buffer.size(), offset, 0);
    sqe->flags |= IOSQE_FIXED_FILE;
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
    }
}

void IoContext::RegisterFiles(const std::vector<int>& fds) 
{
    if (fds.empty()) return;

    // io_uring_register_files 会将用户态 FD 数组同步到内核的固定表
    int ret = io_uring_register_files(&ring_, fds.data(), fds.size());
    if (ret < 0) 
    {
        fprintf(stderr, "io_uring_register_files failed: %s\n", strerror(-ret));
        throw std::runtime_error("Fixed files registration failed");
    }
}