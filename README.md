# TitanKV-IO: 一个基于io_uring的高性能I/O实验库

## 概述
本项目是一个探索Linux内核最新异步I/O接口`io_uring`极限性能的原型库。通过结合**Thread-Per-Core线程模型**、**无锁SPSC队列**及**缓存行对齐**等技术，构建了一个从用户态到内核态完全异步、无锁竞争的高并发I/O管道。

**核心目标**：量化验证在纯软件层面，通过系统编程与并发架构设计，能多大程度压榨出现代硬件（多核CPU、高速SSD）的I/O性能。

**现阶段性能基准测试结果**：

在与100% 随机写入、4KB Payload、强一致性落盘、SPSC架构下，**TitanKV 的吞吐量约 4.4k IOPS，是 RocksDB数据库引擎 的 4.186 倍**，详情见docs/TitanKV 与 RocksDB 在 SPSC 架构的性能对比。

在与100% 随机写入、4KB Payload、强一致性落盘、MPSC架构下，**TitanKV 的吞吐量约 4.4k IOPS，是 RocksDB数据库引擎 的 2.49 倍**，详情见docs/TitanKV 与 RocksDB 在 MPSC 架构的性能对比。

## 性能演进与结果
测试环境：8核VMware + Ubuntu 22.04， 10万次随机写操作。
通过三个阶段，在vmware+ubuntu 22.04 的八核六线程场景中将总耗时从 **5.1秒降低至1.47秒，时间减少71，整体IO性能提升250%**。

1、基础I/O路径性能优化：实现了`io_uring` 替代 `pwrite`，将IO的时间从**5.1秒降低至2.4秒**，详情见test目录的test_iouring.cpp

2、并发功能扩展：实现多线程 + CPU亲和性绑定，将IO的时间从**2.4秒降低至1.81秒**，详情见test目录的test_mutithread.cpp

3、无锁SPSC队列：实现基于io_uring的SPSC的功能开发将IO的时间从**1.81秒降低至1.47秒**，详情见test目录的test_SPSCQueue.cpp。


## 后续目标

1、基于Linux内核的性能调优
