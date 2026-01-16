#include "muti_thread/core_worker.h"
#include "storage/log_entry.h"
#include "storage/log_reader.h"

#include <optional>
#include <immintrin.h>
#include <filesystem>
#include <sys/stat.h>


using namespace titankv;

CoreWorker::CoreWorker(unsigned core_id, const std::string& file_path)
: stop_(false), core_id_(core_id), ctx_(4096)
{    
    filename_ = file_path + "/data_" + std::to_string(core_id) + ".log";
    device_ = std::make_unique<RawDevice>(filename_);
    fd_ = device_->fd();

    current_offset_ = 0;

// SPSC架构下的初始化，MPSC会按需分配
#ifndef TITAN_USE_MPSC
    write_queue_ = std::make_unique<rigtorp::SPSCQueue<WriteRequest>>(QUEUE_CAPACITY);
    read_queue_ = std::make_unique<rigtorp::SPSCQueue<ReadRequest>>(QUEUE_CAPACITY);
#endif

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
        
        this->recover();

        this->run();

    });
}

void CoreWorker::recover()
{
    LogReader reader(filename_);
    LogHeader header;
    std::string key;
    uint64_t offset;
    struct stat st;

    while(reader.Next(header, key, offset))
    {
        size_t real_len = sizeof(LogHeader) + header.key_len + header.val_len;
        size_t aligned_len = (real_len + 4095) & ~4095;
        if (header.type == LogOp::PUT)
        {
            index_[key] = KeyLocation{offset, (uint32_t)real_len};
        }
        else if(header.type == LogOp::DELETE)
        {
            index_.erase(key);
        }
        current_offset_ = offset + aligned_len;
    }

    if(fstat(fd_, &st) != 0)
    {
        std::cerr << "fstat failed: " << std::strerror(errno) << std::endl;
        std::exit(EXIT_FAILURE);
    }

    if(reader.GetValidEndOffset() < static_cast<uint64_t>(st.st_size))
    {
        ftruncate(device_->fd(), reader.GetValidEndOffset());
        current_offset_ = reader.GetValidEndOffset();
    }

}

void CoreWorker::stop()
{
    stop_ = true;
    if(thread_.joinable())
        thread_.join();
}

std::string CoreWorker::ExtractValue(const AlignedBuffer& buf, [[maybe_unused]] uint32_t len)
{
    const char* ptr = (const char*)buf.data();
    auto* header = reinterpret_cast<const LogHeader*>(ptr);
    ptr += sizeof(LogHeader);

    ptr += header->key_len;

    return std::string(ptr, header->val_len);
}

void  CoreWorker::submit(WriteRequest req)
{
    enqueue_blocking(write_queue_, std::move(req));
}

void  CoreWorker::submit(ReadRequest req)
{
    enqueue_blocking(read_queue_, std::move(req));
}

template <typename Q, typename T>
void CoreWorker::enqueue_blocking(Q& queue, T&& item)
{
#ifdef TITAN_USE_MPSC
    while (!queue.enqueue(item))
    {
#if defined(__x86_64__)
        _mm_pause();
#endif
    }
#else
    while (!queue->try_push(item))
    {
#if defined(__x86_64__)
        _mm_pause();
#endif
    }
#endif
}


// 直接从index中写入，也能从硬盘中读取再写入。
// 这取决于你使用的硬盘到底是机械硬盘还是固态硬盘，对于机械硬盘来说，由于log文件大概率是连续的而index的存储大概率不是联系的，所以性能瓶颈在磁盘扫盘中。
// 因为我们想要探索在现代硬件的瓶颈，因此不认为扫盘是瓶颈，从而直接从index读取写入
void CoreWorker::RewriteFile()
{
#ifdef TITAN_USE_MPSC
    while (write_queue_.size_approx() > 0) 
        std::this_thread::yield();
#else
    while (!write_queue_->empty()) 
        std::this_thread::yield();
#endif

    std::string tmp_file = filename_ + ".tmp";

    int new_fd = ::open(tmp_file.c_str(), O_RDWR | O_CREAT | O_TRUNC | O_DIRECT, 0644);
    if(new_fd < 0)
        return;

    uint64_t new_offset = 0;

    AlignedBuffer tmp_buf(4096);

    for(auto& kv : index_)
    {
        KeyLocation& loc = kv.second;

        size_t aligned_len = (loc.len + 4095) & ~4095;

        if(aligned_len > tmp_buf.size())
        {
            tmp_buf = AlignedBuffer(aligned_len);
        }

        ssize_t n = ::pread(device_->fd(), tmp_buf.data(), aligned_len, loc.offset);
        if(n != (ssize_t)aligned_len)
            continue;

        n = ::pwrite(new_fd, tmp_buf.data(), aligned_len, new_offset);

        loc.offset = new_offset;
        new_offset += aligned_len;
    }
    ::close(new_fd);

    ::rename(tmp_file.c_str(), filename_.c_str());
    device_ = std::make_unique<RawDevice>(filename_);
    fd_ = device_->fd();
    current_offset_ = new_offset;
}

void CoreWorker::run() 
{
    // 预分配内存，避免热路径上的扩容
    std::vector<WriteRequest> write_batch;
    write_batch.reserve(URING_CQ_BATCH);

    std::vector<ReadRequest> read_batch;
    read_batch.reserve(URING_CQ_BATCH);

    while(!stop_)
    {
        ctx_.RunOnce();

        // 捞取写请求
#ifdef TITAN_USE_MPSC
        size_t write_count = write_queue_.try_dequeue_bulk(std::back_inserter(write_batch), URING_CQ_BATCH);
#else
        size_t write_count = 0;
        while(write_count < URING_CQ_BATCH)
        {
            auto* req = write_queue_->front();
            if(!req) break;

            write_batch.emplace_back(std::move(*req));
            write_queue_->pop();
            write_count++;
        }
#endif
        // -------------------------------------------------------
        // 批量处理写请求
        // -------------------------------------------------------
        if(write_count > 0)
        {
            size_t raw_size = 0;
            for(const auto& req : write_batch)
                raw_size += req.buf.size();

            size_t aligned_size = (raw_size + 4095) & ~4095;
            AlignedBuffer group_buf(aligned_size);

            size_t current_offset_in_group = 0;

            auto callbacks = std::make_shared<std::vector<std::function<void(int)>>>();
            callbacks->reserve(write_batch.size());
            off_t write_pos_start = current_offset_;

            for(auto& req : write_batch)
            {
                memcpy(group_buf.data() + current_offset_in_group, req.buf.data(), req.buf.size());

                // 更新内存索引
                if (req.type == LogOp::PUT)
                {
                    index_[std::move(req.key)] = KeyLocation{ (uint64_t)(write_pos_start + current_offset_in_group), (uint32_t)req.buf.size() };
                }
                else if(req.type == LogOp::DELETE)
                {
                    index_.erase(req.key);
                }

                if(req.callback)
                    callbacks->push_back(std::move(req.callback));
                
                current_offset_in_group += req.buf.size();
            }
            current_offset_ += aligned_size;
            ctx_.SubmitWrite(device_->fd(), 
                            std::move(group_buf),  
                            write_pos_start, 
                            [cb_list = callbacks](int res) {
                                for (auto& cb : *cb_list) cb(res);
                            });

            write_batch.clear();
        }

        // 捞取读请求
#ifdef TITAN_USE_MPSC
        size_t read_count = read_queue_.try_dequeue_bulk(std::back_inserter(read_batch), URING_CQ_BATCH);
#else
        size_t read_count = 0;
        while(read_count < URING_CQ_BATCH) 
        {
            auto* req = read_queue_->front();
            if(!req) break;
            read_batch.emplace_back(std::move(*req));
            read_queue_->pop();
            read_count++;
        }
#endif

        // -------------------------------------------------------
        // 批量处理读请求
        // -------------------------------------------------------
        if(read_count > 0)
        {
            for(auto& req : read_batch)
            {
                auto it = index_.find(req.key);

                if(it != index_.end()) [[likely]]
                {
                    KeyLocation loc = it->second;
                    size_t aligned_len = (loc.len + 4095) &~ 4095;
                    AlignedBuffer buf(aligned_len);
                    auto user_cb = std::move(req.callback);

                    ctx_.SubmitRead(device_->fd(),
                                    std::move(buf),
                                    loc.offset,
                                    aligned_len,
                                    [this, user_cb, loc](int res, AlignedBuffer& data_buf) 
                                    {
                                        if (res < 0) 
                                        {
                                            user_cb("");
                                        } 
                                        else 
                                        {
                                            // 反序列化
                                            std::string value = ExtractValue(data_buf, loc.len);
                                            user_cb(std::move(value));
                                        }
                                    });
                }
                else [[unlikely]]
                {
                    if(req.callback)
                        req.callback(""); 
                }
            }
            read_batch.clear();
        }


        if (write_count > 0 || read_count > 0) 
        {
            ctx_.Submit(); 
        } 
        else 
        {
            // #if defined(__x86_64__)
            // _mm_pause();
            // #endif
            std::this_thread::yield(); 
        }
    }
}
