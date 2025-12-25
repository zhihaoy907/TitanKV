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
#include "muti_thread/SPSCQueue.h"

TITANKV_NAMESPACE_OPEN

// 最大线程数量
static unsigned default_thread_num = std::thread::hardware_concurrency();

struct WriteRequest 
{
    const AlignedBuffer& buf;
    off_t offset;
    std::function<void(int)> callback;
};

class CoreWorker
{
public:
    CoreWorker(const RawDevice& device);

    TITANKV_NODISCARD bool start(unsigned core_id);

    void stop();

    // 提交不再需要锁，但可能会失败。强制调用者对返回值进行判断
    TITANKV_NODISCARD bool submit(WriteRequest req);

private:
    void run();

    RawDevice device_;
    // 资源隔离：每个 Worker 独享一个 io_uring
    IoContext ctx_;
    rigtorp::SPSCQueue<WriteRequest> queue_;
    std::atomic<bool> stop_;
    std::thread thread_;
};

TITANKV_NAMESPACE_CLOSE


