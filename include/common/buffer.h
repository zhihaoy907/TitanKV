#pragma once
#include <vector>
#include <memory>
#include <cstdint>
#include <cstdlib>

#include "common/common.h"

TITANKV_NAMESPACE_OPEN

class AlignedBuffer
{
public:
    
    static constexpr size_t kAlignment = 4096;

    AlignedBuffer(size_t size) : size_(size)
    {
        // posix_memalign 分配对齐内存
        if(posix_memalign((void**)&data_, kAlignment, size_) != 0)
        {
            throw std::runtime_error("Aligned alloc failed");
        }
    }

    ~AlignedBuffer()
    {
        if(data_)
            free(data_);
    }

    AlignedBuffer(const AlignedBuffer&) = delete;
    AlignedBuffer& operator=(const AlignedBuffer&) = delete;

    AlignedBuffer(AlignedBuffer&& other) noexcept: data_(other.data_), size_(other.size_)
    {
        other.data_ = nullptr;
    }

    uint8_t *data() const
    {
        return data_;
    }

    size_t size() const
    {
        return size_;
    }

private:
    uint8_t *data_ = nullptr;
    size_t size_;
};


TITANKV_NAMESPACE_CLOSE