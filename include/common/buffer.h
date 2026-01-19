#pragma once
#include <vector>
#include <memory>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>
#include <iostream>


#include "common/common.h"

TITANKV_NAMESPACE_OPEN

class AlignedBuffer
{
public:
    
    static constexpr size_t kAlignment = 4096;

    AlignedBuffer(size_t size = kAlignment) : size_(size), is_huge_(false)
    {
        // 只有当申请的大小超过 1MB 时，才考虑使用大页
        if (size >= 1024 * 1024) 
        {
            void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
            
            if (ptr != MAP_FAILED) 
            {
                data_ = static_cast<uint8_t*>(ptr);
                is_huge_ = true;
                return;
            }
        }
        
        // 普通小内存直接走系统分配
        if (posix_memalign((void**)&data_, kAlignment, size_) != 0) {
            throw std::runtime_error("Aligned alloc failed");
        }
    }

    ~AlignedBuffer()
    {
        if(data_)
        {
            if(is_huge_)
                munmap(data_, size_);
            else
                free(data_);
        }
    }

    AlignedBuffer(const AlignedBuffer&) = delete;
    AlignedBuffer& operator=(const AlignedBuffer&) = delete;

    //  移动构造
    AlignedBuffer(AlignedBuffer&& other) noexcept
    : data_(other.data_), size_(other.size_), is_huge_(other.is_huge_)
    {
        other.data_ = nullptr;
        other.size_ = 0;
        other.is_huge_ = false;
    }

    // 移动赋值
    AlignedBuffer& operator=(AlignedBuffer&& other)
    {
        if (this == &other)
            return *this;

        if(data_)
        {
            if(is_huge_) munmap(data_, size_);
            else free(data_);
        }
        data_ = other.data_;
        size_ = other.size_;
        is_huge_ = other.is_huge_;

        other.data_ = nullptr;
        other.size_ = 0;
        other.is_huge_ = false;
        return *this;
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
    bool is_huge_;
};


TITANKV_NAMESPACE_CLOSE