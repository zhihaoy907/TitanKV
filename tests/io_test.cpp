#include <iostream>
#include <vector>
#include <cstring>
#include <chrono>

#include "io/io_uring_loop.h"
#include "io/raw_device.h"
#include "common/buffer.h"

using namespace titankv;

int main() 
{
    // 1. 初始化文件 (创建一个 1GB 的文件做测试)
    // 命令行执行: dd if=/dev/zero of=testfile bs=1G count=1 提前创建好文件，
    // 因为 O_DIRECT 不能改变文件大小（在某些文件系统上），先 pre-allocate。
    
    RawDevice device("testfile");
    IoContext ctx;
    
    // 2. 准备数据: 4KB 数据块
    AlignedBuffer buf(4096);
    memset(buf.data(), 'A', 4096);

    int complete_count = 0;
    int target_io = 10000;

    auto start = std::chrono::high_resolution_clock::now();

    // 3. 疯狂提交写请求
    for (int i = 0; i < target_io; ++i) 
    {
        // 随机写入不同位置
        off_t offset = (i * 4096) % (1024 * 1024 * 1024); // 1GB 范围内
        ctx.SubmitWrite(device.fd(), 
                        buf, 
                        offset, 
                        [&](int res) 
                        {
                            if (res < 0) 
                                std::cerr << "Write error: " << -res << std::endl;
                            else 
                                complete_count++;
                        }
                    );
    }

    // 4. 等待完成 (模拟 Event Loop)
    while (complete_count < target_io) 
    {
        ctx.RunOnce();
    }

    auto end = std::chrono::high_resolution_clock::now();
    double secs = std::chrono::duration<double>(end - start).count();
    
    std::cout << "Done " << target_io << " IOs in " << secs << "s. "
              << "IOPS: " << target_io / secs << std::endl;

    return 0;
}