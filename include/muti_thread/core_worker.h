#pragma once
#include <thread>
#include <vector>
#include <queue>
#include <atomic>
#include <memory>
#include <iostream>
#include <unistd.h>

#include "io/io_uring_loop.h"
#include "io/raw_device.h"
#include "common/buffer.h"
#include "muti_thread/SPSCQueue.h"
#include "common.h"

TITANKV_NAMESPACE_OPEN

class CoreWorker
{
public:
    CoreWorker(unsigned core_id, const std::string& file_path);

    void start();

    void stop();

    void submit(WriteRequest req);

private:
    void run();

    std::unique_ptr<rigtorp::SPSCQueue<WriteRequest>> queue_; 
    std::unique_ptr<RawDevice> device_;
    std::atomic<bool> stop_;
    std::thread thread_;
    unsigned core_id_;
    unsigned current_offset_;
    // 资源隔离：每个 Worker 独享一个 io_uring
    IoContext ctx_;
};

TITANKV_NAMESPACE_CLOSE


