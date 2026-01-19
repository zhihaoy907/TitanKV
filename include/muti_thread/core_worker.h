#pragma once
#include <thread>
#include <vector>
#include <queue>
#include <atomic>
#include <memory>
#include <iostream>
#include <unistd.h>
#include <map>

#include "common.h"
#include "io/io_uring_loop.h"
#include "io/raw_device.h"
#include "common/buffer.h"
#include "common/flat_index.h"

#ifdef TITAN_USE_MPSC
#ifdef BLOCK_SIZE
#undef BLOCK_SIZE
#endif
    #include "common/concurrentqueue.h"
#else
    #include "common/SPSCQueue.h"
#endif


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
    
    void RewriteFile();

    void recover();

private:
    void run();
    std::string ExtractValue(const AlignedBuffer& buf, uint32_t len);

    alignas(64)
#ifdef TITAN_USE_MPSC
    // MPSC 架构
    moodycamel::ConcurrentQueue<ReadRequest> read_queue_;
    moodycamel::ConcurrentQueue<WriteRequest> write_queue_;
#else
    // SPSC 架构
    std::unique_ptr<rigtorp::SPSCQueue<ReadRequest>> read_queue_;
    std::unique_ptr<rigtorp::SPSCQueue<WriteRequest>> write_queue_;
#endif

    alignas(64)
    std::unique_ptr<RawDevice> device_;
    std::atomic<bool> stop_;
    std::thread thread_;
    unsigned core_id_;
    unsigned current_offset_;
    int fd_;
    std::string filename_;
    // 资源隔离：每个 Worker 独享一个 io_uring
    IoContext ctx_;
    // 一定不要使用string_view!!!!!!!!!
    // std::unordered_map<std::string, KeyLocation> index_;
    FlatIndex index_; 
};

TITANKV_NAMESPACE_CLOSE


