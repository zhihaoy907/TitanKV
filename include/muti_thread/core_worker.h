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
    CoreWorker(const RawDevice& device);

    void start(unsigned core_id);

    void stop();

    // 提交不再需要锁，但可能会失败。强制调用者对返回值进行判断
    TITANKV_NODISCARD bool submit(WriteRequest req);

private:
    void run();

    std::unique_ptr<rigtorp::SPSCQueue<WriteRequest>> queue_; 
    RawDevice device_;
    // 资源隔离：每个 Worker 独享一个 io_uring
    IoContext ctx_;
    std::atomic<bool> stop_;
    std::thread thread_;
};

TITANKV_NAMESPACE_CLOSE


