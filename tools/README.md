# 🕵️ TitanKV eBPF 可观测性工具箱 (Observability Tools)
本目录包含了一套基于 eBPF (bpftrace) 的深度性能分析脚本。
这些工具旨在穿透用户态的迷雾，直接从 Linux 内核视角观测 TitanKV 的运行时行为。我们利用它们验证了架构设计的正确性（如无锁机制是否生效、Group Commit 是否聚合），并定位那些传统工具 (perf/top) 无法捕捉的微秒级抖动。
# 环境要求
OS: Linux Kernel 5.x 及以上（建议支持 BTF）。
工具: bpftrace
安装 (Ubuntu): sudo apt-get install bpftrace
# 🛠️ 工具列表 (Toolkit)
每个脚本的头部均包含详细的 使用说明、原理解析 及 实战案例数据，请直接查看脚本源文件。
## 1. 锁竞争分析 (analyze_lock.bt)
核心功能: 追踪 futex 系统调用。
解决痛点: 验证“无锁架构”是否名副其实。检测是否存在隐形的内核态锁等待（Futex Wait）。
典型战绩: 证明 TitanKV Worker 线程实现了 Zero Lock Contention，彻底消除了 RocksDB 中常见的护航效应 (Convoy Effect)。
## 2. CPU 迁移检测 (check_migration.bt)
核心功能: 追踪 sched_switch 事件中的跨核迁移。
解决痛点: 验证 pthread_setaffinity 绑核是否生效，检测 OS 调度器是否在做不必要的负载均衡。
典型战绩: 证实 TitanKV Worker 线程在全生命周期内 CPU 迁移次数为 0，最大化了 L1/L2 Cache 命中率。
## 3. IO 批处理检测 (trace_uring.bt)
核心功能: 追踪 io_uring_enter 系统调用及其参数。
解决痛点: 验证 Group Commit (组提交) 机制是否生效。
典型战绩: 证实 TitanKV 将 N 个用户态写请求聚合成 1 次 系统调用，相比传统 IO 模型减少了 98% 的 Syscall 开销。
# 📊 实战对比案例 (Case Study)
以下数据采集自 4 核 VM 环境，4KB 随机写入场景。详细报告见 docs/titanvsrocsdb.md
1. 锁竞争 (Lock Contention)
RocksDB (Sync模式): 触发 28241次 内核锁等待，长尾延迟高达 2ms。
TitanKV: 触发 16890次 等待（主要来自测试 Client 端），Worker 线程保持零竞争。
2. 上下文切换 (Context Switch)
RocksDB: 总计 315,728 次切换。CPU 大量时间浪费在调度开销上。
TitanKV: 总计 26,862 次切换。TPC 绑核策略成功减少了 91% 的调度开销。
⚠️ 注意事项
所有脚本均需 Root 权限 (sudo) 运行。
eBPF 脚本会对系统产生微小的 Overhead，建议仅在性能调优和排查问题时挂载。