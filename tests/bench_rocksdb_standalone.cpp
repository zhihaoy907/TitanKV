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

// RocksDB 头文件
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/table.h> // 引入 table.h 以配置 BlockCache

// ==========================================
// 统一配置参数
// ==========================================
const int NUM_THREADS = 2; // 客户端并发数
const int NUM_KEYS_PER_THREAD = 100000;// 每个线程写多少
const int TOTAL_OPS = NUM_THREADS * NUM_KEYS_PER_THREAD;
const int VALUE_SIZE = 4096;          // 4KB
const std::string ROCKSDB_PATH = "./bench_rocksdb_data";
const std::string TITANKV_PATH = "./bench_titankv_data";


void wait_for_attach() 
{
    std::cout << "PID: " << getpid() << std::endl;
    std::cout << "Press ENTER to start benchmark..." << std::endl;
    std::cin.get();
}

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
    std::cout << "[Bench] RocksDB (O_DIRECT Enabled)..." << std::endl;
    std::cout << "  Threads: " << NUM_THREADS << std::endl;
    std::cout << "  Total Ops: " << TOTAL_OPS << std::endl;
    
    if (std::filesystem::exists(ROCKSDB_PATH)) std::filesystem::remove_all(ROCKSDB_PATH);

    rocksdb::DB* db;
    rocksdb::Options options;
    rocksdb::WriteOptions wo;
    wo.sync = false; 

    options.create_if_missing = true;
    options.write_buffer_size = 64 * 1024 * 1024; 
    options.max_background_jobs = 4; 

    options.use_direct_io_for_flush_and_compaction = true;
    options.use_direct_reads = true;

    rocksdb::BlockBasedTableOptions table_options;
    table_options.block_cache = rocksdb::NewLRUCache(64 * 1024 * 1024); // 64MB Cache
    options.table_factory.reset(rocksdb::NewBlockBasedTableFactory(table_options));

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

int main() 
{
    wait_for_attach();
    bench_rocksdb();
    return 0;
}