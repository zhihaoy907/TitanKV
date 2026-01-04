/*
io_uring的性能测试。
使用io_uring较使用pwrite接口在10w次io测试中
将时间从5.1秒降至2.4秒
*/
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

using namespace titankv;

// 定义测试参数
// 4KB KV Item
const int IO_SIZE = 4096;   
// 10万次写入        
const int TOTAL_IOS = 100000;       
const std::string FILE_PATH = "bench_file.dat";

// ---------------------------------------------------------
// 1. POSIX pwrite (Sync, Direct IO)
// ---------------------------------------------------------
void bench_posix(const std::vector<off_t>& offsets, const AlignedBuffer& buf) 
{
    // 手动打开文件，模仿 RawDevice 的行为，开启 O_DIRECT
    int fd = ::open(FILE_PATH.c_str(), O_RDWR | O_CREAT | O_DIRECT, 0644);
    if (fd < 0) 
    {
        perror("POSIX open failed");
        return;
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < offsets.size(); ++i) 
    {
        // 同步系统调用：发起 -> 切换内核 -> 等待磁盘 -> 返回 -> 切换用户态
        ssize_t ret = ::pwrite(fd, buf.data(), buf.size(), offsets[i]);
        if (ret != (ssize_t)buf.size()) 
        {
            std::cerr << "pwrite failed" << std::endl;
            break;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    ::close(fd);

    double elapsed_sec = std::chrono::duration<double>(end - start).count();
    double iops = offsets.size() / elapsed_sec;
    std::cout << "[POSIX pwrite] Time: " << elapsed_sec << "s | IOPS: " << iops 
              << " | Latency: " << (elapsed_sec / offsets.size()) * 1e6 << " us/op" << std::endl;
}

// ---------------------------------------------------------
// 2. TitanKV (Async, Ring Batching)
// ---------------------------------------------------------
void bench_titankv(const std::vector<off_t>& offsets) 
{
    RawDevice device(FILE_PATH);
    // 测试队列
    IoContext ctx(4096);
    AlignedBuffer template_buf(IO_SIZE);
    std::memset(template_buf.data(), 'K', IO_SIZE); 

    // int pending_ios = 0;
    int completed_ios = 0;
    int total = offsets.size();

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < total; ++i) 
    {
        AlignedBuffer req_buf(IO_SIZE);
        std::memcpy(req_buf.data(), template_buf.data(), IO_SIZE);
        
        ctx.SubmitWrite(device.fd(), 
                        std::move(req_buf), 
                        offsets[i], 
                        [&](int res) 
                        {
                            if (res < 0) std::cerr << "Async write error" << std::endl;
                            completed_ios++;
                        });
        // pending_ios++;

        // if (pending_ios > 4000) 
        // {
        //     ctx.RunOnce();
        //     pending_ios--;
        // }
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
    bench_posix(offsets, buf);
    std::cout << "-----------------------------------------" << std::endl;
    bench_titankv(offsets);

    return 0;
}