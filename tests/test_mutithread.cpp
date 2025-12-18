#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <fcntl.h>
#include <unistd.h>
#include <iomanip>
#include <cstring>

#include "io/io_uring_loop.h"
#include "io/raw_device.h"
#include "common/buffer.h"
#include "muti_thread/muti_thread.h"

using namespace titankv;

// 定义测试参数
// 4KB KV Item
const int IO_SIZE = 4096;   
// 10万次写入        
const int TOTAL_IOS = 100000;       
const std::string FILE_PATH = "bench_file.dat";

// ---------------------------------------------------------
// 1. TitanKV (Async, Ring Batching)
// ---------------------------------------------------------
void bench_titankv(const std::vector<off_t>& offsets, const AlignedBuffer& buf) 
{
    RawDevice device(FILE_PATH);
    // 测试队列
    IoContext ctx(4096); 

    int pending_ios = 0;
    int completed_ios = 0;
    int total = offsets.size();

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < total; ++i) 
    {
        ctx.SubmitWrite(device.fd(), 
                        buf, 
                        offsets[i], 
                        [&](int res) 
                        {
                            if (res < 0) std::cerr << "Async write error" << std::endl;
                            completed_ios++;
                        });
        pending_ios++;

        if (pending_ios > 4000) 
        {
            ctx.RunOnce();
            pending_ios--;
        }
    }

    // 等待剩余所有的 IO 完成
    while (completed_ios < total) 
    {
        ctx.RunOnce();
    }

    auto end = std::chrono::high_resolution_clock::now();

    double elapsed_sec = std::chrono::duration<double>(end - start).count();
    double iops = total / elapsed_sec;
    std::cout << "[TitanKV Uring] Time: " << elapsed_sec << "s | IOPS: " << iops 
              << " | Latency (Amortized): " << (elapsed_sec / total) * 1e6 << " us/op" << std::endl;
}


// ---------------------------------------------------------
// 2. TitanKV muti_thread (Async, Ring Batching)
// ---------------------------------------------------------
void bench_titankv_mutithread(const std::vector<off_t>& offsets, const AlignedBuffer& buf) 
{
    std::atomic<unsigned> completed_ios{0};
    RawDevice device(FILE_PATH);
    unsigned total = offsets.size();
    
    std::vector<std::unique_ptr<MutiThread>> workers;
    workers.reserve(default_thread_num);
    for(unsigned i = 0; i < default_thread_num; i++) 
        workers.emplace_back(std::make_unique<MutiThread>(device));

    for(auto& w : workers)
        w->start();

    auto start = std::chrono::high_resolution_clock::now();

    for(unsigned i = 0; i < total; ++i)
    {
        size_t w = i % workers.size();
        workers[w]->submit(WriteRequest{
            buf,
            offsets[i],
            [&](int /*res*/){
                completed_ios.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    while(completed_ios.load(std::memory_order_relaxed) < total)
        std::this_thread::yield();

    for(unsigned i = 0;i < default_thread_num; i++)
        workers[i]->stop();

    auto end = std::chrono::high_resolution_clock::now();

    double elapsed_sec = std::chrono::duration<double>(end - start).count();
    double iops = total / elapsed_sec;
    std::cout << "Total thread num: " << default_thread_num << " | [TitanKV Uring] Time: " << elapsed_sec << "s | IOPS: " << iops 
              << " | Latency (Amortized): " << (elapsed_sec / total) * 1e6 << " us/op" << std::endl;
}


int main() 
{
    // 预分配 1GB 文件 (避免文件元数据增长带来的开销)
    std::cout << "Preparing 1GB file (please wait)..." << std::endl;
    int ret = system("dd if=/dev/zero of=bench_file.dat bs=1G count=1 > /dev/null 2>&1");
    (void)ret;

    // 准备数据 buffer (内存对齐)
    AlignedBuffer buf(IO_SIZE);
    memset(buf.data(), 'K', IO_SIZE);

    // 随机生成 10万 个写入偏移量，确保两次测试位置一样
    std::cout << "Generating " << TOTAL_IOS << " random offsets..." << std::endl;
    std::vector<off_t> offsets;
    offsets.reserve(TOTAL_IOS);
    std::mt19937 rng(42); // 固定种子
    std::uniform_int_distribution<uint64_t> dist(0, (1024 * 1024 * 1024) - IO_SIZE);
    
    for(int i=0; i<TOTAL_IOS; ++i) 
    {
        // 必须要按 4KB 对齐偏移，否则 O_DIRECT 报错
        off_t raw = dist(rng);
        off_t aligned = (raw / 4096) * 4096;
        offsets.push_back(aligned);
    }

    std::cout << "=========================================" << std::endl;
    std::cout << " BENCHMARK START (QD=1 vs QD=Async)" << std::endl;
    std::cout << "=========================================" << std::endl;

    // 运行测试
    bench_titankv(offsets, buf);
    std::cout << "-----------------------------------------" << std::endl;
    bench_titankv_mutithread(offsets, buf);

    return 0;
}