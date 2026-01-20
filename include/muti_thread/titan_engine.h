#pragma once

#include <vector>
#include <memory>
#include <immintrin.h>
#include <functional>
#include <cstring>
#include <mutex>

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
        
        for(auto& w : workers_) 
            w->start();
    }

    ~TitanEngine()
    {
        for(auto& w : workers_)
            w->stop();
    }

    AlignedBuffer Serialize(std::string_view key, std::string_view val, LogOp op) 
    {
        size_t total_size = sizeof(LogHeader) + key.size() + val.size();
        AlignedBuffer buffer(total_size);
        
        LogHeader* header = reinterpret_cast<LogHeader*>(buffer.data());
        header->type = op;
        header->key_len = static_cast<uint32_t>(key.size());
        header->val_len = static_cast<uint32_t>(val.size());

        uint8_t* ptr = buffer.data() + sizeof(LogHeader);
        std::memcpy(ptr, key.data(), key.size());
        std::memcpy(ptr + key.size(), val.data(), val.size());

        return buffer;
    }

    void write(AlignedBuffer&& buf, off_t offset, std::function<void(int)> cb) 
    {
        static std::atomic<size_t> idx{0};
        size_t current = idx.fetch_add(1, std::memory_order_relaxed) % workers_.size();
        workers_[current]->submit(WriteRequest(std::move(buf), offset, LogOp::PUT, std::move(cb)));
    }

    void Put(std::string_view key, std::string_view val, std::function<void(int)> on_complete)
    {
        AlignedBuffer buffer = Serialize(key, val, LogOp::PUT); 
        WriteRequest req(std::move(buffer), 0, LogOp::PUT, std::move(on_complete));

        size_t idx = std::hash<std::string_view>{}(key) % workers_.size();
        workers_[idx]->submit(std::move(req));
    }

    void Get(std::string_view key, std::function<void(std::string)> on_complete) 
    {
        size_t worker_idx = std::hash<std::string_view>{}(key) % workers_.size();
        workers_[worker_idx]->submit(ReadRequest(key, std::move(on_complete)));
    }

    void Delete(std::string_view key, std::function<void(int)> on_complete) 
    {
        auto buffer = Serialize(key, "", LogOp::DELETE);
        WriteRequest req(std::move(buffer), 0, LogOp::DELETE, std::move(on_complete));
        
        size_t idx = std::hash<std::string_view>{}(key) % workers_.size();
        workers_[idx]->submit(std::move(req));
    }

    void Compact()
    {
        for (auto& w : workers_) 
            w->RewriteFile();
    }

private:
    std::vector<std::unique_ptr<CoreWorker>> workers_;
};


TITANKV_NAMESPACE_CLOSE