#include "muti_thread/muti_thread.h"

#include <optional>


using namespace titankv;

MutiThread::MutiThread(const RawDevice& device)
: device_(device), ctx_(4096), stop_(false)
{}
 
void MutiThread::start(unsigned core_id)
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


// void MutiThread::run() 
// {
//     while(!stop_) 
//     {
//         unsigned count = 0;
//         // 每64次提交调用一次系统接口批处理
//         {
//             std::lock_guard<std::mutex> lock(queue_mutex_);
//             while(!queue_.empty() && count < URING_CQ_BATCH) 
//             {
//                 auto req = std::move(queue_.front());
//                 queue_.pop();
//                 // 仅填入 SQ Ring，不立即 syscall
//                 ctx_.SubmitWrite(device_.fd(), req.buf, req.offset, req.callback);
//                 count++;
//             }
//         }

//         ctx_.RunOnce(); 
        
//         if(count == 0) 
//         {
//             // 没任务时让出 CPU
//             std::this_thread::yield(); 
//         }
//     }
// }

void MutiThread::run() 
{
    // 本地缓存队列，减少持锁时间
    std::vector<WriteRequest> local_batch; 
    local_batch.reserve(URING_CQ_BATCH);

    while(!stop_) 
    {
        bool busy = false;
        // 只负责从 std::queue 搬运指针
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            while(!queue_.empty() && local_batch.size() < URING_CQ_BATCH) 
            {
                local_batch.emplace_back(std::move(queue_.front()));
                queue_.pop();
            }
        }

        // 填装 SQE (IoUring Prep)
        if(!local_batch.empty())
        {
            busy = true;
            for(auto& req : local_batch)
            {
                // 确保 SubmitWrite 只做 io_uring_prep_write，不要调用 submit
                ctx_.SubmitWrite(device_.fd(), std::move(req.buf), req.offset, req.callback);
            }
            
            // 批量填装完后，调用一次系统调用
            ctx_.Submit(); 

            // 清空本地缓存，为下一轮做准备
            local_batch.clear();
        }

        // 处理完成事件
        ctx_.RunOnce();
        
        // 策略优化：如果没活干，适当让出 CPU
        if(!busy) 
        {
            std::this_thread::yield(); 
        }
    }
}