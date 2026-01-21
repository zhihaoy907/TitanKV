# 优化：标准库优化与全路径零分配

## 1. 标准库优化

### 1.1 背景
在 Fixed Files 优化后，通过 perf 分析发现：
<img width="2810" height="290" alt="image" src="https://github.com/user-attachments/assets/d4e06ba5-a589-46ed-93f4-9fa1da6c6f5d" />

现在的性能瓶颈在于频繁页分配。大概率是 unordered_map 的节点式分配触发了高频缺页中断。

### 1.2 方案
设计了一套 **FlatIndex** 接口来替代 **unordered_map** ，通过 **预分配连续内存** 来避免频繁页分配。

### 1.3 测试结果(优化后左，优化前右)
<img width="2812" height="504" alt="image" src="https://github.com/user-attachments/assets/658a3faf-b38e-411e-a209-d6ffc552aabb" />
显然clear_page_erms已经从热点前几消失。

## 2. 全路径零分配

### 2.1 背景
在优化了 unordered_map 通过 perf 发现：
<img width="2806" height="362" alt="image" src="https://github.com/user-attachments/assets/1dea39e1-9ce1-439a-b8df-34c82757fadf" />

CPU 仍有 1/4 的时间都在用户态和内核态之间切换，并且__mprotect的占比搞到15%，这说明热路径上依然存在频繁的大块内存申请/释放动作，每处理一个批次，都要进行一次内存保护状态的设置。

### 2.2 方案
将 GroupBuffer 转化为 Worker 成员变量进行持久复用。

### 2.3 测试结果(优化后左，优化前右)
<img width="2820" height="473" alt="image" src="https://github.com/user-attachments/assets/b6c7ee97-ec0d-41c7-beb4-6ce879d0d456" />
<img width="2816" height="873" alt="image" src="https://github.com/user-attachments/assets/9f78800b-fbe1-4c04-a485-794955eb577a" />

该优化在**IO性能受限**的机器上收效甚微，但是在更高性能的机器上TitanKV已基本将IO压榨到了极限，参考：
<img width="2020" height="549" alt="image" src="https://github.com/user-attachments/assets/00654e8f-4e38-4c55-ad3b-98d553c71c1d" />

<img width="2301" height="1165" alt="image" src="https://github.com/user-attachments/assets/811374cb-6f10-4dbe-a062-a7478a93c4c0" />

**核心指标**

1、业务逻辑：**run函数占比27.25%**，CPU 超过 1/4 的时间都在实打实地运行业务代码（搬运数据、哈希、处理批次）

2、系统调用：**do_syscall_64占比仅2.25%，libstdc++损耗几乎没有，sysmalloc / __mprotect：降到了 0.82% 和 0.41%**，现在几乎没有内核态跟用户态的上下文切换

3、磁盘驱动：**mpt_put_msg_frame合计约14.5%**。数据流已经基本塞满了IO总线，物理硬件成为性能瓶颈

4、内存抖动：**clear_page_erms、__mprotect合计小于1.5%**，已经 实现了真正的“零缺页中断”。IO 路径上的内存页已被预先 Pin 住并映射，彻底消灭了动态申请带来的内核态抖动

其中上述指标由于是VMware环境导致整体偏低，实际性能应该会更佳
