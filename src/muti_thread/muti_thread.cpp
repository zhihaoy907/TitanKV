#include "muti_thread/muti_thread.h"

#include <optional>


using namespace titankv;

MutiThread::MutiThread(const RawDevice& device)
: device_(device), ctx_(4096), stop_(false)
{}
 
void MutiThread::start()
{
    thread_ = std::thread([this]{ this->run(); });
}

void MutiThread::stop()
{
    stop_ = true;
    if(thread_.joinable()) 
        thread_.join();
}

void MutiThread::submit(WriteRequest req) 
{
    std::lock_guard<std::mutex> lock(queue_mutex_);
    queue_.push(std::move(req));
}

void MutiThread::run() 
{
    while(!stop_) 
    {
        std::optional<WriteRequest> req;
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if(!queue_.empty())
            {
                req.emplace(std::move(queue_.front()));
                queue_.pop();
            }
        }
        
        if(req)
            ctx_.SubmitWrite(device_.fd(), req->buf, req->offset, req->callback);

        // 批量消费CQE
        ctx_.RunOnce();
        
        // 阻塞或 yield 避免CPU忙等
        if(!req)
            std::this_thread::yield();
    }
}