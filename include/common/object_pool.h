#pragma once

#include <vector>
#include <cassert>
#include <cstddef>
#include <utility>
#include <cstdlib>
#include "common/common.h"

TITANKV_NAMESPACE_OPEN

template <typename T>
class ObjectPool 
{
public:
    struct FreeNode 
    {
        FreeNode* next;
    };

    // 如果 T 的体积还没一个指针大，放不下这个 next 指针，就异常退出
    static_assert(sizeof(T) >= sizeof(FreeNode), "Object too small for intrusive freelist");
    // 缓存行对齐，避免多线程下的伪共享
    static constexpr size_t kCacheLineSize = CACHE_LINE_SIZE;
    static constexpr size_t item_size_ = (sizeof(T) + kCacheLineSize - 1) / kCacheLineSize * kCacheLineSize;

    // 默认一次扩容申请 128 个对象，避免频繁扩容
    explicit ObjectPool(size_t chunk_size = 128) 
    :   chunk_size_(chunk_size), 
        object_num_(0),
        in_use_(0)
    {}

    ~ObjectPool() 
    {
        assert(in_use_ == 0 && "ObjectPool destroyed with live objects");

        for (void* block : blocks_)
        {
            ::free(block);
        }
    }

    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    template <typename... Args>
    T* alloc(Args&&... args) 
    {
        // 如果链表是空，先扩容
        if (free_head_ == nullptr) [[unlikely]]
        {
            expand();
        }

        assert(free_head_ != nullptr);

        FreeNode* node = free_head_;
        
        free_head_ = free_head_->next;

        try
        {
            T* t_ptr = reinterpret_cast<T*>(node);

            // new(addr) T 告诉编译器我已经有一块内存了，不需要再次分配内存
            // std::forward 如果传进来的args是右值，把他传递给T的构造函数可能会触发拷贝构造函数，
            // 而不是移动构造，使用std::forward是为了避免这种情况减少不必要的拷贝
            // ... 把args的参数打包，按，分割传递给T的构造函数 
            new (t_ptr) T(std::forward<Args>(args)...);
            in_use_++;
            return t_ptr;
        }
        catch(const std::exception& e)
        {
            node->next = free_head_;
            free_head_ = node;
            throw; 
        }        
    }

    // 预热 使得系统抖动可控
    void reserve(size_t count)
    {
        while(object_num_ < count)
            expand();
    }


    void free(T* ptr) 
    {
        if (!ptr) [[unlikely]]
            return; 

        ptr->~T();

        FreeNode* node = reinterpret_cast<FreeNode*>(ptr);
        
        node->next = free_head_;
        free_head_ = node;
        in_use_--;
    }

private:

    void expand() 
    {
        // 计算一次需要申请的总字节数
        size_t block_bytes = chunk_size_ * item_size_;
        
        void* raw_mem = nullptr;
        // 使用 64 字节对齐，不仅为了 cache line，也是为了后续 O_DIRECT 的对齐要求
        if (posix_memalign(&raw_mem, 64, block_bytes) != 0) [[unlikely]]
        {
            throw std::bad_alloc();
        }

        blocks_.push_back(raw_mem);
        
        char* cursor = reinterpret_cast<char*>(raw_mem);
        
        for (size_t i = 0; i < chunk_size_ - 1; ++i) 
        {
            FreeNode* current = reinterpret_cast<FreeNode*>(cursor);

            char* next_addr = cursor + item_size_;

            current->next = reinterpret_cast<FreeNode*>(next_addr);

            cursor = next_addr;
        }

        FreeNode* tail = reinterpret_cast<FreeNode*>(cursor);
        // 链表插入，而不是新建链表
        tail->next = free_head_;
        free_head_ = reinterpret_cast<FreeNode*>(raw_mem);

        object_num_ += chunk_size_;
    }

    // 自由链表头指针
    FreeNode* free_head_ = nullptr;
    // 记录所有申请过的大块内存，方便最后 free
    std::vector<void*> blocks_;
    // 每次扩容的个数
    size_t chunk_size_;
    // 对象池中的对象个数
    unsigned object_num_;
    // 已使用的对象个数
    unsigned in_use_;
};

TITANKV_NAMESPACE_CLOSE
