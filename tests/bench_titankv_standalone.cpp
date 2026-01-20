/*
titankv的随机写入效率测试代码。
titankv支持SPSC+MPSC架构，但是由于为了跟Rocksdb对比极致的性能，因此SPSC未加锁.
所以如果在编译选项没加 -DUSE_MPSC_QUEUE=ON 开启MPSC功能，
请将 NUM_THREADS设置为1，否则由于Put时竞争会导致Iorequest的数据损坏，测试代码死循环
*/

#include <iostream>
#include <vector>
#include <thread>
#include <string>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <cstring>
#include <iomanip>
#include <unistd.h>

// TitanKV 头文件
#include "muti_thread/titan_engine.h"
#include "common/buffer.h"

using namespace titankv;

// ==========================================
// 统一配置参数
// ==========================================
// const int NUM_THREADS = std::thread::hardware_concurrency() > 2 ? std::thread::hardware_concurrency() - 2 : 1; 
const int NUM_THREADS = 2;
const int NUM_KEYS_PER_THREAD = 100000; // 每个线程写多少
const int TOTAL_OPS = NUM_THREADS * NUM_KEYS_PER_THREAD;
const int VALUE_SIZE = 4096;           // 4KB
const std::string TITANKV_PATH = "./bench_titankv_data";

std::string gen_value() 
{
    return std::string(VALUE_SIZE, 'v');
}

void wait_for_attach() 
{
    std::cout << "PID: " << getpid() << std::endl;
    std::cout << "1. Open another terminal." << std::endl;
    std::cout << "2. Run: sudo ../tools/analyze_lock.bt " << getpid() << std::endl;
    std::cout << "3. Press ENTER here to START." << std::endl;
    std::cin.get();
}

void bench_titankv() 
{
    std::cout << "------------------------------------------------" << std::endl;
    std::cout << "[Bench] TitanKV (DirectIO + TPC)..." << std::endl;
    std::cout << "  Threads: " << NUM_THREADS << std::endl;
    std::cout << "  Total Ops: " << TOTAL_OPS << std::endl;

    if (std::filesystem::exists(TITANKV_PATH)) std::filesystem::remove_all(TITANKV_PATH);
    std::filesystem::create_directory(TITANKV_PATH);

    TitanEngine db(TITANKV_PATH, 2); 

    std::string value_str = gen_value();
    
    AlignedBuffer template_buf(VALUE_SIZE);
    std::memcpy(template_buf.data(), value_str.data(), VALUE_SIZE);

    std::atomic<int> completed_count{0};
    std::vector<std::thread> threads;
    
    // 等待 eBPF Attach
    wait_for_attach();

    auto start = std::chrono::high_resolution_clock::now();

    // 启动 Client 线程
    for (int t = 0; t < NUM_THREADS; ++t) 
    {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < NUM_KEYS_PER_THREAD; ++i) 
            {
                std::string key = "key_" + std::to_string(t) + "_" + std::to_string(i);
                
                AlignedBuffer req_buf(VALUE_SIZE);
                std::memcpy(req_buf.data(), template_buf.data(), VALUE_SIZE);
                
                db.Put(key, value_str, [&](int){
                    completed_count.fetch_add(1, std::memory_order_relaxed);
                });
            }
        });
    }

    for (auto& t : threads) 
        t.join();
    
    while (completed_count.load(std::memory_order_relaxed) < TOTAL_OPS) 
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(end - start).count();

    std::cout << "  -> Time: " << elapsed << " s" << std::endl;
    std::cout << "  -> IOPS: " << TOTAL_OPS / elapsed << std::endl;
    std::cout << "  -> Throughput: " << (double)TOTAL_OPS * VALUE_SIZE / 1024 / 1024 / elapsed << " MB/s" << std::endl;
}

int main() 
{
    bench_titankv();
    return 0;
}