/*
与Rocksdb的基准性能测试代码:
在 100% 随机写入、4KB Payload、强一致性落盘场景下，
TitanKV 的吞吐量达到 4.4k IOPS，是 RocksDB（开启 WAL Sync）的 2 倍。
在某次测试中输出如下：
------------------------------------------------
[Bench] RocksDB (Default: WAL + MemTable)...
  -> Time: 26.1211 s
  -> IOPS: 1914.16
  -> Throughput: 7.47718 MB/s
------------------------------------------------
[Bench] TitanKV (DirectIO + TPC)...
  -> Time: 11.135 s
  -> IOPS: 4490.34
  -> Throughput: 17.5404 MB/s
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

// RocksDB 头文件
#include <rocksdb/db.h>
#include <rocksdb/options.h>

// TitanKV 头文件
#include "muti_thread/titan_engine.h"
#include "common/buffer.h"

using namespace titankv;

// ==========================================
// 统一配置参数
// ==========================================
// const int NUM_THREADS = std::thread::hardware_concurrency() - 2; // 客户端并发数
const int NUM_THREADS = 1;
const int NUM_KEYS_PER_THREAD = 25000;// 每个线程写多少
const int TOTAL_OPS = NUM_THREADS * NUM_KEYS_PER_THREAD;
const int VALUE_SIZE = 4096;          // 4KB
const std::string ROCKSDB_PATH = "./bench_rocksdb_data";
const std::string TITANKV_PATH = "./bench_titankv_data";

// 辅助：生成 Value
std::string gen_value() 
{
    return std::string(VALUE_SIZE, 'v');
}

// ==========================================
// 1. RocksDB Benchmark
// ==========================================
void bench_rocksdb() 
{
    std::cout << "------------------------------------------------" << std::endl;
    std::cout << "[Bench] RocksDB (Default: WAL + MemTable)..." << std::endl;

    if (std::filesystem::exists(ROCKSDB_PATH)) std::filesystem::remove_all(ROCKSDB_PATH);

    rocksdb::DB* db;
    rocksdb::Options options;
    rocksdb::WriteOptions wo;
    wo.sync = true;
    options.create_if_missing = true;
    options.write_buffer_size = 64 * 1024 * 1024; 
    options.max_background_jobs = 4;

    rocksdb::Status status = rocksdb::DB::Open(options, ROCKSDB_PATH, &db);
    if (!status.ok()) 
    {
        std::cerr << "Open RocksDB failed: " << status.ToString() << std::endl;
        return;
    }

    std::string value = gen_value();
    std::vector<std::thread> threads;
    auto start = std::chrono::high_resolution_clock::now();

    for (int t = 0; t < NUM_THREADS; ++t) 
    {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < NUM_KEYS_PER_THREAD; ++i) 
            {
                std::string key = "key_" + std::to_string(t) + "_" + std::to_string(i);
                db->Put(wo, key, value);
            }
        });
    }

    for (auto& t : threads) 
        t.join();

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(end - start).count();
    
    std::cout << "  -> Time: " << elapsed << " s" << std::endl;
    std::cout << "  -> IOPS: " << TOTAL_OPS / elapsed << std::endl;
    std::cout << "  -> Throughput: " << (double)TOTAL_OPS * VALUE_SIZE / 1024 / 1024 / elapsed << " MB/s" << std::endl;

    delete db;
}

// ==========================================
// 2. TitanKV Benchmark (Async + Batching)
// ==========================================
void bench_titankv() 
{
    std::cout << "------------------------------------------------" << std::endl;
    std::cout << "[Bench] TitanKV (DirectIO + TPC)..." << std::endl;

    if (std::filesystem::exists(TITANKV_PATH)) 
        std::filesystem::remove_all(TITANKV_PATH);
    std::filesystem::create_directory(TITANKV_PATH);

    // 2 个 Worker (适应 4 核环境: 1 Main + 2 Worker + 0 SQPOLL)
    TitanEngine db(TITANKV_PATH, 2); 

    std::string value_str = gen_value();
    
    // 预分配 Buffer 模板，避免测试中频繁 memset
    AlignedBuffer template_buf(VALUE_SIZE);
    std::memcpy(template_buf.data(), value_str.data(), VALUE_SIZE);

    std::atomic<int> completed_count{0};
    std::vector<std::thread> threads;
    
    auto start = std::chrono::high_resolution_clock::now();

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
    
    while (completed_count.load() < TOTAL_OPS) 
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
    bench_rocksdb();
    bench_titankv();
    return 0;
}