#include "muti_thread/core_worker.h"

#include <optional>


using namespace titankv;

CoreWorker::CoreWorker(const RawDevice& device)
: device_(device), ctx_(4096), stop_(false)
{
    queue_ = std::make_unique<rigtorp::SPSCQueue<WriteRequest>>(URING_CQ_BATCH);
}
 
void CoreWorker::start(unsigned core_id)
{
    if(thread_.joinable())
        thread_.join();    

    thread_ = std::thread([this, core_id]{ 
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(core_id, &cpuset);

        // auto handle = thread_.native_handle();
        auto handle = pthread_self();

        int rc = pthread_setaffinity_np(handle, sizeof(cpu_set_t), &cpuset);
        if(rc != 0)
            std::cerr << "Error calling pthread_setaffinity_np: " << rc << std::endl;

        this->run();

    });
}

void CoreWorker::stop()
{
    stop_ = true;
    if(thread_.joinable())
        thread_.join();
}

TITANKV_NODISCARD bool  CoreWorker::submit(WriteRequest req) 
{
    return queue_->try_push(std::move(req));
}

void CoreWorker::run() 
{
    while(!stop_) 
    {
        // 确保 SubmitWrite 只做 io_uring_prep_write，不要调用 submit
        auto req = queue_->front();
        ctx_.SubmitWrite(device_.fd(), req->buf, req->offset, req->callback);
        queue_->pop();
            
        // 批量填装完后，调用一次系统调用
        ctx_.Submit(); 
    }

    // 处理完成事件
    ctx_.RunOnce();
}