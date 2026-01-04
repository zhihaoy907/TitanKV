/*
基于已有环境：IO_URING、CPU亲和性，测试SPSC无锁队列相较普通的带锁队列的程序，
在多次4核2线程的测试中SPSCQueue把时间从9.x秒缩短至7.x秒，平均提升约18%的性能
*/

#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <fcntl.h>
#include <unistd.h>
#include <iomanip>
#include <cstring>
#include <immintrin.h>
#include <filesystem>
#include <atomic>
#include <thread>

#include "io/io_uring_loop.h"
#include "io/raw_device.h"
#include "common/buffer.h"

#include "muti_thread/muti_thread.h"
#include "muti_thread/core_worker.h"

using namespace titankv;

// ==========================================
// 全局配置
// ==========================================
const int IO_SIZE = 4096;
const int TOTAL_IOS = 100000;

// Mutex 版本使用的共享大文件
const std::string COMMON_FILE_PATH = "bench_file_mutex.dat";
// SPSC 版本使用的目录 (每个线程一个文件)
const std::string SPSC_DIR_PATH = "bench_data_spsc";

// ---------------------------------------------------------
// 1. TitanKV Mutex Version (基准对照组)
// ---------------------------------------------------------
void bench_titankv_mutithread(const std::vector<off_t>& offsets) 
{
    std::atomic<unsigned> completed_ios{0};
    
    RawDevice device(COMMON_FILE_PATH);
    unsigned total = offsets.size();
    
    // 留出核心给主线程和 SQPOLL
    unsigned num_cpus = std::thread::hardware_concurrency();
    unsigned real_thread_num = (num_cpus > 2) ? (num_cpus - 2) : 1;

    std::vector<std::unique_ptr<MutiThread>> workers;
    workers.reserve(real_thread_num);
    
    for(unsigned i = 0; i < real_thread_num; i++)
        workers.emplace_back(std::make_unique<MutiThread>(device));

    // 启动线程
    for(size_t k = 0; k < workers.size(); k++) 
        workers[k]->start(k);

    AlignedBuffer template_buf(IO_SIZE);
    std::memset(template_buf.data(), 'M', IO_SIZE);

    auto start = std::chrono::high_resolution_clock::now();

    for(unsigned i = 0; i < total; ++i)
    {
        size_t w = i % workers.size();
        
        AlignedBuffer req_buf(IO_SIZE);
        std::memcpy(req_buf.data(), template_buf.data(), IO_SIZE);

        workers[w]->submit(WriteRequest{
            std::move(req_buf),
            offsets[i],
            [&](int /*res*/){
                completed_ios.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    // 等待完成
    while(completed_ios.load(std::memory_order_relaxed) < total)
        std::this_thread::yield();

    for(auto& w : workers) w->stop();

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_sec = std::chrono::duration<double>(end - start).count();
    double iops = total / elapsed_sec;

    std::cout << "Thread(Mutex) Num: " << real_thread_num 
              << " | Time: " << elapsed_sec << "s | IOPS: " << iops 
              << " | Latency: " << (elapsed_sec / total) * 1e6 << " us/op" << std::endl;
}

// ---------------------------------------------------------
// 2. TitanKV SPSC Version (无锁 TPC)
// ---------------------------------------------------------
void bench_titankv_spscqueue(const std::vector<off_t>& offsets) 
{
    std::atomic<unsigned> completed_ios{0};
    unsigned total = offsets.size();

    // 准备目录
    if (!std::filesystem::exists(SPSC_DIR_PATH)) {
        std::filesystem::create_directory(SPSC_DIR_PATH);
    }

    // 线程数策略
    unsigned num_cpus = std::thread::hardware_concurrency();
    unsigned real_thread_num = (num_cpus > 2) ? (num_cpus - 2) : 1;

    std::vector<std::unique_ptr<CoreWorker>> workers;
    workers.reserve(real_thread_num);

    // 创建 Worker
    for(unsigned i = 0; i < real_thread_num; i++) {
        workers.emplace_back(std::make_unique<CoreWorker>(i, SPSC_DIR_PATH));
    }

    for(size_t k = 0; k < workers.size(); k++) 
        workers[k]->start();

    AlignedBuffer template_buf(IO_SIZE);
    std::memset(template_buf.data(), 'S', IO_SIZE);

    auto start = std::chrono::high_resolution_clock::now();

    // 分发任务
    for(unsigned i = 0; i < total; ++i)
    {
        size_t w = i % workers.size();
        
        AlignedBuffer req_buf(IO_SIZE);
        std::memcpy(req_buf.data(), template_buf.data(), IO_SIZE);

        // 构造请求
        WriteRequest req{
            std::move(req_buf), 
            offsets[i],
            [&](int){
                completed_ios.fetch_add(1, std::memory_order_relaxed);
            }
        };

        workers[w]->submit(std::move(req));
    }

    while(completed_ios.load(std::memory_order_relaxed) < total)
        std::this_thread::yield();

    for(unsigned i = 0;i < workers.size(); i++)
        workers[i]->stop();

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_sec = std::chrono::duration<double>(end - start).count();
    double iops = total / elapsed_sec;
    
    std::cout << "Thread(SPSC) Num:  " << real_thread_num 
              << " | Time: " << elapsed_sec << "s | IOPS: " << iops 
              << " | Latency: " << (elapsed_sec / total) * 1e6 << " us/op" << std::endl;
}


int main() 
{
    if (access(COMMON_FILE_PATH.c_str(), F_OK) != 0) {
        std::cout << "Creating sparse file for Mutex test..." << std::endl;
        std::string cmd = "dd if=/dev/zero of=" + COMMON_FILE_PATH + " bs=1G count=1 >/dev/null 2>&1";
        system(cmd.c_str());
    }

    // 生成 Offset
    std::cout << "Generating " << TOTAL_IOS << " random offsets..." << std::endl;
    std::vector<off_t> offsets;
    offsets.reserve(TOTAL_IOS);
    std::mt19937 rng(42); 
    // 限制范围在 1GB 内
    std::uniform_int_distribution<uint64_t> dist(0, (1024 * 1024 * 1024 - IO_SIZE) / IO_SIZE);
    
    for(int i=0; i<TOTAL_IOS; ++i)
        offsets.push_back(dist(rng) * IO_SIZE);

    std::cout << "=========================================" << std::endl;
    std::cout << " BENCHMARK: Mutex (Shared) vs SPSC (TPC)" << std::endl;
    std::cout << "=========================================" << std::endl;

    bench_titankv_mutithread(offsets);
    
    std::cout << "-----------------------------------------" << std::endl;
    
    bench_titankv_spscqueue(offsets);

    return 0;
}