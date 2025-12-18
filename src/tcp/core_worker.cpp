#include "tcp/core_worker.h"

using namespace titankv;

CoreWorker::CoreWorker(int core_id, const std::string& file_path)
: core_id_(core_id), device_(file_path), ctx_(4096), stop_(false)
{}

void CoreWorker::start()
{
    thread_ = std::thread([this]{ this->run(); });
}

void CoreWorker::stop()
{
    stop_ = true;
    if(thread_.joinable()) 
        thread_.join();
}

void CoreWorker::submit(WriteRequest req) 
{
    queue_.push(std::move(req));
}

void CoreWorker::run() 
{
    while(!stop_) 
    {
        // 处理本线程队列
        while(!queue_.empty()) 
        {
            WriteRequest req = std::move(queue_.front());
            queue_.pop();
            ctx_.SubmitWrite(device_.fd(), req.buf, req.offset, req.callback);
        }

        // 批量消费CQE
        ctx_.RunOnce();
        
        // 阻塞或 yield 避免CPU忙等
        std::this_thread::yield();
    }
}