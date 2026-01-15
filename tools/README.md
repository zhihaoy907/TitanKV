**TitanKV eBPF 可观测性工具箱 (Observability Tools)**
本目录包含了一套基于 eBPF (bpftrace) 的深度性能分析脚本。
这些工具旨在穿透用户态的迷雾，直接从 Linux 内核视角观测 TitanKV 的运行时行为。我们利用它们来验证架构设计的正确性（如无锁机制是否生效），并定位那些 perf 无法捕捉的深层瓶颈（如内核调度延迟、IO 提交阻塞）。
**环境要求**
OS: Linux Kernel 5.x 及以上（需要支持 BTF）。
工具: bpftrace。
Ubuntu 环境安装：
sudo apt-get install bpftrace

**核心目的:**
验证 TitanKV Thread-per-Core (TPC) 架构是否真正实现了“无锁化”。
该脚本追踪内核的 futex 系统调用，统计线程因争抢锁而陷入睡眠的频率和时长。
使用方法:
# 1. 启动压测程序，获取 PID (例如 12345)
# 2. 运行脚本
sudo ./analyze_lock.bt -p 12345
**关键指标解读:**
@total_waits: 进程内所有线程触发内核态锁等待的总次数。
@wait_ns: 等待耗时的直方图。
RocksDB: 通常呈现长尾分布（毫秒级），说明发生了严重的 Convoy Effect（护航效应）。
TitanKV: 应集中在微秒级（主要来自 Client 端的内存分配锁），Worker 线程应接近 0。
@waits_by_tid: 按线程 ID 统计。CoreWorker 线程的计数应极低。

**案例：TitanKV vs RocksDB 实战对比**
以下数据采集自 4 核 VM 环境，4KB 随机写入场景。
1. 锁竞争 (Futex Wait)
RocksDB (Sync模式): 触发了 28241次 内核锁等待，长尾延迟高达 2ms。
TitanKV: 触发了 16890次 等待（主要来自测试框架本身），P99 延迟稳定在 500us 以下。Worker 线程零竞争。
2. 上下文切换 (Context Switch)
RocksDB: 总计 315728 次切换 (Involuntary Context Switches)。CPU 大量时间浪费在调度上。
TitanKV: 总计 26862 次切换。TPC 绑核策略成功减少了 91% 的调度开销。
(详细输出见项目根目录 docs/titanvsrocsdb.md)
