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

using namespace titankv;

struct WriteRequest 
{
    AlignedBuffer buf;
    off_t offset;
    std::function<void(int)> callback;
};

class CoreWorker 
{
public:
    CoreWorker(int core_id, const std::string& file_path);

    void start();

    void stop();

    // 消息传递接口
    void submit(WriteRequest req);

private:
    void run();

    int core_id_;
    RawDevice device_;
    IoContext ctx_;
    std::queue<WriteRequest> queue_;
    std::atomic<bool> stop_;
    std::thread thread_;
};


