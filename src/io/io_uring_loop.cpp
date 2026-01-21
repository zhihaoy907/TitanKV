#include <cstring>
#include <thread>
#include <iostream>

#include "io/io_uring_loop.h"
#include "muti_thread/core_worker.h"

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

    arena_size_ = 128 * 1024 * 1024; 
    arena_ = std::make_unique<IoArena>(arena_size_);

    // 注册
    struct iovec iov;
    iov.iov_base = arena_->ptr;
    iov.iov_len = arena_->size;
    
    if (io_uring_register_buffers(&ring_, &iov, 1) < 0) 
        throw std::runtime_error("io_uring_register_buffers failed");

    request_pool_.reserve(entries); 
}

IoContext::~IoContext()
{
    io_uring_queue_exit(&ring_);
}

void IoContext::SubmitRead(int fd, AlignedBuffer&& buf, off_t offset, size_t len, void* worker, std::function<void(std::string)> cb)
{
    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
    while (!sqe) { RunOnce(); sqe = io_uring_get_sqe(&ring_); }

    auto* req = request_pool_.alloc();
    req->reset();
    req->offset = offset;
    req->worker_ptr = worker;
    req->read_len = (uint32_t)len;
    req->read_cb = std::move(cb);
    req->held_buffer = std::move(buf);

    io_uring_prep_read(sqe, fd, req->held_buffer.data(), len, offset);
    io_uring_sqe_set_data(sqe, req);
}

void IoContext::SubmitWrite(int /*fd*/, const void* data, size_t len, off_t offset, std::vector<WriteRequest>& batch) 
{
    auto* req = request_pool_.alloc();
    req->reset();
    
    auto callbacks = std::make_shared<std::vector<std::function<void(int)>>>();
    callbacks->reserve(batch.size());
    for (auto& r : batch) callbacks->push_back(std::move(r.callback));
    req->write_cbs_vec = std::move(callbacks);

    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
    while (!sqe) { RunOnce(); sqe = io_uring_get_sqe(&ring_); }

    io_uring_prep_write_fixed(sqe, 0, data, len, offset, 0); 
    sqe->flags |= IOSQE_FIXED_FILE;
    io_uring_sqe_set_data(sqe, req);
}

// 兼容性接口
void IoContext::SubmitWrite(int fd, AlignedBuffer&& buf, off_t offset, std::function<void(int)> cb) 
{
    auto* req = request_pool_.alloc();
    req->reset();
    req->held_buffer = std::move(buf);
    
    auto v = std::make_shared<std::vector<std::function<void(int)>>>();
    v->push_back(std::move(cb));
    req->write_cbs_vec = std::move(v);

    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
    while (!sqe) { RunOnce(); sqe = io_uring_get_sqe(&ring_); }

    io_uring_prep_write(sqe, fd, req->held_buffer.data(), req->held_buffer.size(), offset);
    io_uring_sqe_set_data(sqe, req);
    io_uring_submit(&ring_);
}

void IoContext::RunOnce()
{
    io_uring_cqe* cqes[URING_CQ_BATCH];
    unsigned count = io_uring_peek_batch_cqe(&ring_, cqes, URING_CQ_BATCH);

    for(unsigned i = 0; i < count; ++i)
    {
        auto* req = static_cast<IoRequest*>(io_uring_cqe_get_data(cqes[i]));
        int res = cqes[i]->res;

        if (req->read_cb) 
        {
            auto* worker = static_cast<CoreWorker*>(req->worker_ptr);
            req->read_cb(worker->ExtractValue(req->held_buffer, req->read_len));
        } 
        else if (req->write_cbs_vec) 
        {
            for (auto& cb : *req->write_cbs_vec) if (cb) cb(res);
        }

        req->reset();
        request_pool_.free(req);
    }
    if(count > 0) io_uring_cq_advance(&ring_, count);
}

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

    int ret = io_uring_register_files(&ring_, fds.data(), fds.size());
    if (ret < 0) 
    {
        fprintf(stderr, "io_uring_register_files failed: %s\n", strerror(-ret));
        throw std::runtime_error("Fixed files registration failed");
    }
}

void* IoContext::AllocateFromArena(size_t size) 
{
    return arena_->alloc(size);
}