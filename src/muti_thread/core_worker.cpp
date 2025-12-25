#include "muti_thread/core_worker.h"

#include <optional>


using namespace titankv;

CoreWorker::CoreWorker(const RawDevice& device)
: device_(device), ctx_(4096), stop_(false)
{}
 
TITANKV_NODISCARD bool CoreWorker::start(unsigned core_id)
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
    return queue_.try_push(std::move(req));
}

void CoreWorker::run() 
{
    while(!stop_) 
    {
        bool busy = false;
        while(auto *req = queue_.front())
        {
            busy = true;
            ctx_.SubmitWrite(device_.fd(), req->buf, req->offset, req->callback);

            queue_.pop();
        }
        ctx_.RunOnce();
    }
}

void CoreWorker::run() 
{
    while(!stop_) 
    {
        int count = 0;
        
        // 批量从队列取数据，但限制最大数量
        while(count < URING_CQ_BATCH)
        {
            auto* req = queue_.front();
            // 队列空了，退出内层循环
            if (!req) break;            

            // 仅仅是填入 SQE，不触发系统调用
            ctx_.SubmitWrite(device_.fd(), req->buf, req->offset, req->callback);
            
            queue_.pop();
            count++;
        }

        if (count > 0)
            ctx_.RunOnce(); 
        else
            std::this_thread::yield(); 
    }
}