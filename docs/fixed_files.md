# 优化：Fixed Files (固定文件注册)

## 1. 背景与挑战
在 TitanKV 达到 10w 级 IOPS 后，通过 **perf** 采样发现了一个反直觉的现象：尽管使用了异步的 io_uring，但内核态入口函数 do_syscall_64 的 **CPU 占比依然接近 50%**。
<img width="1390" height="318" alt="image" src="https://github.com/user-attachments/assets/1300652b-d4cb-44e3-804e-8d65ad7fd799" />

**深度分析：**
通过查阅 Linux 5.10 内核源码，锁定了标准异步 IO 路径中的两个核心瓶颈：
**FD 查找开销** ：
内核在 fs/file.c 的 __fget_files 中通过 fcheck_files 查找 FD 对应的 struct file。虽然是 O(1) 操作，但高频调用依然会产生显著的 CPU 周期损耗。
<img width="1475" height="869" alt="image" src="https://github.com/user-attachments/assets/4a07a0b0-1835-46e3-bc3d-f89c5af34418" />

**引用计数原子竞争**：
在 __fget_files 中，内核必须调用 get_file_rcu_many。其底层通过 atomic_long_add_unless 对 file->f_count 执行原子加操作。
<img width="984" height="109" alt="image" src="https://github.com/user-attachments/assets/209d135a-b8e8-4807-b319-8dfee41b4615" />

**硬件瓶颈**：在高频写入场景下，多个 Worker 线程并发操作同一个 Log 文件的引用计数。这导致了强烈的**缓存行抖动**。CPU 硬件为了维持 f_count 的一致性，频繁触发 MESI 协议中的失效消息，这成为了阻塞单机吞吐量的性能瓶颈。

## 2. 方案：内核态文件对象常驻化
引入 io_uring 的 **固定文件注册** 机制，旨在实现全路径 **“零查找、零原子计数”**。
**核心动作：**
初始化阶段：通过 io_uring_register_files 将 Log 文件的 FD 预注册到内核 ring 的私有数组中。
提交阶段：在 SQE 请求中设置 IOSQE_FIXED_FILE 标志，直接通过数组索引（Index）提交。
效果：内核直接在 ring 的生命周期内持有文件引用，彻底消灭了热路径上的 fget/fput 原子开销。

## 3. 基准测试结果
**修改前后性能对比(左边为优化后，右边为优化前)：**
<img width="2814" height="1240" alt="image" src="https://github.com/user-attachments/assets/b579f0bf-34fc-4334-b975-1c38c9afa0a6" />

**结论**：100,000 次随机写入，4 核虚拟机，开启 Group Commit。
<img width="1256" height="378" alt="image" src="https://github.com/user-attachments/assets/72f426ed-89a3-4844-92c9-e41b54642333" />

