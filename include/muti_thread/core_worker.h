#pragma once
#include <thread>
#include <vector>
#include <queue>
#include <atomic>
#include <memory>
#include <iostream>
#include <unistd.h>
#include <map>

#include "io/io_uring_loop.h"
#include "io/raw_device.h"
#include "common/buffer.h"
#include "muti_thread/SPSCQueue.h"
#include "common.h"

TITANKV_NAMESPACE_OPEN

// 内存索引：Key -> 物理位置
struct KeyLocation 
{
    // 文件中的偏移量
    uint64_t offset;
    // 数据总长度 (Header + Key + Value)
    uint32_t len;
};


class CoreWorker
{
public:
    CoreWorker(unsigned core_id, const std::string& file_path);

    void start();

    void stop();

    void submit(WriteRequest req);

    void submit(ReadRequest req);

    template <typename Q, typename T>
    void enqueue_blocking(Q& queue, T&& item);
    
private:
    void run();
    std::string ExtractValue(const AlignedBuffer& buf, uint32_t len);

    std::unique_ptr<rigtorp::SPSCQueue<ReadRequest>> read_queue_; 
    std::unique_ptr<rigtorp::SPSCQueue<WriteRequest>> write_queue_; 
    std::unique_ptr<RawDevice> device_;
    std::atomic<bool> stop_;
    std::thread thread_;
    unsigned core_id_;
    unsigned current_offset_;
    // 资源隔离：每个 Worker 独享一个 io_uring
    IoContext ctx_;
    // std::unordered_map<std::string_view, KeyLocation> index_;
    std::map<std::string_view, KeyLocation> index_;

};

TITANKV_NAMESPACE_CLOSE


