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

    // 只有当申请大小 >= 64KB 时才尝试大页，否则直接走 malloc
    AlignedBuffer(size_t size = kAlignment) : size_(size)
    {
        bool try_huge = (size >= 64 * 1024);
        
        void* ptr = MAP_FAILED;
        
        if (try_huge) 
            ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
        
        
        if(ptr != MAP_FAILED)
        {
            data_ = static_cast<uint8_t*>(ptr);
            is_huge_ = true;
            mlock(ptr, size);
        }
        else
        {
            if(posix_memalign((void**)&data_, kAlignment, size_) != 0)
            {
                throw std::runtime_error("Aligned alloc failed");
            }
            is_huge_ = false;
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

    //  移动构造
    AlignedBuffer(AlignedBuffer&& other) noexcept
    : data_(other.data_), size_(other.size_), is_huge_(other.is_huge_)
    {
        other.data_ = nullptr;
        other.size_ = 0;
    }

    // 复制构造
    AlignedBuffer(AlignedBuffer& other)
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

    // 拷贝构造
    AlignedBuffer(const AlignedBuffer& other)
    : size_(other.size_), is_huge_(false)
    {
        void* ptr = nullptr;
        if (posix_memalign(&ptr, kAlignment, size_) != 0)
            throw std::runtime_error("Aligned alloc failed");

        data_ = static_cast<uint8_t*>(ptr);
        std::memcpy(data_, other.data_, size_);
    }

    // 拷贝赋值
    AlignedBuffer& operator=(const AlignedBuffer& other)
    {
        if (this == &other)
            return *this;

        void* new_data = nullptr;
        if (posix_memalign(&new_data, kAlignment, other.size_) != 0)
            throw std::runtime_error("Aligned alloc failed");

        std::memcpy(new_data, other.data_, other.size_);

        if(data_) 
        {
            if(is_huge_) munmap(data_, size_);
            else free(data_);
        }
        
        data_ = static_cast<uint8_t*>(new_data);
        size_ = other.size_;
        is_huge_ = false;
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