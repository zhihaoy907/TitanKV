#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstring>
#include <unistd.h>
#include <filesystem>
#include <iomanip>

#include "muti_thread/titan_engine.h"
#include "common/buffer.h"

using namespace titankv;

const int DURATION_SEC = 5;
const int NUM_THREADS = 1;
const int VALUE_SIZE = 4096;

void prove_lockless() 
{
    std::string path = "./prove_lockless_data";
    if (std::filesystem::exists(path)) 
        std::filesystem::remove_all(path);
    std::filesystem::create_directory(path);

    // 2 个 Worker (绑核)
    TitanEngine db(path, 2);

    std::cout << "============================================" << std::endl;
    std::cout << "   TitanKV Lockless Proof Benchmark         " << std::endl;
    std::cout << "============================================" << std::endl;
    std::cout << "PID: " << getpid() << std::endl;
    std::cout << "1. Open another terminal." << std::endl;
    std::cout << "2. Run: sudo ./tools/trace_futex.bt " << getpid() << std::endl;
    std::cout << "3. Press ENTER here to START." << std::endl;
    std::cin.get();

    std::atomic<bool> running{true};
    std::atomic<uint64_t> total_ops{0}; // 实时计数
    std::vector<std::thread> threads;

    AlignedBuffer template_buf(VALUE_SIZE);
    std::memset(template_buf.data(), 'X', VALUE_SIZE);

    // 启动 Client 线程
    for (int t = 0; t < NUM_THREADS; ++t) 
    {
        threads.emplace_back([&, t]() 
        {
            uint64_t i = 0;
            
            while (running.load(std::memory_order_relaxed)) 
            {
                std::string key = "k_" + std::to_string(t) + "_" + std::to_string(i++);
                
                AlignedBuffer req_buf(VALUE_SIZE);
                std::memcpy(req_buf.data(), template_buf.data(), VALUE_SIZE);

                db.Put(key, std::string((char*)req_buf.data(), VALUE_SIZE), 
                       [&](int){ 
                           total_ops.fetch_add(1, std::memory_order_relaxed); 
                       });
            }
        });
    }

    auto start = std::chrono::steady_clock::now();
    for(int i=0; i<DURATION_SEC; ++i) 
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        uint64_t ops = total_ops.load(std::memory_order_relaxed);
        std::cout << "[" << i+1 << "s] Total Ops: " << ops 
                  << " (Avg: " << ops / (i+1) << " ops/s)" << std::endl;
    }

    running.store(false);
    for (auto& t : threads) 
        t.join();

    auto end = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(end - start).count();
    
    std::cout << "Benchmark finished. Total: " << total_ops << " Time: " << elapsed << "s" << std::endl;
}

int main() 
{
    prove_lockless();
    return 0;
}