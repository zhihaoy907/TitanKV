#include <thread>
#include <vector>
#include <queue>
#include <atomic>
#include <functional>
#include <memory>
#include <iostream>
#include <unistd.h>

#include "io/io_uring_loop.h"
#include "io/raw_device.h"
#include "common/buffer.h"

TITANKV_NAMESPACE_OPEN

// 最大线程数量
static unsigned default_thread_num = std::thread::hardware_concurrency();

struct WriteRequest 
{
    const AlignedBuffer& buf;
    off_t offset;
    std::function<void(int)> callback;
};

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
    std::queue<WriteRequest> queue_;
    std::atomic<bool> stop_;
    std::thread thread_;
    std::mutex queue_mutex_;
};

TITANKV_NAMESPACE_CLOSE


