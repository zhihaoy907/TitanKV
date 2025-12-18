#pragma once
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>

#include "common/common.h"

TITANKV_NAMESPACE_OPEN

class RawDevice
{
public:
    explicit RawDevice(const std::string& path);

    ~RawDevice();

    int fd();

private:
    int fd_ = -1;
};

TITANKV_NAMESPACE_CLOSE