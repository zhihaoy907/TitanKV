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

    write_queue_ = std::make_unique<rigtorp::SPSCQueue<WriteRequest>>(QUEUE_CAPACITY);
    read_queue_ = std::make_unique<rigtorp::SPSCQueue<ReadRequest>>(QUEUE_CAPACITY);
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
void  CoreWorker::enqueue_blocking(Q& queue, T&& item)
{
    while (!queue->try_push(std::move(item))) 
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

        unsigned count = 0;

        // -------------------------------------------------------
        // 批量处理写请求
        // -------------------------------------------------------
        while(count < URING_CQ_BATCH)
        {
            auto* req = write_queue_->front();
            if(!req)
                break;

            off_t write_pos = current_offset_;
            current_offset_ += req->buf.size();
            uint32_t entry_size = req->buf.size();
            // 更新内存索引
            if (req->type == LogOp::PUT)
            {
                index_[std::move(req->key)] = KeyLocation{ (uint64_t)write_pos, entry_size };
            }
            else if(req->type == LogOp::DELETE)
            {
                index_.erase(req->key);
            }

            ctx_.SubmitWrite(device_->fd(), std::move(req->buf), write_pos, std::move(req->callback));

            write_queue_->pop();
            count++;
        }

        // -------------------------------------------------------
        // 批量处理读请求
        // -------------------------------------------------------
        while(count < URING_CQ_BATCH)
        {
            auto* req = read_queue_->front();
            if(!req)
                break;
                        
            auto it = index_.find(req->key);
            
            if(it != index_.end()) [[likely]]
            {
                KeyLocation loc = it->second;
                size_t aligned_len = (loc.len + 4095) &~ 4095;
                AlignedBuffer buf(aligned_len);
                auto user_cb = std::move(req->callback);

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
                count++;
            }
            else [[unlikely]]
            {
                if(req->callback)
                    req->callback(""); 
            }
            read_queue_->pop();
        }


        if (count > 0) 
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