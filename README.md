# TitanKV-IO: 一个基于io_uring的高性能I/O实验库

## 概述
本项目是一个探索Linux内核最新异步I/O接口`io_uring`极限性能的原型库。通过结合**Thread-Per-Core线程模型**、**无锁SPSC/MPSC队列**、**缓存行对齐**、**Linux内核调优**等技术，构建了一个从用户态到内核态完全异步、无锁竞争的高并发I/O管道。

**核心目标**：量化验证在纯软件层面，通过系统编程与并发架构设计，能多大程度压榨出现代硬件（多核CPU、高速SSD）的I/O性能。


## 性能演进与结果
**数据结构设计与演进**
测试环境：8核VMware + Ubuntu 22.04， 10万次随机写操作。
通过三个阶段，在vmware+ubuntu 22.04 的四核两线程场景中将总耗时从 **5.1秒降低至1.47秒，时间减少71，整体IO性能提升250%**。

1、基础I/O路径性能优化：实现了`io_uring` 替代 `pwrite`，将IO的时间从**5.1秒降低至2.4秒**，详情见test目录的test_iouring.cpp

2、并发功能扩展：实现多线程 + CPU亲和性绑定，将IO的时间从**2.4秒降低至1.81秒**，详情见test目录的test_mutithread.cpp

3、无锁SPSC队列：实现基于io_uring的SPSC的功能开发将IO的时间从**1.81秒降低至1.47秒**，详情见test目录的test_SPSCQueue.cpp。

**与RocksDB数据库引擎的对比**：

在与100% 随机写入、4KB Payload、强一致性落盘、SPSC架构下，**TitanKV 的吞吐量约 4.4k IOPS，是 RocksDB数据库引擎 的 4.186 倍**，详情见docs/titankv_vs_rocksdb_spsc.md。

在与100% 随机写入、4KB Payload、强一致性落盘、MPSC架构下，**TitanKV 的吞吐量约 4.4k IOPS，是 RocksDB数据库引擎 的 2.49 倍**，详情见docs/titankv_vs_rocksdb_mpsc.md。

**性能优化演进**

通过以下几个阶段，在vmware+ubuntu 22.04 的四核两程场景中将总耗时从 **1.47秒降低至0.9秒**，相较于上面的5.1秒，**时间减少82.35%，整体IO性能提升567%**。

1、**新增批处理机制**：显著提升系统吞吐量。在极端情况下提升约91%的IO性能。详情见docs/batch_submit.md。

2、**新增批内核大页机制**：在批处理的基础上新增内核大页分配。通过合理的内存管理，增加约 **17%吞吐量** ，减少约**14.7%时间开销** ，详情见docs/huge_page.md。

3、**新增固定文件注册机制**：在批处理的基础上新增固定文件注册新增。通过perf工具、分析内核源码，引入io_uring的fix_files机制增加约**30.9%吞吐量**，减少约**23.7%时间开销**，详情见docs/fixed_files.md。

4、**新增扁平索引与注册内存直通机制**：基于对 perf 报告中 libstdc++ (unordered_map) 指针跳转和 asm_exc_page_fault 缺页异常的深度分析，引入了 FlatIndex (开放寻址线性探测哈希表) 与 Registered Buffers (注册缓冲区) 机制。配合 SSE4.2 硬件加速哈希（CRC32）与 Registered Arena 内存模型，实现了热路径上的 Zero-Allocation 与 Zero-Mapping。
单机写入性能突破 21.1w IOPS，将内核调用开销压缩至 2% 以内。perf 采样显示核心业务逻辑占比飙升至 27.25%，系统瓶颈穿透用户态，下沉至内核磁盘驱动层。详情见 docs/zero_allocation.md。

## 后续目标

1、基于Linux内核的性能调优
