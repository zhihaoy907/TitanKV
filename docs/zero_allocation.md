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

纯纯的负优化，我不理解
