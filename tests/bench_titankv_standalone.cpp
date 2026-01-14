#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstring>
#include <unistd.h>
#include <filesystem>

#include "muti_thread/titan_engine.h"
#include "common/buffer.h"

using namespace titankv;

// 持续运行 10 秒，足够 bpftrace 抓取
const int DURATION_SEC = 10;
const int NUM_THREADS = 1;
const int VALUE_SIZE = 4096;

void prove_lockless() {
    std::string path = "./prove_lockless_data";
    if (std::filesystem::exists(path)) std::filesystem::remove_all(path);
    std::filesystem::create_directory(path);

    TitanEngine db(path, 2); // 2 Workers

    std::cout << "PID: " << getpid() << std::endl;
    std::cout << "Ready to run lockless benchmark for " << DURATION_SEC << " seconds." << std::endl;
    std::cout << "Please start bpftrace now: sudo ./analyze_lock.bt -p " << getpid() << std::endl;
    std::cout << "Press ENTER to start..." << std::endl;
    std::cin.get();

    std::atomic<bool> running{true};
    std::atomic<uint64_t> total_ops{0};
    std::vector<std::thread> threads;

    AlignedBuffer template_buf(VALUE_SIZE);
    std::memset(template_buf.data(), 'X', VALUE_SIZE);

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            uint64_t i = 0;
            while (running.load(std::memory_order_relaxed)) {
                // 1. 构造 Key
                // 确保 Key 足够分散，但也别太长
                std::string key = "k_" + std::to_string(t) + "_" + std::to_string(i++);
                
                // 2. 构造 Buffer (必须拷贝)
                AlignedBuffer req_buf(VALUE_SIZE);
                std::memcpy(req_buf.data(), template_buf.data(), VALUE_SIZE);

                // 3. 异步写入 (无 promise!)
                db.Put(key, std::string((char*)req_buf.data(), VALUE_SIZE), 
                       [](int){ /* 空回调，越快越好 */ });
                       
                // 简单的流控，防止内存爆掉 (Backpressure)
                // 这里我们稍微自旋一下，模拟真实负载
                // for (int k=0; k<100; ++k) _mm_pause(); 
            }
        });
    }

    // 运行 DURATION_SEC 秒
    running.store(false);

    for (auto& t : threads) t.join();

    std::cout << "Benchmark finished." << std::endl;
}

int main() {
    prove_lockless();
    return 0;
}