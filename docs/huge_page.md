# 优化：内核大页引入 (Huge_page)

## 1. 背景

在 TitanKV 的高并发写入场景下，频繁的内存分配成为了热路径上的隐形瓶颈。

TLB Miss: 在高内存占用下，使用默认的 4KB 页表会导致 CPU 的 TLB 频繁失效，增加内存访问延迟。

系统风暴: 初步尝试全量使用 mmap 导致了严重的性能回退。经分析发现，对小对象（如 4KB 请求）频繁调用 mmap 会引发系统调用风暴，且造成巨大的物理内存浪费。

## 2. 方案
为了兼顾小对象的分配速度和大块内存的 TLB 友好性，我实现了一个基于阈值的混合内存分配器。

### 策略:
小对象 (< 64KB): 继续使用 posix_memalign ，利用其 Thread Cache 机制实现纳秒级分配，避免陷入内核。
大对象 (>= 64KB): 切换为 mmap + MAP_HUGETLB + MAP_POPULATE，直接申请 2MB 大页。

### 应用范围:
主要应用于 批量提交 (通常在 100KB - 4MB 之间) 的内存映射。

## 3. 基准测试结果
** 不使用大页 **
<img width="1561" height="404" alt="image" src="https://github.com/user-attachments/assets/a54d031e-df52-4245-98ff-de83363dccfe" />

** 使用大页 **
<img width="1544" height="407" alt="image" src="https://github.com/user-attachments/assets/6e269881-46cf-429e-9689-56d91e71c452" />

## 4. 结论
<img width="1051" height="290" alt="image" src="https://github.com/user-attachments/assets/8ed0f10b-db1d-4942-bb92-00535da23663" />

在相同的 4 核虚拟化环境下，针对 100% 写入负载进行压测。结果显示，引入基于阈值的混合大页分配策略 (Hybrid HugePage Allocation) 后，系统性能获得显著提升：

**吞吐量提升**：系统整体 IOPS 从 5.4 万提升至 6.3 万，增幅达 17.2%。证明该策略有效降低了高频内存分配带来的系统开销。

**延迟降低**：平均单次操作耗时从 18.4 微秒缩短至 15.7 微秒，降低了 14.7%。这主要得益于大页内存对 TLB Miss 的抑制以及对 Group Commit 缓冲区的优化。

**结论**：通过避免对小对象（<64KB）滥用 mmap，同时确保关键路径（Group Buffer）使用大页，成功在系统调用开销与内存访问效率之间找到了最佳平衡点。

