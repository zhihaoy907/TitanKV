#pragma once
#include <vector>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <sys/mman.h>

#include "common.h"

TITANKV_NAMESPACE_OPEN

struct IndexEntry 
{
    // 0 代表槽位为空
    uint64_t key_hash; 
    uint64_t offset;
    uint32_t len;
    // 填充对齐到 24 字节，或留作扩展
    uint32_t unused;   
};

class FlatIndex 
{
public:
    // capacity 必须是 2 的幂次，方便用 & 代替 % 取模
    explicit FlatIndex(size_t capacity = 1 << 22) : capacity_(capacity) 
    {
        if ((capacity & (capacity - 1)) != 0) 
            throw std::runtime_error("Capacity must be power of 2");
        mask_ = capacity_ - 1;
        
        size_t total_size = capacity_ * sizeof(IndexEntry);
        // 使用大页分配索引内存
        void* ptr = mmap(nullptr, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
        
        if (ptr == MAP_FAILED) 
        {
            ptr = std::aligned_alloc(64, total_size);
            std::memset(ptr, 0, total_size);
        } 
        else 
        {
            std::memset(ptr, 0, total_size);
        }
        entries_ = static_cast<IndexEntry*>(ptr);
    }

    // 插入/更新
    inline void put(uint64_t hash, uint64_t off, uint32_t len) 
    {
        if (hash == kEmpty || hash == kTombstone) hash = 1; // 避免冲突
        
        size_t idx = hash & mask_;
        size_t first_tombstone = mask_ + 1; // 记录第一个遇到的墓碑

        while (entries_[idx].key_hash != kEmpty) 
        {
            if (entries_[idx].key_hash == hash) 
            {
                entries_[idx].offset = off;
                entries_[idx].len = len;
                return;
            }
            if (entries_[idx].key_hash == kTombstone && first_tombstone > mask_) 
                first_tombstone = idx;
            
            idx = (idx + 1) & mask_;
        }

        // 如果没找到现有的，优先插入到墓碑位置
        size_t insert_idx = (first_tombstone <= mask_) ? first_tombstone : idx;
        entries_[insert_idx].key_hash = hash;
        entries_[insert_idx].offset = off;
        entries_[insert_idx].len = len;
    }

    
    inline void erase(uint64_t hash) 
    {
        if (hash == kEmpty || hash == kTombstone) hash = 1;
        size_t idx = hash & mask_;
        while (entries_[idx].key_hash != kEmpty) 
        {
            if (entries_[idx].key_hash == hash) 
            {
                entries_[idx].key_hash = kTombstone; // 标记为墓碑
                return;
            }
            idx = (idx + 1) & mask_;
        }
    }
    
    inline bool get(uint64_t hash, uint64_t& off, uint32_t& len) const 
    {
        size_t idx = hash & mask_;
        while (entries_[idx].key_hash != 0)
        {
            if (entries_[idx].key_hash == hash) 
            {
                off = entries_[idx].offset;
                len = entries_[idx].len;
                return true;
            }
            idx = (idx + 1) & mask_;
        }
        return false;
    }

    size_t capacity() const 
    { 
        return capacity_; 
    }

    // 获取特定位置的 Entry
    IndexEntry& get_entry(size_t i) 
    { 
        return entries_[i]; 
    }

    static constexpr uint64_t kEmpty = 0;
    // 全 1
    static constexpr uint64_t kTombstone = ~0ULL;
private:
    IndexEntry* entries_;
    size_t capacity_;
    size_t mask_;
};

TITANKV_NAMESPACE_CLOSE