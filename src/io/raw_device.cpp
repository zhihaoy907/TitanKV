#include "io/raw_device.h"

#include <fcntl.h>

using namespace titankv;

RawDevice::RawDevice(const std::string& path)
{
    // O_DIRECT: 绕过pagecache，直接DMA
    // O_DSYNC: 保证元数据和数据落盘，先不开看看极限带宽
    int flag = O_RDWR | O_CREAT | O_DIRECT;

    fd_ = ::open(path.c_str(), flag, 0644);
    
    if(fd_ < 0)
    {
        perror("Open failed");
        throw std::runtime_error("Failed to open file with O_DIRECT");
    }

    fallocate(fd_, 0, 0, 1024 * 1024 * 1024);
}

RawDevice::~RawDevice()
{
    if(fd_ > 0)
        ::close(fd_);
}

int RawDevice::fd()
{
    return fd_;
}