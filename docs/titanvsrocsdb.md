** 程序输出结果对比 **

TitanKV 与 RocksDB 性能对比

本 README 总结了 TitanKV 与 RocksDB 在单线程环境下的基准测试结果，对比了两者的操作性能和吞吐量。

1. 测试环境

机器环境：虚拟机 (Linux)

磁盘 I/O：

TitanKV 使用 DirectIO + TPC

RocksDB 使用默认 WAL + MemTable

总操作数：25,000

线程数：1

测试工具：

bench_titankv_standalone (TitanKV)

bench_rocksdb_standalone (RocksDB)

2. 测试结果
2.1 TitanKV (DirectIO + TPC)
------------------------------------------------
[Bench] TitanKV (DirectIO + TPC)...
  Threads: 1
  Total Ops: 25000
PID: 23035
1. Open another terminal.
2. Run: sudo ../tools/analyze_lock.bt 23035
3. Press ENTER here to START.

  -> Time: 5.41949 s
  -> IOPS: 4612.98
  -> Throughput: 18.0194 MB/s

2.2 RocksDB (默认 WAL + MemTable)
------------------------------------------------
[Bench] RocksDB (Default: WAL + MemTable)...
PID: 23088
Press ENTER to start benchmark...

  -> Time: 22.6749 s
  -> IOPS: 1102.54
  -> Throughput: 4.3068 MB/s

3. 结果对比
数据库	总时间 (s)	IOPS	吞吐量 (MB/s)
TitanKV	5.42	4613	18.02
RocksDB	22.67	1103	4.31

分析：

在单线程环境下，TitanKV 的 IOPS 和吞吐量明显高于 RocksDB。

TitanKV 采用 DirectIO 和 TPC 的组合，可以减少内核缓存干扰，提高磁盘写入效率。


4. eBPF 锁竞争与系统调用分析
为了深入理解性能差异背后的系统级原因，我们使用 eBPF 工具对两个数据库在测试期间的内核行为进行了追踪，重点关注锁竞争和线程调度。

测试方法

工具：自定义 eBPF 脚本 analyze_lock.bt

追踪对象：数据库进程在基准测试期间的全部 FUTEX_WAIT 事件（用户态锁竞争的主要指标）和上下文切换。

目标：量化评估两者在高并发写入压力下的同步效率和线程阻塞情况。

4.1 锁竞争总览
下表汇总了核心的锁与调度指标，直接反映程序并发效率：

<img width="1823" height="569" alt="image" src="https://github.com/user-attachments/assets/ddd9ce05-ffbf-4c13-a7be-e0c72d7aa915" />

