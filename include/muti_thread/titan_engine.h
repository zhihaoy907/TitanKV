#pragma once

#include <vector>
#include <memory>
#include <immintrin.h>
#include "core_worker.h"

TITANKV_NAMESPACE_OPEN

class TitanEngine
{
public:
    TitanEngine(const std::string& file_path, unsigned thread_num)
    {
        workers_.reserve(thread_num);
        for(unsigned i = 0; i < thread_num; ++i)
            workers_.emplace_back(std::make_unique<CoreWorker>(i, file_path));
        
        for(auto& w:workers_)
            w->start();
    }

    ~TitanEngine()
    {
        for(auto& w:workers_)
            w->stop();
    }

    void write(const AlignedBuffer& buf, off_t offset, std::function<void(int)> cb)
    {
        static std::atomic<size_t> idx{0};
        size_t current = idx.fetch_add(1, std::memory_order_relaxed) % workers_.size();

        WriteRequest req{buf, offset, std::move(cb)};

        while(!workers_[current]->submit(std::move(req)))
        {
            // 优化忙等待,充分利用后续的超线程
            #if defined(__x86_64__)
            // 该函数只在x86有效，后续也不打算拓展到其他架构
            _mm_pause();
            #endif
        }
    }

private:
    std::vector<std::unique_ptr<CoreWorker>> workers_;
};


TITANKV_NAMESPACE_CLOSE