#include "muti_thread/core_worker.h"

#include <optional>
#include <immintrin.h>
#include <filesystem>


using namespace titankv;

CoreWorker::CoreWorker(unsigned core_id, const std::string& file_path)
: core_id_(core_id), ctx_(4096), stop_(false)
{    
    std::string filename = file_path + "/data_" + std::to_string(core_id) + ".log";
    device_ = std::make_unique<RawDevice>(filename);

    current_offset_ = 0;

    queue_ = std::make_unique<rigtorp::SPSCQueue<WriteRequest>>(URING_CQ_BATCH);
}
 
void CoreWorker::start()
{
    if(thread_.joinable())
        thread_.join();    

    thread_ = std::thread([this]{ 
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(core_id_, &cpuset);

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

void  CoreWorker::submit(WriteRequest req)
{
    while (!queue_->try_push(std::move(req))) 
    {
        #if defined(__x86_64__)
        _mm_pause();
        #endif
    }
}

void CoreWorker::run() 
{
    while(!stop_)
    {
        ctx_.RunOnce();

        while(auto* req = queue_->front())
        {
            off_t write_pos = current_offset_;

            ctx_.SubmitWrite(device_->fd(), std::move(req->buf), write_pos, [req](int res)
                {
                    if(req->callback)
                        req->callback(res);
                });
            current_offset_ += req->buf.size();
            queue_->pop();
        }
    }
}