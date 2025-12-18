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

    // 默认一次扩容申请 128 个对象，避免频繁扩容
    explicit ObjectPool(size_t chunk_size = 128) : chunk_size_(chunk_size) {}

    ~ObjectPool() 
    {
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
        if (free_head_ == nullptr) 
        {
            expand();
        }

        assert(free_head_ != nullptr);

        FreeNode* node = free_head_;
        
        free_head_ = free_head_->next;

        T* t_ptr = reinterpret_cast<T*>(node);

        // new(addr) T 告诉编译器我已经有一块内存了，不需要再次分配内存
        // std::forward 如果传进来的args是右值，把他传递给T的构造函数可能会触发拷贝构造函数，
        // 而不是移动构造，使用std::forward是为了避免这种情况减少不必要的拷贝
        // ... 把args的参数打包，按，分割传递给T的构造函数 
        new (t_ptr) T(std::forward<Args>(args)...);
        
        return t_ptr;
    }


    void free(T* ptr) 
    {
        if (!ptr) return;

        ptr->~T();

        FreeNode* node = reinterpret_cast<FreeNode*>(ptr);
        
        node->next = free_head_;
        free_head_ = node;
    }

private:

    void expand() 
    {
        // 计算一次需要申请的总字节数
        size_t block_bytes = chunk_size_ * sizeof(T);
        
        void* raw_mem = nullptr;
        // 使用 64 字节对齐，不仅为了 cache line，也是为了后续 O_DIRECT 的对齐要求
        if (posix_memalign(&raw_mem, 64, block_bytes) != 0) 
        {
            throw std::bad_alloc();
        }

        blocks_.push_back(raw_mem);
        
        char* cursor = reinterpret_cast<char*>(raw_mem);
        
        for (size_t i = 0; i < chunk_size_ - 1; ++i) 
        {
            FreeNode* current = reinterpret_cast<FreeNode*>(cursor);

            char* next_addr = cursor + sizeof(T);

            current->next = reinterpret_cast<FreeNode*>(next_addr);

            cursor = next_addr;
        }

        FreeNode* tail = reinterpret_cast<FreeNode*>(cursor);
        // 链表插入，而不是新建链表
        tail->next = free_head_;
        free_head_ = reinterpret_cast<FreeNode*>(raw_mem);
    }

    // 自由链表头指针
    FreeNode* free_head_ = nullptr;
    // 记录所有申请过的大块内存，方便最后 free
    std::vector<void*> blocks_;
    // 每次扩容的个数
    size_t chunk_size_;
};

TITANKV_NAMESPACE_CLOSE