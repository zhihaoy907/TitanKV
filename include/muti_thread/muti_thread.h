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
#include "common.h"

TITANKV_NAMESPACE_OPEN

class MutiThread
{
public:
    MutiThread(const RawDevice& device);

    void start(unsigned core_id);

    void stop();

    // 消息传递接口
    void submit(WriteRequest req);

private:
    void run();

    RawDevice device_;
    IoContext ctx_;
    std::queue<WriteRequest> write_queue_;
    std::atomic<bool> stop_;
    std::thread thread_;
    std::mutex write_queue_mutex_;
};

TITANKV_NAMESPACE_CLOSE


