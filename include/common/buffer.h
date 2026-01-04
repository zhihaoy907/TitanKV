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

    AlignedBuffer(size_t size = kAlignment) : size_(size)
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
        other.size_ = 0;
    }

    uint8_t *data() const
    {
        return data_;
    }

    size_t size() const
    {
        return size_;
    }

    AlignedBuffer& operator=(AlignedBuffer&& other) noexcept
    {
        if(this != &other)
        {
            if(data_)
                free(data_);

            data_ = other.data_;
            size_ = other.size_;

            other.data_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

private:
    uint8_t *data_ = nullptr;
    size_t size_;
};


TITANKV_NAMESPACE_CLOSE