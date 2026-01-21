#pragma once
#include <liburing.h>
#include <functional>

#include "common/buffer.h"
#include "common/common.h"
#include "common/object_pool.h"
#include "muti_thread/common.h"

TITANKV_NAMESPACE_OPEN

using WriteCompletionCallback = std::function<void(int bytes_transferred)>;
using ReadCompletionCallback = std::function<void(int, AlignedBuffer&)>;

struct IoRequest
{
    struct iovec iov;
    // IO 偏移量
    off_t offset;
    // 仅测试路径使用，用于保持生命周期 
    AlignedBuffer held_buffer;

    // --- 逻辑上下文 (用于读路径) ---
    // 存 CoreWorker* 
    void* worker_ptr;
    // 存读取长度
    uint32_t read_len;
    
    // --- 回调容器 ---
    // 读回调：统一使用用户层的 std::string 回调
    std::function<void(std::string)> read_cb;
    // 写回调：统一使用共享向量模式
    std::shared_ptr<std::vector<std::function<void(int)>>> write_cbs_vec;

    IoRequest() : offset(0), worker_ptr(nullptr), read_len(0) {}

    void reset() 
    {
        offset = 0;
        worker_ptr = nullptr;
        read_len = 0;
        read_cb = nullptr;
        write_cbs_vec = nullptr;
        // 释放临时 buffer
        held_buffer = AlignedBuffer(); 
    }
};

struct IoArena 
{
    void* ptr;
    size_t size;
    std::atomic<size_t> offset;
    bool is_huge;

    IoArena(size_t sz) : size(sz), offset(0), is_huge(false) 
    {
        // 优先尝试大页
        ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, 
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
        
        if (ptr != MAP_FAILED) 
        {
            is_huge = true;
        }
        else 
        {
            // 大页失败，回退到普通对齐内存
            if (posix_memalign(&ptr, 4096, size) != 0)
                throw std::runtime_error("Arena memory allocation failed");
            
            is_huge = false;
        }
        
        // 预热内存，防止运行期 Page Fault
        memset(ptr, 0, size); 
    }

    ~IoArena() 
    {
        if (is_huge) 
            munmap(ptr, size);
        else 
            free(ptr);
    }

    void* alloc(size_t sz) 
    {
        size_t aligned_sz = (sz + 4095) & ~4095;
        size_t off = offset.fetch_add(aligned_sz);

        if (off + aligned_sz > size) 
            return nullptr;
        return (char*)ptr + off;
    }
};

class IoContext
{
public:
    explicit IoContext(unsigned entries = 1024);

    ~IoContext();

    void SubmitWrite(int /*fd*/, 
                    const void* data, 
                    size_t len, 
                    off_t offset, 
                    std::vector<WriteRequest>& batch);

    void SubmitRead(int fd, 
                    AlignedBuffer&& buf, 
                    off_t offset, 
                    size_t len, 
                    void* worker, 
                    std::function<void(std::string)> cb);
    
    void SubmitWrite(int fd, AlignedBuffer&& buf, off_t offset, std::function<void(int)> cb);

    void RunOnce();

    void Submit();

    void Drain();

    void RegisterFiles(const std::vector<int>& fds);

    void* AllocateFromArena(size_t size);

private:
    struct io_uring ring_;
    ObjectPool<IoRequest> request_pool_; 
    void* arena_ptr_ = nullptr;
    size_t arena_size_ = 0;
    std::unique_ptr<IoArena> arena_;
};

TITANKV_NAMESPACE_CLOSE