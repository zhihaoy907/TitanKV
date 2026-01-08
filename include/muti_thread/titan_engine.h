#pragma once

#include <vector>
#include <memory>
#include <immintrin.h>
#include <functional>
#include <cstring>

#include "core_worker.h"
#include "storage/log_entry.h"
#include "muti_thread/common.h"

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

    void write(AlignedBuffer&& buf, off_t offset, std::function<void(int)> cb)
    {
        static std::atomic<size_t> idx{0};
        size_t current = idx.fetch_add(1, std::memory_order_relaxed) % workers_.size();

        WriteRequest req{std::move(buf), offset, std::move(cb)};

        workers_[current]->submit(std::move(req));
    }

    void Put(std::string_view key, std::string_view val, std::function<void(int)> on_complete)
    {
        size_t worker_idx = std::hash<std::string_view>{}(key) % workers_.size();
        size_t total_size = LogRecord::size_of(key, val);
        // 强制字节对齐
        size_t aligned_size = (total_size + 4095) & ~4095;
        AlignedBuffer buffer(aligned_size);
        std::memset(buffer.data(), 0, aligned_size);

        LogRecord::encode(key, val, LogOp::PUT, {buffer.data(), buffer.size()});

        WriteRequest req(std::move(buffer), 0, std::move(on_complete));

        workers_[worker_idx]->submit(std::move(req));
    }

    void Get(std::string_view key, std::function<void(std::string)> on_complete)
    {
        size_t worker_idx = std::hash<std::string_view>{}(key) % workers_.size();

        ReadRequest req(key, std::move(on_complete));

        workers_[worker_idx]->submit(std::move(req));
    }

private:
    std::vector<std::unique_ptr<CoreWorker>> workers_;
};


TITANKV_NAMESPACE_CLOSE