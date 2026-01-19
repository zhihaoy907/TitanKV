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
        for (auto& block : blocks_) 
        {
            if (block.is_huge) 
            {
                ::munmap(block.ptr, block.size);
            } 
            else 
            {
                ::free(block.ptr);
            }
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

    inline unsigned in_use()
    {
        return in_use_;
    }

    void init_with_arena(void* ptr, size_t size) 
    {
        fixed_arena_cur_ = ptr;
    }

private:

    struct BlockInfo 
    {
        void* ptr;
        size_t size;
        bool is_huge;
    };

    void expand() 
    {
        size_t block_bytes = chunk_size_ * item_size_;
        void* raw_mem = nullptr;
        bool allocated_as_huge = false;
        bool from_arena = false;

        // 如果设置了外部 Arena 指针，优先使用
        if (fixed_arena_cur_) 
        {
            raw_mem = fixed_arena_cur_;
            // 移动 Arena 指针，为下次 expand 留位
            fixed_arena_cur_ = (char*)fixed_arena_cur_ + block_bytes;
            from_arena = true;
            // 假设 Arena 总是大页
            allocated_as_huge = true; 
        } 
        else 
        {
            raw_mem = mmap(nullptr, block_bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);

            if (raw_mem == MAP_FAILED) {
                if (posix_memalign(&raw_mem, kCacheLineSize, block_bytes) != 0) {
                    throw std::bad_alloc();
                }
                allocated_as_huge = false;
            } else {
                allocated_as_huge = true;
            }
        }
        
        // 如果内存来自 Arena，不要存入 blocks_，避免析构时重复 munmap
        if (!from_arena) 
            blocks_.push_back({raw_mem, block_bytes, allocated_as_huge});
        
        // 链表切分
        for (size_t i = 0; i < chunk_size_; ++i) 
        {
            FreeNode* current = reinterpret_cast<FreeNode*>((char*)raw_mem + i * item_size_);
            current->next = free_head_;
            free_head_ = current;
        }
        object_num_ += chunk_size_;
    }

    FreeNode* free_head_;
    std::vector<BlockInfo> blocks_;
    size_t chunk_size_;
    unsigned object_num_;
    unsigned in_use_;
    void* fixed_arena_cur_ = nullptr;

    inline void* allocate_huge_pages(size_t size) 
    {
        void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
        if (ptr == MAP_FAILED) return nullptr;
        return ptr;
    }
};

TITANKV_NAMESPACE_CLOSE
