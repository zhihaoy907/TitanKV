# TitanKV 与 RocksDB 性能对比

本 README 总结了 TitanKV 与 RocksDB 在单线程环境下的基准测试结果，对比了两者的操作性能和吞吐量。

# 1. 测试对比
1.1 TitanKV (DirectIO + TPC)
<img width="1389" height="442" alt="image" src="https://github.com/user-attachments/assets/acd75c9d-a1fc-45c1-85f0-123957d35e76" />


1.2 RocksDB (默认 WAL + MemTable)
<img width="1397" height="328" alt="image" src="https://github.com/user-attachments/assets/7afd91c2-1b05-4e3a-a473-19508e722273" />


# 分析：

**在单线程环境下，TitanKV 的 IOPS 和吞吐量明显高于 RocksDB。**

**TitanKV 采用 DirectIO 和 TPC 的组合，可以减少内核缓存干扰，提高磁盘写入效率。**


# 2. eBPF 锁竞争与系统调用分析
为了深入理解性能差异背后的系统级原因，我们使用 eBPF 工具对两个数据库在测试期间的内核行为进行了追踪，重点关注锁竞争和线程调度。

测试方法

工具：tools/analyze_lock.bt

追踪对象：数据库进程在基准测试期间的全部 FUTEX_WAIT 事件（用户态锁竞争的主要指标）和上下文切换。

目标：量化评估两者在高并发写入压力下的同步效率和线程阻塞情况。

**2.1 锁竞争总览**
下表汇总了核心的锁与调度指标，直接反映程序并发效率：

<img width="1823" height="569" alt="image" src="https://github.com/user-attachments/assets/ddd9ce05-ffbf-4c13-a7be-e0c72d7aa915" />

**TitanKV：绝大多数（>75%）的锁等待落在 128μs至512μs 的较低延迟区间，等待“既少且短”。**
**对比系统：绝大多数（>80%）锁等待集中在 512μs至1ms 的高延迟区间，并且出现了数量巨大的竞争事件（22518次），导致线程阻塞时间更长。**


详细输出见下：
(base) zhihaoy@virtual-machine:~/workspace/github/TitanKV/build$ sudo ../tools/analyze_lock.bt 23035
../tools/analyze_lock.bt:47:18-20: WARNING: comparison of integers of different signs: 'int32' and 'unsigned int64' can lead to undefined behavior
/ args->prev_pid == (uint64)pid /
                 ~~
Attaching 5 probes...
Tracing FUTEX_WAIT contention for PID 23051...
Collecting context switch statistics...
Hit Ctrl-C to end.

========== Futex Contention Summary (PID 23051) ==========

---- Total Futex Wait Count ----
@total_waits: 16890


---- Futex Wait Latency Histogram (ns) ----
@wait_ns: 
[512, 1K)              8 |                                                    |
[1K, 2K)             222 |@                                                   |
[2K, 4K)             304 |@                                                   |
[4K, 8K)              19 |                                                    |
[8K, 16K)            511 |@@@                                                 |
[16K, 32K)           119 |                                                    |
[32K, 64K)           156 |@                                                   |
[64K, 128K)          783 |@@@@@                                               |
[128K, 256K)        7912 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@|
[256K, 512K)        4978 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@                    |
[512K, 1M)           995 |@@@@@@                                              |
[1M, 2M)             373 |@@                                                  |
[2M, 4M)             367 |@@                                                  |
[4M, 8M)               6 |                                                    |
[8M, 16M)              5 |                                                    |
[16M, 32M)             5 |                                                    |
[32M, 64M)             7 |                                                    |
[64M, 128M)           13 |                                                    |
[128M, 256M)           9 |                                                    |
[256M, 512M)          58 |                                                    |
[512M, 1G)            13 |                                                    |
[1G, 2G)              16 |                                                    |
[2G, 4G)              23 |                                                    |


---- Futex Wait Count by Address ----
@wait_count[140071658156728]: 1
@wait_count[139438967190280]: 1
@wait_count[127731694807108]: 1
@wait_count[140069331452680]: 1
@wait_count[18709002780080]: 1
@wait_count[100230249621932]: 1
@wait_count[127731673114692]: 1
@wait_count[140071675073208]: 1
@wait_count[127437383981840]: 1
@wait_count[100230244458784]: 1
@wait_count[127731732666772]: 1
@wait_count[100658825090464]: 1
@wait_count[100658823976176]: 1
@wait_count[824638574432]: 1
@wait_count[18708877778344]: 1
@wait_count[127731654535236]: 1
@wait_count[99650151063200]: 1
@wait_count[130403864150064]: 1
@wait_count[100658823945568]: 1
@wait_count[131400162468616]: 1
@wait_count[130711828818192]: 1
@wait_count[102366167137088]: 1
@wait_count[127731641985092]: 1
@wait_count[100230244435560]: 1
@wait_count[18708877772872]: 1
@wait_count[130711837210896]: 1
@wait_count[99402073285784]: 1
@wait_count[130711820425488]: 1
@wait_count[70282845172616]: 2
@wait_count[140071658156856]: 2
@wait_count[127731732652364]: 2
@wait_count[106851438508864]: 2
@wait_count[103824434959160]: 2
@wait_count[100230244264704]: 2
@wait_count[130404105253160]: 2
@wait_count[58360016093168]: 2
@wait_count[824634183520]: 2
@wait_count[140735182348984]: 2
@wait_count[99905364722544]: 2
@wait_count[42112436]: 3
@wait_count[106376837255448]: 3
@wait_count[140071675073336]: 3
@wait_count[100658824055160]: 3
@wait_count[136268375764096]: 3
@wait_count[15204184489992]: 4
@wait_count[136268038105232]: 4
@wait_count[140730337290024]: 4
@wait_count[136268038062912]: 4
@wait_count[58360016102568]: 4
@wait_count[140071658156808]: 5
@wait_count[99905364722624]: 6
@wait_count[9225590653832]: 6
@wait_count[100230249621952]: 6
@wait_count[136268038021536]: 6
@wait_count[99905364722628]: 7
@wait_count[100230249621956]: 7
@wait_count[140722749159032]: 7
@wait_count[106376837248320]: 7
@wait_count[100658824054904]: 9
@wait_count[110917073343152]: 9
@wait_count[127731732531540]: 9
@wait_count[140071675073288]: 11
@wait_count[58360015880200]: 11
@wait_count[136268399075600]: 13
@wait_count[136268360585472]: 13
@wait_count[100230244434792]: 14
@wait_count[140732511318664]: 15
@wait_count[136268399075624]: 15
@wait_count[99650149505568]: 17
@wait_count[99650149511448]: 18
@wait_count[127731725449572]: 20
@wait_count[140726211580344]: 29
@wait_count[140722749159112]: 34
@wait_count[15204184569736]: 37
@wait_count[100658825087264]: 50
@wait_count[58360015960968]: 60
@wait_count[140732511318744]: 67
@wait_count[58360016530048]: 403
@wait_count[130711625400368]: 988
@wait_count[58360016530132]: 7458
@wait_count[58360016530128]: 7458


---- Futex Total Wait Time by Address (ns) ----
@wait_time_ns[100230244458784]: 2335
@wait_time_ns[130403864150064]: 3065
@wait_time_ns[100658825090464]: 3125
@wait_time_ns[100658823945568]: 3777
@wait_time_ns[130404105253160]: 6482
@wait_time_ns[18708877772872]: 9947
@wait_time_ns[18708877778344]: 10579
@wait_time_ns[127731732666772]: 11079
@wait_time_ns[99650151063200]: 11751
@wait_time_ns[140071658156728]: 13554
@wait_time_ns[136268038021536]: 15598
@wait_time_ns[136268399075600]: 24061
@wait_time_ns[100658824055160]: 27017
@wait_time_ns[127437383981840]: 27188
@wait_time_ns[106376837255448]: 30414
@wait_time_ns[136268399075624]: 32408
@wait_time_ns[42112436]: 36103
@wait_time_ns[140071675073208]: 42094
@wait_time_ns[15204184489992]: 43678
@wait_time_ns[136268375764096]: 74492
@wait_time_ns[102366167137088]: 90820
@wait_time_ns[127731732652364]: 99616
@wait_time_ns[140722749159032]: 100098
@wait_time_ns[99402073285784]: 108802
@wait_time_ns[136268038105232]: 111227
@wait_time_ns[70282845172616]: 141540
@wait_time_ns[99905364722544]: 151317
@wait_time_ns[136268038062912]: 165964
@wait_time_ns[100230244435560]: 225118
@wait_time_ns[58360016093168]: 266601
@wait_time_ns[140732511318664]: 288340
@wait_time_ns[58360015880200]: 319795
@wait_time_ns[100230249621932]: 325916
@wait_time_ns[106851438508864]: 365817
@wait_time_ns[100658823976176]: 475070
@wait_time_ns[130711837210896]: 480059
@wait_time_ns[9225590653832]: 560041
@wait_time_ns[58360016102568]: 564147
@wait_time_ns[100230244434792]: 599117
@wait_time_ns[18709002780080]: 649588
@wait_time_ns[103824434959160]: 871079
@wait_time_ns[99650149511448]: 1204522
@wait_time_ns[136268360585472]: 1292715
@wait_time_ns[100230244264704]: 1527870
@wait_time_ns[100658825087264]: 1542809
@wait_time_ns[100658824054904]: 2201473
@wait_time_ns[824638574432]: 2666082
@wait_time_ns[99650149505568]: 2824159
@wait_time_ns[106376837248320]: 2944843
@wait_time_ns[824634183520]: 4575597
@wait_time_ns[58360016530048]: 9655652
@wait_time_ns[15204184569736]: 11978660
@wait_time_ns[100230249621952]: 16038175
@wait_time_ns[58360015960968]: 20646552
@wait_time_ns[130711828818192]: 20872178
@wait_time_ns[100230249621956]: 37451411
@wait_time_ns[130711625400368]: 336558182
@wait_time_ns[130711820425488]: 497309233
@wait_time_ns[139438967190280]: 1000517174
@wait_time_ns[131400162468616]: 1082102020
@wait_time_ns[140069331452680]: 1732007420
@wait_time_ns[58360016530132]: 2645044430
@wait_time_ns[58360016530128]: 2691732935
@wait_time_ns[140071658156808]: 3007714299
@wait_time_ns[140071675073336]: 3987210500
@wait_time_ns[127731654535236]: 5001076796
@wait_time_ns[127731641985092]: 5001163268
@wait_time_ns[127731673114692]: 5001466745
@wait_time_ns[127731694807108]: 5001557454
@wait_time_ns[140071675073288]: 6010772439
@wait_time_ns[140726211580344]: 6479653338
@wait_time_ns[140071658156856]: 6990702627
@wait_time_ns[127731732531540]: 7651921691
@wait_time_ns[140735182348984]: 8000242112
@wait_time_ns[140730337290024]: 8000397383
@wait_time_ns[110917073343152]: 9004339752
@wait_time_ns[140722749159112]: 9413817832
@wait_time_ns[140732511318744]: 10001996302
@wait_time_ns[127731725449572]: 10007061357
@wait_time_ns[99905364722628]: 14928622485
@wait_time_ns[99905364722624]: 20135430005


---- Futex Wait Count by Thread ----
@waits_by_tid[1240]: 1
@waits_by_tid[19603]: 1
@waits_by_tid[19874]: 1
@waits_by_tid[19481]: 1
@waits_by_tid[19496]: 1
@waits_by_tid[19966]: 1
@waits_by_tid[19543]: 1
@waits_by_tid[1144]: 2
@waits_by_tid[19750]: 2
@waits_by_tid[19523]: 2
@waits_by_tid[19656]: 2
@waits_by_tid[16463]: 2
@waits_by_tid[2040]: 3
@waits_by_tid[23035]: 3
@waits_by_tid[19777]: 3
@waits_by_tid[20545]: 3
@waits_by_tid[19775]: 4
@waits_by_tid[19604]: 4
@waits_by_tid[19776]: 4
@waits_by_tid[1815]: 4
@waits_by_tid[19774]: 4
@waits_by_tid[19798]: 5
@waits_by_tid[21484]: 5
@waits_by_tid[1816]: 5
@waits_by_tid[19514]: 6
@waits_by_tid[1989]: 6
@waits_by_tid[1817]: 7
@waits_by_tid[847]: 9
@waits_by_tid[19802]: 9
@waits_by_tid[1814]: 10
@waits_by_tid[19575]: 10
@waits_by_tid[2124]: 10
@waits_by_tid[19573]: 16
@waits_by_tid[1793]: 19
@waits_by_tid[19807]: 20
@waits_by_tid[19570]: 30
@waits_by_tid[1999]: 34
@waits_by_tid[19773]: 37
@waits_by_tid[1799]: 45
@waits_by_tid[19741]: 45
@waits_by_tid[19778]: 60
@waits_by_tid[1930]: 62
@waits_by_tid[19742]: 97
@waits_by_tid[23053]: 265
@waits_by_tid[23036]: 318
@waits_by_tid[23037]: 407
@waits_by_tid[19797]: 15316


---- Context Switch Total Count ----
@ctx_switch_total: 26862


---- Context Switch Count by Thread ----
@ctx_switch_by_tid[2101]: 1
@ctx_switch_by_tid[21975]: 1
@ctx_switch_by_tid[876]: 1
@ctx_switch_by_tid[925]: 1
@ctx_switch_by_tid[22503]: 2
@ctx_switch_by_tid[1363]: 2
@ctx_switch_by_tid[11]: 2
@ctx_switch_by_tid[856]: 2
@ctx_switch_by_tid[2]: 2
@ctx_switch_by_tid[19523]: 3
@ctx_switch_by_tid[36]: 3
@ctx_switch_by_tid[35]: 3
@ctx_switch_by_tid[18]: 3
@ctx_switch_by_tid[553]: 3
@ctx_switch_by_tid[21502]: 3
@ctx_switch_by_tid[29]: 3
@ctx_switch_by_tid[850]: 4
@ctx_switch_by_tid[21499]: 4
@ctx_switch_by_tid[1658]: 5
@ctx_switch_by_tid[19604]: 5
@ctx_switch_by_tid[23]: 5
@ctx_switch_by_tid[555]: 5
@ctx_switch_by_tid[30]: 5
@ctx_switch_by_tid[22484]: 7
@ctx_switch_by_tid[321]: 7
@ctx_switch_by_tid[19799]: 7
@ctx_switch_by_tid[15008]: 8
@ctx_switch_by_tid[866]: 9
@ctx_switch_by_tid[19587]: 9
@ctx_switch_by_tid[21644]: 9
@ctx_switch_by_tid[15]: 10
@ctx_switch_by_tid[23049]: 10
@ctx_switch_by_tid[21634]: 10
@ctx_switch_by_tid[21176]: 10
@ctx_switch_by_tid[21486]: 10
@ctx_switch_by_tid[22029]: 10
@ctx_switch_by_tid[2127]: 13
@ctx_switch_by_tid[19474]: 17
@ctx_switch_by_tid[52]: 21
@ctx_switch_by_tid[16]: 32
@ctx_switch_by_tid[2124]: 36
@ctx_switch_by_tid[22642]: 45
@ctx_switch_by_tid[673]: 45
@ctx_switch_by_tid[7768]: 47
@ctx_switch_by_tid[19570]: 50
@ctx_switch_by_tid[21941]: 50
@ctx_switch_by_tid[19389]: 76
@ctx_switch_by_tid[1999]: 77
@ctx_switch_by_tid[20547]: 78
@ctx_switch_by_tid[397]: 88
@ctx_switch_by_tid[19741]: 98
@ctx_switch_by_tid[24]: 106
@ctx_switch_by_tid[19782]: 111
@ctx_switch_by_tid[2032]: 117
@ctx_switch_by_tid[23051]: 118
@ctx_switch_by_tid[712]: 123
@ctx_switch_by_tid[19742]: 133
@ctx_switch_by_tid[17]: 155
@ctx_switch_by_tid[1930]: 182
@ctx_switch_by_tid[22667]: 188
@ctx_switch_by_tid[20894]: 338
@ctx_switch_by_tid[19924]: 417
@ctx_switch_by_tid[23035]: 490
@ctx_switch_by_tid[2594]: 501
@ctx_switch_by_tid[23087]: 570
@ctx_switch_by_tid[23086]: 636
@ctx_switch_by_tid[1793]: 897
@ctx_switch_by_tid[22634]: 1099
@ctx_switch_by_tid[22481]: 1188
@ctx_switch_by_tid[0]: 18574



@ctx_switch_by_tid[2101]: 1
@ctx_switch_by_tid[21975]: 1
@ctx_switch_by_tid[876]: 1
@ctx_switch_by_tid[925]: 1
@ctx_switch_by_tid[22503]: 2
@ctx_switch_by_tid[1363]: 2
@ctx_switch_by_tid[11]: 2
@ctx_switch_by_tid[856]: 2
@ctx_switch_by_tid[2]: 2
@ctx_switch_by_tid[19523]: 3
@ctx_switch_by_tid[36]: 3
@ctx_switch_by_tid[35]: 3
@ctx_switch_by_tid[18]: 3
@ctx_switch_by_tid[553]: 3
@ctx_switch_by_tid[21502]: 3
@ctx_switch_by_tid[29]: 3
@ctx_switch_by_tid[850]: 4
@ctx_switch_by_tid[21499]: 4
@ctx_switch_by_tid[1658]: 5
@ctx_switch_by_tid[19604]: 5
@ctx_switch_by_tid[23]: 5
@ctx_switch_by_tid[555]: 5
@ctx_switch_by_tid[30]: 5
@ctx_switch_by_tid[22484]: 7
@ctx_switch_by_tid[321]: 7
@ctx_switch_by_tid[19799]: 7
@ctx_switch_by_tid[15008]: 8
@ctx_switch_by_tid[866]: 9
@ctx_switch_by_tid[19587]: 9
@ctx_switch_by_tid[21644]: 9
@ctx_switch_by_tid[15]: 10
@ctx_switch_by_tid[23049]: 10
@ctx_switch_by_tid[21634]: 10
@ctx_switch_by_tid[21176]: 10
@ctx_switch_by_tid[21486]: 10
@ctx_switch_by_tid[22029]: 10
@ctx_switch_by_tid[2127]: 13
@ctx_switch_by_tid[19474]: 17
@ctx_switch_by_tid[52]: 21
@ctx_switch_by_tid[16]: 32
@ctx_switch_by_tid[2124]: 36
@ctx_switch_by_tid[22642]: 45
@ctx_switch_by_tid[673]: 45
@ctx_switch_by_tid[7768]: 47
@ctx_switch_by_tid[19570]: 50
@ctx_switch_by_tid[21941]: 50
@ctx_switch_by_tid[19389]: 76
@ctx_switch_by_tid[1999]: 77
@ctx_switch_by_tid[20547]: 78
@ctx_switch_by_tid[397]: 88
@ctx_switch_by_tid[19741]: 98
@ctx_switch_by_tid[24]: 106
@ctx_switch_by_tid[19782]: 111
@ctx_switch_by_tid[2032]: 117
@ctx_switch_by_tid[23051]: 118
@ctx_switch_by_tid[712]: 123
@ctx_switch_by_tid[19742]: 133
@ctx_switch_by_tid[17]: 155
@ctx_switch_by_tid[1930]: 182
@ctx_switch_by_tid[22667]: 188
@ctx_switch_by_tid[20894]: 338
@ctx_switch_by_tid[19924]: 417
@ctx_switch_by_tid[23035]: 490
@ctx_switch_by_tid[2594]: 501
@ctx_switch_by_tid[23087]: 570
@ctx_switch_by_tid[23086]: 636
@ctx_switch_by_tid[1793]: 897
@ctx_switch_by_tid[22634]: 1099
@ctx_switch_by_tid[22481]: 1188
@ctx_switch_by_tid[0]: 18574

@ctx_switch_total: 26862

@total_waits: 16890

@wait_addr[1144]: 824634183520
@wait_addr[1145]: 824634185312
@wait_addr[1240]: 824638574432
@wait_addr[16463]: 824642861408
@wait_addr[19750]: 15204184934056
@wait_addr[19797]: 58360016530128
@wait_addr[19498]: 99402073285568
@wait_addr[19497]: 99402073285568
@wait_addr[19499]: 99402073285572
@wait_addr[19496]: 99402073285572
@wait_addr[19776]: 99905364722624
@wait_addr[19774]: 99905364722624
@wait_addr[19775]: 99905364722624
@wait_addr[19777]: 99905364722628
@wait_addr[1817]: 100230249621952
@wait_addr[1814]: 100230249621952
@wait_addr[1816]: 100230249621956
@wait_addr[1815]: 100230249621956
@wait_addr[1142]: 103824434959136
@wait_addr[19770]: 106851438097344
@wait_addr[19771]: 106851438097344
@wait_addr[19772]: 106851438097344
@wait_addr[19769]: 106851438097344
@wait_addr[847]: 110917073343152
@wait_addr[20545]: 127731641985092
@wait_addr[19966]: 127731654535236
@wait_addr[21484]: 127731673114692
@wait_addr[19874]: 127731694807108
@wait_addr[19802]: 127731732531540
@wait_addr[19481]: 131400162468664
@wait_addr[19543]: 139438967190328
@wait_addr[19603]: 140069331452728
@wait_addr[19575]: 140071658156856
@wait_addr[19573]: 140071675073336
@wait_addr[19741]: 140722749159112
@wait_addr[19570]: 140726211580344
@wait_addr[19604]: 140730337290024
@wait_addr[19742]: 140732511318744
@wait_addr[19523]: 140735182348984

@wait_count[140071658156728]: 1
@wait_count[139438967190280]: 1
@wait_count[127731694807108]: 1
@wait_count[140069331452680]: 1
@wait_count[18709002780080]: 1
@wait_count[100230249621932]: 1
@wait_count[127731673114692]: 1
@wait_count[140071675073208]: 1
@wait_count[127437383981840]: 1
@wait_count[100230244458784]: 1
@wait_count[127731732666772]: 1
@wait_count[100658825090464]: 1
@wait_count[100658823976176]: 1
@wait_count[824638574432]: 1
@wait_count[18708877778344]: 1
@wait_count[127731654535236]: 1
@wait_count[99650151063200]: 1
@wait_count[130403864150064]: 1
@wait_count[100658823945568]: 1
@wait_count[131400162468616]: 1
@wait_count[130711828818192]: 1
@wait_count[102366167137088]: 1
@wait_count[127731641985092]: 1
@wait_count[100230244435560]: 1
@wait_count[18708877772872]: 1
@wait_count[130711837210896]: 1
@wait_count[99402073285784]: 1
@wait_count[130711820425488]: 1
@wait_count[70282845172616]: 2
@wait_count[140071658156856]: 2
@wait_count[127731732652364]: 2
@wait_count[106851438508864]: 2
@wait_count[103824434959160]: 2
@wait_count[100230244264704]: 2
@wait_count[130404105253160]: 2
@wait_count[58360016093168]: 2
@wait_count[824634183520]: 2
@wait_count[140735182348984]: 2
@wait_count[99905364722544]: 2
@wait_count[42112436]: 3
@wait_count[106376837255448]: 3
@wait_count[140071675073336]: 3
@wait_count[100658824055160]: 3
@wait_count[136268375764096]: 3
@wait_count[15204184489992]: 4
@wait_count[136268038105232]: 4
@wait_count[140730337290024]: 4
@wait_count[136268038062912]: 4
@wait_count[58360016102568]: 4
@wait_count[140071658156808]: 5
@wait_count[99905364722624]: 6
@wait_count[9225590653832]: 6
@wait_count[100230249621952]: 6
@wait_count[136268038021536]: 6
@wait_count[99905364722628]: 7
@wait_count[100230249621956]: 7
@wait_count[140722749159032]: 7
@wait_count[106376837248320]: 7
@wait_count[100658824054904]: 9
@wait_count[110917073343152]: 9
@wait_count[127731732531540]: 9
@wait_count[140071675073288]: 11
@wait_count[58360015880200]: 11
@wait_count[136268399075600]: 13
@wait_count[136268360585472]: 13
@wait_count[100230244434792]: 14
@wait_count[140732511318664]: 15
@wait_count[136268399075624]: 15
@wait_count[99650149505568]: 17
@wait_count[99650149511448]: 18
@wait_count[127731725449572]: 20
@wait_count[140726211580344]: 29
@wait_count[140722749159112]: 34
@wait_count[15204184569736]: 37
@wait_count[100658825087264]: 50
@wait_count[58360015960968]: 60
@wait_count[140732511318744]: 67
@wait_count[58360016530048]: 403
@wait_count[130711625400368]: 988
@wait_count[58360016530132]: 7458
@wait_count[58360016530128]: 7458

@wait_ns: 
[512, 1K)              8 |                                                    |
[1K, 2K)             222 |@                                                   |
[2K, 4K)             304 |@                                                   |
[4K, 8K)              19 |                                                    |
[8K, 16K)            511 |@@@                                                 |
[16K, 32K)           119 |                                                    |
[32K, 64K)           156 |@                                                   |
[64K, 128K)          783 |@@@@@                                               |
[128K, 256K)        7912 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@|
[256K, 512K)        4978 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@                    |
[512K, 1M)           995 |@@@@@@                                              |
[1M, 2M)             373 |@@                                                  |
[2M, 4M)             367 |@@                                                  |
[4M, 8M)               6 |                                                    |
[8M, 16M)              5 |                                                    |
[16M, 32M)             5 |                                                    |
[32M, 64M)             7 |                                                    |
[64M, 128M)           13 |                                                    |
[128M, 256M)           9 |                                                    |
[256M, 512M)          58 |                                                    |
[512M, 1G)            13 |                                                    |
[1G, 2G)              16 |                                                    |
[2G, 4G)              23 |                                                    |

@wait_start[19769]: 43476295114794
@wait_start[19771]: 43478798289040
@wait_start[19481]: 43479000447217
@wait_start[19497]: 43479307652694
@wait_start[19498]: 43479307665376
@wait_start[19772]: 43481297914580
@wait_start[19966]: 43481609445294
@wait_start[19874]: 43481609573639
@wait_start[1815]: 43481955794535
@wait_start[1816]: 43481956437713
@wait_start[1817]: 43481956922670
@wait_start[1814]: 43481956931225
@wait_start[19543]: 43482000547387
@wait_start[19603]: 43482000665687
@wait_start[19770]: 43483799505377
@wait_start[21484]: 43484109955287
@wait_start[20545]: 43484110038033
@wait_start[19777]: 43484216683067
@wait_start[1240]: 43484254952891
@wait_start[16463]: 43484255057745
@wait_start[1144]: 43484255058216
@wait_start[1145]: 43484255076979
@wait_start[1142]: 43484260186510
@wait_start[19802]: 43484260438200
@wait_start[19499]: 43484319449505
@wait_start[19496]: 43484319487021
@wait_start[19797]: 43484646479324
@wait_start[19750]: 43484733335397
@wait_start[847]: 43485605276767
@wait_start[19741]: 43485725613175
@wait_start[19570]: 43485765639469
@wait_start[19523]: 43485857841024
@wait_start[19776]: 43485871032829
@wait_start[19775]: 43485871082536
@wait_start[19774]: 43485871100087
@wait_start[19742]: 43485871341955
@wait_start[19575]: 43486000271353
@wait_start[19573]: 43486000279757
@wait_start[19604]: 43486091074311

@wait_time_ns[100230244458784]: 2335
@wait_time_ns[130403864150064]: 3065
@wait_time_ns[100658825090464]: 3125
@wait_time_ns[100658823945568]: 3777
@wait_time_ns[130404105253160]: 6482
@wait_time_ns[18708877772872]: 9947
@wait_time_ns[18708877778344]: 10579
@wait_time_ns[127731732666772]: 11079
@wait_time_ns[99650151063200]: 11751
@wait_time_ns[140071658156728]: 13554
@wait_time_ns[136268038021536]: 15598
@wait_time_ns[136268399075600]: 24061
@wait_time_ns[100658824055160]: 27017
@wait_time_ns[127437383981840]: 27188
@wait_time_ns[106376837255448]: 30414
@wait_time_ns[136268399075624]: 32408
@wait_time_ns[42112436]: 36103
@wait_time_ns[140071675073208]: 42094
@wait_time_ns[15204184489992]: 43678
@wait_time_ns[136268375764096]: 74492
@wait_time_ns[102366167137088]: 90820
@wait_time_ns[127731732652364]: 99616
@wait_time_ns[140722749159032]: 100098
@wait_time_ns[99402073285784]: 108802
@wait_time_ns[136268038105232]: 111227
@wait_time_ns[70282845172616]: 141540
@wait_time_ns[99905364722544]: 151317
@wait_time_ns[136268038062912]: 165964
@wait_time_ns[100230244435560]: 225118
@wait_time_ns[58360016093168]: 266601
@wait_time_ns[140732511318664]: 288340
@wait_time_ns[58360015880200]: 319795
@wait_time_ns[100230249621932]: 325916
@wait_time_ns[106851438508864]: 365817
@wait_time_ns[100658823976176]: 475070
@wait_time_ns[130711837210896]: 480059
@wait_time_ns[9225590653832]: 560041
@wait_time_ns[58360016102568]: 564147
@wait_time_ns[100230244434792]: 599117
@wait_time_ns[18709002780080]: 649588
@wait_time_ns[103824434959160]: 871079
@wait_time_ns[99650149511448]: 1204522
@wait_time_ns[136268360585472]: 1292715
@wait_time_ns[100230244264704]: 1527870
@wait_time_ns[100658825087264]: 1542809
@wait_time_ns[100658824054904]: 2201473
@wait_time_ns[824638574432]: 2666082
@wait_time_ns[99650149505568]: 2824159
@wait_time_ns[106376837248320]: 2944843
@wait_time_ns[824634183520]: 4575597
@wait_time_ns[58360016530048]: 9655652
@wait_time_ns[15204184569736]: 11978660
@wait_time_ns[100230249621952]: 16038175
@wait_time_ns[58360015960968]: 20646552
@wait_time_ns[130711828818192]: 20872178
@wait_time_ns[100230249621956]: 37451411
@wait_time_ns[130711625400368]: 336558182
@wait_time_ns[130711820425488]: 497309233
@wait_time_ns[139438967190280]: 1000517174
@wait_time_ns[131400162468616]: 1082102020
@wait_time_ns[140069331452680]: 1732007420
@wait_time_ns[58360016530132]: 2645044430
@wait_time_ns[58360016530128]: 2691732935
@wait_time_ns[140071658156808]: 3007714299
@wait_time_ns[140071675073336]: 3987210500
@wait_time_ns[127731654535236]: 5001076796
@wait_time_ns[127731641985092]: 5001163268
@wait_time_ns[127731673114692]: 5001466745
@wait_time_ns[127731694807108]: 5001557454
@wait_time_ns[140071675073288]: 6010772439
@wait_time_ns[140726211580344]: 6479653338
@wait_time_ns[140071658156856]: 6990702627
@wait_time_ns[127731732531540]: 7651921691
@wait_time_ns[140735182348984]: 8000242112
@wait_time_ns[140730337290024]: 8000397383
@wait_time_ns[110917073343152]: 9004339752
@wait_time_ns[140722749159112]: 9413817832
@wait_time_ns[140732511318744]: 10001996302
@wait_time_ns[127731725449572]: 10007061357
@wait_time_ns[99905364722628]: 14928622485
@wait_time_ns[99905364722624]: 20135430005

@waits_by_tid[1240]: 1
@waits_by_tid[19603]: 1
@waits_by_tid[19874]: 1
@waits_by_tid[19481]: 1
@waits_by_tid[19496]: 1
@waits_by_tid[19966]: 1
@waits_by_tid[19543]: 1
@waits_by_tid[1144]: 2
@waits_by_tid[19750]: 2
@waits_by_tid[19523]: 2
@waits_by_tid[19656]: 2
@waits_by_tid[16463]: 2
@waits_by_tid[2040]: 3
@waits_by_tid[23035]: 3
@waits_by_tid[19777]: 3
@waits_by_tid[20545]: 3
@waits_by_tid[19775]: 4
@waits_by_tid[19604]: 4
@waits_by_tid[19776]: 4
@waits_by_tid[1815]: 4
@waits_by_tid[19774]: 4
@waits_by_tid[19798]: 5
@waits_by_tid[21484]: 5
@waits_by_tid[1816]: 5
@waits_by_tid[19514]: 6
@waits_by_tid[1989]: 6
@waits_by_tid[1817]: 7
@waits_by_tid[847]: 9
@waits_by_tid[19802]: 9
@waits_by_tid[1814]: 10
@waits_by_tid[19575]: 10
@waits_by_tid[2124]: 10
@waits_by_tid[19573]: 16
@waits_by_tid[1793]: 19
@waits_by_tid[19807]: 20
@waits_by_tid[19570]: 30
@waits_by_tid[1999]: 34
@waits_by_tid[19773]: 37
@waits_by_tid[1799]: 45
@waits_by_tid[19741]: 45
@waits_by_tid[19778]: 60
@waits_by_tid[1930]: 62
@waits_by_tid[19742]: 97
@waits_by_tid[23053]: 265
@waits_by_tid[23036]: 318
@waits_by_tid[23037]: 407
@waits_by_tid[19797]: 15316

(base) zhihaoy@virtual-machine:~/workspace/github/TitanKV/build$ sudo ../tools/analyze_lock.bt 23088
../tools/analyze_lock.bt:47:18-20: WARNING: comparison of integers of different signs: 'int32' and 'unsigned int64' can lead to undefined behavior
/ args->prev_pid == (uint64)pid /
                 ~~
Attaching 5 probes...
Tracing FUTEX_WAIT contention for PID 23091...
Collecting context switch statistics...
Hit Ctrl-C to end.

========== Futex Contention Summary (PID 23091) ==========

---- Total Futex Wait Count ----
@total_waits: 28241


---- Futex Wait Latency Histogram (ns) ----
@wait_ns: 
[512, 1K)              1 |                                                    |
[1K, 2K)             105 |                                                    |
[2K, 4K)              51 |                                                    |
[4K, 8K)               6 |                                                    |
[8K, 16K)            123 |                                                    |
[16K, 32K)           177 |                                                    |
[32K, 64K)           299 |                                                    |
[64K, 128K)          904 |@@                                                  |
[128K, 256K)         600 |@                                                   |
[256K, 512K)         340 |                                                    |
[512K, 1M)         22518 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@|
[1M, 2M)            2472 |@@@@@                                               |
[2M, 4M)              99 |                                                    |
[4M, 8M)              30 |                                                    |
[8M, 16M)             22 |                                                    |
[16M, 32M)            14 |                                                    |
[32M, 64M)            15 |                                                    |
[64M, 128M)           61 |                                                    |
[128M, 256M)          38 |                                                    |
[256M, 512M)         193 |                                                    |
[512M, 1G)            32 |                                                    |
[1G, 2G)              40 |                                                    |
[2G, 4G)             103 |                                                    |


---- Futex Wait Count by Address ----
@wait_count[127731732666404]: 1
@wait_count[100230249621932]: 1
@wait_count[108669846481560]: 1
@wait_count[824638572640]: 1
@wait_count[99402073285784]: 1
@wait_count[103824434959136]: 1
@wait_count[824634185312]: 1
@wait_count[139438967190280]: 1
@wait_count[18708950110896]: 1
@wait_count[106851438508864]: 1
@wait_count[100658825090464]: 1
@wait_count[100230244423164]: 1
@wait_count[108669846476344]: 1
@wait_count[140069257581144]: 1
@wait_count[132648705972496]: 1
@wait_count[100658824055160]: 1
@wait_count[140071675073208]: 1
@wait_count[127437383981840]: 1
@wait_count[132649291086096]: 1
@wait_count[108669846466788]: 1
@wait_count[127731732538220]: 1
@wait_count[140726211580264]: 1
@wait_count[132649192519952]: 1
@wait_count[18708877863896]: 1
@wait_count[140069345435968]: 1
@wait_count[100230244264704]: 1
@wait_count[132649184127248]: 1
@wait_count[100658824005776]: 1
@wait_count[140069345436480]: 1
@wait_count[18708877772872]: 1
@wait_count[108669846466784]: 1
@wait_count[9225590426168]: 2
@wait_count[132914747146560]: 2
@wait_count[106851438097264]: 2
@wait_count[136268038062912]: 2
@wait_count[140069257581064]: 2
@wait_count[15204184489992]: 2
@wait_count[18709001824392]: 2
@wait_count[99650151063200]: 2
@wait_count[9225590426252]: 2
@wait_count[99402073285548]: 2
@wait_count[15204184933976]: 2
@wait_count[136268038105232]: 2
@wait_count[100230244423160]: 2
@wait_count[130403864150064]: 2
@wait_count[127437676601640]: 3
@wait_count[15204184934056]: 3
@wait_count[108669846466032]: 3
@wait_count[127437676601616]: 3
@wait_count[15204184934060]: 3
@wait_count[106376837360784]: 3
@wait_count[102366167137088]: 3
@wait_count[99905364722840]: 3
@wait_count[136268038021536]: 3
@wait_count[42112436]: 3
@wait_count[106376837255448]: 3
@wait_count[106376837293056]: 3
@wait_count[140069331452680]: 3
@wait_count[100230249621928]: 3
@wait_count[37766724]: 4
@wait_count[70282845172616]: 4
@wait_count[140071658156856]: 4
@wait_count[106851438097344]: 4
@wait_count[106376837248320]: 4
@wait_count[106851438097348]: 4
@wait_count[58360016102568]: 5
@wait_count[140071675073336]: 5
@wait_count[136268399075624]: 5
@wait_count[99402073285488]: 5
@wait_count[99650149572688]: 5
@wait_count[136268375764096]: 5
@wait_count[140722749159032]: 5
@wait_count[99905364722604]: 6
@wait_count[100658824055672]: 7
@wait_count[140735182348984]: 7
@wait_count[99650149505568]: 8
@wait_count[127731673114692]: 9
@wait_count[99905364722544]: 9
@wait_count[127731694807108]: 9
@wait_count[127731654535236]: 9
@wait_count[130404105253160]: 10
@wait_count[140730337290024]: 10
@wait_count[127731641985092]: 10
@wait_count[140069368057784]: 10
@wait_count[100230244434792]: 11
@wait_count[100230249621956]: 11
@wait_count[99650149511448]: 11
@wait_count[58360015880200]: 11
@wait_count[130404101237888]: 12
@wait_count[130404105253136]: 12
@wait_count[136268399075600]: 13
@wait_count[100230249621952]: 13
@wait_count[140071658156808]: 14
@wait_count[99905364722624]: 14
@wait_count[99402073285568]: 14
@wait_count[99402073285572]: 14
@wait_count[136268360585472]: 15
@wait_count[99905364722628]: 20
@wait_count[100658824054904]: 21
@wait_count[140732511318664]: 25
@wait_count[140071675073288]: 26
@wait_count[110917073343152]: 27
@wait_count[9225590653832]: 29
@wait_count[127731732531540]: 38
@wait_count[100658823945568]: 40
@wait_count[100658825087264]: 43
@wait_count[127731725449572]: 55
@wait_count[140726211580344]: 127
@wait_count[15204184569736]: 167
@wait_count[140722749159112]: 173
@wait_count[140732511318744]: 228
@wait_count[58360015960968]: 243
@wait_count[58360016530048]: 1445
@wait_count[58360016530128]: 12553
@wait_count[58360016530132]: 12553


---- Futex Total Wait Time by Address (ns) ----
@wait_time_ns[18708877863896]: 1813
@wait_time_ns[100658824005776]: 2274
@wait_time_ns[100230244264704]: 2976
@wait_time_ns[130403864150064]: 4788
@wait_time_ns[140069345435968]: 4979
@wait_time_ns[127437676601640]: 5931
@wait_time_ns[100658825090464]: 6101
@wait_time_ns[127437676601616]: 6191
@wait_time_ns[100658824055160]: 6412
@wait_time_ns[99650149572688]: 8225
@wait_time_ns[136268038021536]: 8906
@wait_time_ns[140071675073208]: 11590
@wait_time_ns[18708877772872]: 11781
@wait_time_ns[99402073285784]: 13624
@wait_time_ns[127437383981840]: 14555
@wait_time_ns[99650151063200]: 14677
@wait_time_ns[140069257581144]: 16278
@wait_time_ns[18709001824392]: 16599
@wait_time_ns[140069257581064]: 17961
@wait_time_ns[130404105253136]: 18262
@wait_time_ns[130404105253160]: 19987
@wait_time_ns[106851438097264]: 22189
@wait_time_ns[130404101237888]: 22821
@wait_time_ns[15204184933976]: 24062
@wait_time_ns[136268038105232]: 24824
@wait_time_ns[136268399075624]: 26106
@wait_time_ns[9225590426168]: 29332
@wait_time_ns[136268038062912]: 30104
@wait_time_ns[106376837255448]: 32758
@wait_time_ns[106376837360784]: 34061
@wait_time_ns[106376837293056]: 38698
@wait_time_ns[15204184489992]: 67829
@wait_time_ns[136268375764096]: 68790
@wait_time_ns[100658823945568]: 79241
@wait_time_ns[140726211580264]: 85351
@wait_time_ns[106851438508864]: 95438
@wait_time_ns[18708950110896]: 95689
@wait_time_ns[132649291086096]: 99251
@wait_time_ns[132649192519952]: 132254
@wait_time_ns[100230244423164]: 134738
@wait_time_ns[58360016102568]: 144535
@wait_time_ns[102366167137088]: 183043
@wait_time_ns[127731732538220]: 188964
@wait_time_ns[100230249621932]: 194502
@wait_time_ns[140722749159032]: 194815
@wait_time_ns[132649184127248]: 221412
@wait_time_ns[58360015880200]: 240505
@wait_time_ns[106376837248320]: 278613
@wait_time_ns[100230249621928]: 331741
@wait_time_ns[100230244423160]: 386592
@wait_time_ns[132914747146560]: 400689
@wait_time_ns[70282845172616]: 401640
@wait_time_ns[99905364722840]: 466886
@wait_time_ns[127731732666404]: 498247
@wait_time_ns[42112436]: 614705
@wait_time_ns[140732511318664]: 636186
@wait_time_ns[99905364722604]: 843130
@wait_time_ns[108669846476344]: 942907
@wait_time_ns[99402073285548]: 949788
@wait_time_ns[99650149505568]: 957080
@wait_time_ns[99905364722544]: 1269085
@wait_time_ns[99402073285488]: 1272708
@wait_time_ns[140069345436480]: 1381553
@wait_time_ns[99650149511448]: 1394486
@wait_time_ns[100658825087264]: 1479166
@wait_time_ns[100230244434792]: 1495074
@wait_time_ns[136268399075600]: 1812645
@wait_time_ns[136268360585472]: 1985751
@wait_time_ns[100658824055672]: 2305776
@wait_time_ns[100658824054904]: 3005620
@wait_time_ns[37766724]: 3234115
@wait_time_ns[140069368057784]: 5065463
@wait_time_ns[9225590653832]: 14472475
@wait_time_ns[9225590426252]: 27099704
@wait_time_ns[58360015960968]: 55882091
@wait_time_ns[15204184569736]: 72409428
@wait_time_ns[824634185312]: 99173733
@wait_time_ns[58360016530048]: 156631671
@wait_time_ns[140069331452680]: 1637665064
@wait_time_ns[139438967190280]: 1894543009
@wait_time_ns[140071658156808]: 6718745338
@wait_time_ns[15204184934060]: 7730430309
@wait_time_ns[108669846466788]: 8014675571
@wait_time_ns[100230249621956]: 10066924494
@wait_time_ns[58360016530132]: 11108431198
@wait_time_ns[58360016530128]: 11206196148
@wait_time_ns[140071675073288]: 11397417779
@wait_time_ns[140071675073336]: 12754951896
@wait_time_ns[108669846466784]: 14573153254
@wait_time_ns[140071658156856]: 16915420990
@wait_time_ns[103824434959136]: 17263550048
@wait_time_ns[824638572640]: 17275734951
@wait_time_ns[127731641985092]: 22501011642
@wait_time_ns[127731694807108]: 22502038035
@wait_time_ns[132648705972496]: 22674854738
@wait_time_ns[108669846481560]: 22691335836
@wait_time_ns[140730337290024]: 24000808522
@wait_time_ns[140735182348984]: 24001602316
@wait_time_ns[127731654535236]: 24779719736
@wait_time_ns[127731673114692]: 25279779022
@wait_time_ns[140722749159112]: 25357832433
@wait_time_ns[127731732531540]: 25434095224
@wait_time_ns[140726211580344]: 25792137157
@wait_time_ns[140732511318744]: 25939332649
@wait_time_ns[110917073343152]: 27007200554
@wait_time_ns[127731725449572]: 27515664961
@wait_time_ns[100230249621952]: 29994548467
@wait_time_ns[106851438097344]: 32054466344
@wait_time_ns[15204184934056]: 34423711130
@wait_time_ns[106851438097348]: 37872630572
@wait_time_ns[99402073285572]: 40149727627
@wait_time_ns[99905364722624]: 46951063317
@wait_time_ns[99905364722628]: 49844174839
@wait_time_ns[99402073285568]: 50184970898
@wait_time_ns[108669846466032]: 68075729967


---- Futex Wait Count by Thread ----
@waits_by_tid[19543]: 1
@waits_by_tid[23093]: 1
@waits_by_tid[19574]: 1
@waits_by_tid[23095]: 1
@waits_by_tid[23094]: 1
@waits_by_tid[1145]: 1
@waits_by_tid[19799]: 1
@waits_by_tid[23097]: 1
@waits_by_tid[1142]: 1
@waits_by_tid[1226]: 1
@waits_by_tid[19772]: 2
@waits_by_tid[19750]: 2
@waits_by_tid[19486]: 2
@waits_by_tid[19769]: 2
@waits_by_tid[19488]: 2
@waits_by_tid[19474]: 3
@waits_by_tid[19770]: 3
@waits_by_tid[23096]: 3
@waits_by_tid[19748]: 3
@waits_by_tid[19771]: 3
@waits_by_tid[19603]: 3
@waits_by_tid[23088]: 4
@waits_by_tid[19749]: 4
@waits_by_tid[2124]: 4
@waits_by_tid[19656]: 4
@waits_by_tid[22655]: 5
@waits_by_tid[19496]: 7
@waits_by_tid[19523]: 7
@waits_by_tid[1816]: 8
@waits_by_tid[19499]: 8
@waits_by_tid[1817]: 8
@waits_by_tid[19498]: 8
@waits_by_tid[1815]: 9
@waits_by_tid[2126]: 9
@waits_by_tid[21484]: 10
@waits_by_tid[19578]: 10
@waits_by_tid[19604]: 10
@waits_by_tid[19497]: 10
@waits_by_tid[19874]: 10
@waits_by_tid[19775]: 11
@waits_by_tid[1814]: 11
@waits_by_tid[19774]: 12
@waits_by_tid[19777]: 12
@waits_by_tid[19966]: 12
@waits_by_tid[19776]: 12
@waits_by_tid[20545]: 13
@waits_by_tid[1999]: 15
@waits_by_tid[2040]: 18
@waits_by_tid[1793]: 19
@waits_by_tid[19575]: 20
@waits_by_tid[847]: 27
@waits_by_tid[19514]: 29
@waits_by_tid[1799]: 33
@waits_by_tid[19573]: 34
@waits_by_tid[19802]: 38
@waits_by_tid[19807]: 55
@waits_by_tid[1989]: 60
@waits_by_tid[1930]: 90
@waits_by_tid[19570]: 131
@waits_by_tid[19773]: 167
@waits_by_tid[19741]: 182
@waits_by_tid[19778]: 243
@waits_by_tid[19742]: 275
@waits_by_tid[19797]: 26551


---- Context Switch Total Count ----
@ctx_switch_total: 315728


---- Context Switch Count by Thread ----
@ctx_switch_by_tid[1928]: 1
@ctx_switch_by_tid[1626]: 1
@ctx_switch_by_tid[2024]: 1
@ctx_switch_by_tid[21502]: 1
@ctx_switch_by_tid[2307]: 1
@ctx_switch_by_tid[860]: 1
@ctx_switch_by_tid[2101]: 2
@ctx_switch_by_tid[361]: 2
@ctx_switch_by_tid[1363]: 2
@ctx_switch_by_tid[876]: 3
@ctx_switch_by_tid[925]: 3
@ctx_switch_by_tid[1]: 3
@ctx_switch_by_tid[22503]: 3
@ctx_switch_by_tid[1658]: 4
@ctx_switch_by_tid[866]: 5
@ctx_switch_by_tid[23089]: 5
@ctx_switch_by_tid[676]: 5
@ctx_switch_by_tid[850]: 5
@ctx_switch_by_tid[553]: 6
@ctx_switch_by_tid[35]: 7
@ctx_switch_by_tid[18]: 7
@ctx_switch_by_tid[23]: 7
@ctx_switch_by_tid[19523]: 8
@ctx_switch_by_tid[29]: 8
@ctx_switch_by_tid[555]: 9
@ctx_switch_by_tid[15]: 10
@ctx_switch_by_tid[2127]: 11
@ctx_switch_by_tid[19604]: 12
@ctx_switch_by_tid[19799]: 17
@ctx_switch_by_tid[19587]: 23
@ctx_switch_by_tid[23088]: 26
@ctx_switch_by_tid[22484]: 27
@ctx_switch_by_tid[21176]: 28
@ctx_switch_by_tid[21634]: 28
@ctx_switch_by_tid[22029]: 29
@ctx_switch_by_tid[21486]: 30
@ctx_switch_by_tid[1999]: 43
@ctx_switch_by_tid[2124]: 45
@ctx_switch_by_tid[36]: 45
@ctx_switch_by_tid[19389]: 49
@ctx_switch_by_tid[15008]: 53
@ctx_switch_by_tid[52]: 55
@ctx_switch_by_tid[21644]: 61
@ctx_switch_by_tid[21975]: 65
@ctx_switch_by_tid[9973]: 76
@ctx_switch_by_tid[19474]: 78
@ctx_switch_by_tid[7768]: 101
@ctx_switch_by_tid[673]: 113
@ctx_switch_by_tid[397]: 120
@ctx_switch_by_tid[30]: 138
@ctx_switch_by_tid[21941]: 141
@ctx_switch_by_tid[23086]: 143
@ctx_switch_by_tid[19570]: 152
@ctx_switch_by_tid[1930]: 167
@ctx_switch_by_tid[17]: 229
@ctx_switch_by_tid[19741]: 263
@ctx_switch_by_tid[23091]: 294
@ctx_switch_by_tid[2032]: 295
@ctx_switch_by_tid[24]: 328
@ctx_switch_by_tid[712]: 332
@ctx_switch_by_tid[19742]: 390
@ctx_switch_by_tid[16]: 445
@ctx_switch_by_tid[2594]: 554
@ctx_switch_by_tid[22634]: 608
@ctx_switch_by_tid[19782]: 630
@ctx_switch_by_tid[1793]: 1095
@ctx_switch_by_tid[21499]: 2840
@ctx_switch_by_tid[20894]: 3984
@ctx_switch_by_tid[19924]: 8022
@ctx_switch_by_tid[21500]: 10253
@ctx_switch_by_tid[11]: 26246
@ctx_switch_by_tid[321]: 80643
@ctx_switch_by_tid[0]: 176530



@ctx_switch_by_tid[1928]: 1
@ctx_switch_by_tid[1626]: 1
@ctx_switch_by_tid[2024]: 1
@ctx_switch_by_tid[21502]: 1
@ctx_switch_by_tid[2307]: 1
@ctx_switch_by_tid[860]: 1
@ctx_switch_by_tid[2101]: 2
@ctx_switch_by_tid[361]: 2
@ctx_switch_by_tid[1363]: 2
@ctx_switch_by_tid[876]: 3
@ctx_switch_by_tid[925]: 3
@ctx_switch_by_tid[1]: 3
@ctx_switch_by_tid[22503]: 3
@ctx_switch_by_tid[1658]: 4
@ctx_switch_by_tid[866]: 5
@ctx_switch_by_tid[23089]: 5
@ctx_switch_by_tid[676]: 5
@ctx_switch_by_tid[850]: 5
@ctx_switch_by_tid[553]: 6
@ctx_switch_by_tid[35]: 7
@ctx_switch_by_tid[18]: 7
@ctx_switch_by_tid[23]: 7
@ctx_switch_by_tid[19523]: 8
@ctx_switch_by_tid[29]: 8
@ctx_switch_by_tid[555]: 9
@ctx_switch_by_tid[15]: 10
@ctx_switch_by_tid[2127]: 11
@ctx_switch_by_tid[19604]: 12
@ctx_switch_by_tid[19799]: 17
@ctx_switch_by_tid[19587]: 23
@ctx_switch_by_tid[23088]: 26
@ctx_switch_by_tid[22484]: 27
@ctx_switch_by_tid[21176]: 28
@ctx_switch_by_tid[21634]: 28
@ctx_switch_by_tid[22029]: 29
@ctx_switch_by_tid[21486]: 30
@ctx_switch_by_tid[1999]: 43
@ctx_switch_by_tid[2124]: 45
@ctx_switch_by_tid[36]: 45
@ctx_switch_by_tid[19389]: 49
@ctx_switch_by_tid[15008]: 53
@ctx_switch_by_tid[52]: 55
@ctx_switch_by_tid[21644]: 61
@ctx_switch_by_tid[21975]: 65
@ctx_switch_by_tid[9973]: 76
@ctx_switch_by_tid[19474]: 78
@ctx_switch_by_tid[7768]: 101
@ctx_switch_by_tid[673]: 113
@ctx_switch_by_tid[397]: 120
@ctx_switch_by_tid[30]: 138
@ctx_switch_by_tid[21941]: 141
@ctx_switch_by_tid[23086]: 143
@ctx_switch_by_tid[19570]: 152
@ctx_switch_by_tid[1930]: 167
@ctx_switch_by_tid[17]: 229
@ctx_switch_by_tid[19741]: 263
@ctx_switch_by_tid[23091]: 294
@ctx_switch_by_tid[2032]: 295
@ctx_switch_by_tid[24]: 328
@ctx_switch_by_tid[712]: 332
@ctx_switch_by_tid[19742]: 390
@ctx_switch_by_tid[16]: 445
@ctx_switch_by_tid[2594]: 554
@ctx_switch_by_tid[22634]: 608
@ctx_switch_by_tid[19782]: 630
@ctx_switch_by_tid[1793]: 1095
@ctx_switch_by_tid[21499]: 2840
@ctx_switch_by_tid[20894]: 3984
@ctx_switch_by_tid[19924]: 8022
@ctx_switch_by_tid[21500]: 10253
@ctx_switch_by_tid[11]: 26246
@ctx_switch_by_tid[321]: 80643
@ctx_switch_by_tid[0]: 176530

@ctx_switch_total: 315728

@total_waits: 28241

@wait_addr[1144]: 824634183520
@wait_addr[1226]: 824638572640
@wait_addr[19488]: 9225590426248
@wait_addr[19487]: 9225590426248
@wait_addr[19486]: 9225590426248
@wait_addr[19749]: 15204184934056
@wait_addr[19748]: 15204184934056
@wait_addr[19750]: 15204184934060
@wait_addr[19762]: 58360016324268
@wait_addr[19797]: 58360016530128
@wait_addr[19499]: 99402073285572
@wait_addr[19498]: 99402073285572
@wait_addr[19497]: 99402073285572
@wait_addr[19496]: 99402073285572
@wait_addr[19774]: 99905364722628
@wait_addr[19776]: 99905364722628
@wait_addr[19777]: 99905364722628
@wait_addr[19775]: 99905364722628
@wait_addr[1816]: 100230249621952
@wait_addr[1815]: 100230249621952
@wait_addr[1817]: 100230249621952
@wait_addr[1814]: 100230249621952
@wait_addr[1142]: 103824434959136
@wait_addr[19769]: 106851438097344
@wait_addr[19772]: 106851438097348
@wait_addr[19771]: 106851438097348
@wait_addr[19770]: 106851438097348
@wait_addr[847]: 110917073343152
@wait_addr[20545]: 127731641985092
@wait_addr[19966]: 127731654535236
@wait_addr[21484]: 127731673114692
@wait_addr[19874]: 127731694807108
@wait_addr[19807]: 127731725449572
@wait_addr[19802]: 127731732531540
@wait_addr[19597]: 130245569774184
@wait_addr[19543]: 139438967190328
@wait_addr[23048]: 140067481076488
@wait_addr[22655]: 140069257581144
@wait_addr[19603]: 140069331452728
@wait_addr[19575]: 140071658156856
@wait_addr[19573]: 140071675073336
@wait_addr[19741]: 140722749159112
@wait_addr[19570]: 140726211580344
@wait_addr[19604]: 140730337290024
@wait_addr[19742]: 140732511318744
@wait_addr[19523]: 140735182348984

@wait_count[127731732666404]: 1
@wait_count[100230249621932]: 1
@wait_count[108669846481560]: 1
@wait_count[824638572640]: 1
@wait_count[99402073285784]: 1
@wait_count[103824434959136]: 1
@wait_count[824634185312]: 1
@wait_count[139438967190280]: 1
@wait_count[18708950110896]: 1
@wait_count[106851438508864]: 1
@wait_count[100658825090464]: 1
@wait_count[100230244423164]: 1
@wait_count[108669846476344]: 1
@wait_count[140069257581144]: 1
@wait_count[132648705972496]: 1
@wait_count[100658824055160]: 1
@wait_count[140071675073208]: 1
@wait_count[127437383981840]: 1
@wait_count[132649291086096]: 1
@wait_count[108669846466788]: 1
@wait_count[127731732538220]: 1
@wait_count[140726211580264]: 1
@wait_count[132649192519952]: 1
@wait_count[18708877863896]: 1
@wait_count[140069345435968]: 1
@wait_count[100230244264704]: 1
@wait_count[132649184127248]: 1
@wait_count[100658824005776]: 1
@wait_count[140069345436480]: 1
@wait_count[18708877772872]: 1
@wait_count[108669846466784]: 1
@wait_count[9225590426168]: 2
@wait_count[132914747146560]: 2
@wait_count[106851438097264]: 2
@wait_count[136268038062912]: 2
@wait_count[140069257581064]: 2
@wait_count[15204184489992]: 2
@wait_count[18709001824392]: 2
@wait_count[99650151063200]: 2
@wait_count[9225590426252]: 2
@wait_count[99402073285548]: 2
@wait_count[15204184933976]: 2
@wait_count[136268038105232]: 2
@wait_count[100230244423160]: 2
@wait_count[130403864150064]: 2
@wait_count[127437676601640]: 3
@wait_count[15204184934056]: 3
@wait_count[108669846466032]: 3
@wait_count[127437676601616]: 3
@wait_count[15204184934060]: 3
@wait_count[106376837360784]: 3
@wait_count[102366167137088]: 3
@wait_count[99905364722840]: 3
@wait_count[136268038021536]: 3
@wait_count[42112436]: 3
@wait_count[106376837255448]: 3
@wait_count[106376837293056]: 3
@wait_count[140069331452680]: 3
@wait_count[100230249621928]: 3
@wait_count[37766724]: 4
@wait_count[70282845172616]: 4
@wait_count[140071658156856]: 4
@wait_count[106851438097344]: 4
@wait_count[106376837248320]: 4
@wait_count[106851438097348]: 4
@wait_count[58360016102568]: 5
@wait_count[140071675073336]: 5
@wait_count[136268399075624]: 5
@wait_count[99402073285488]: 5
@wait_count[99650149572688]: 5
@wait_count[136268375764096]: 5
@wait_count[140722749159032]: 5
@wait_count[99905364722604]: 6
@wait_count[100658824055672]: 7
@wait_count[140735182348984]: 7
@wait_count[99650149505568]: 8
@wait_count[127731673114692]: 9
@wait_count[99905364722544]: 9
@wait_count[127731694807108]: 9
@wait_count[127731654535236]: 9
@wait_count[130404105253160]: 10
@wait_count[140730337290024]: 10
@wait_count[127731641985092]: 10
@wait_count[140069368057784]: 10
@wait_count[100230244434792]: 11
@wait_count[100230249621956]: 11
@wait_count[99650149511448]: 11
@wait_count[58360015880200]: 11
@wait_count[130404101237888]: 12
@wait_count[130404105253136]: 12
@wait_count[136268399075600]: 13
@wait_count[100230249621952]: 13
@wait_count[140071658156808]: 14
@wait_count[99905364722624]: 14
@wait_count[99402073285568]: 14
@wait_count[99402073285572]: 14
@wait_count[136268360585472]: 15
@wait_count[99905364722628]: 20
@wait_count[100658824054904]: 21
@wait_count[140732511318664]: 25
@wait_count[140071675073288]: 26
@wait_count[110917073343152]: 27
@wait_count[9225590653832]: 29
@wait_count[127731732531540]: 38
@wait_count[100658823945568]: 40
@wait_count[100658825087264]: 43
@wait_count[127731725449572]: 55
@wait_count[140726211580344]: 127
@wait_count[15204184569736]: 167
@wait_count[140722749159112]: 173
@wait_count[140732511318744]: 228
@wait_count[58360015960968]: 243
@wait_count[58360016530048]: 1445
@wait_count[58360016530128]: 12553
@wait_count[58360016530132]: 12553

@wait_ns: 
[512, 1K)              1 |                                                    |
[1K, 2K)             105 |                                                    |
[2K, 4K)              51 |                                                    |
[4K, 8K)               6 |                                                    |
[8K, 16K)            123 |                                                    |
[16K, 32K)           177 |                                                    |
[32K, 64K)           299 |                                                    |
[64K, 128K)          904 |@@                                                  |
[128K, 256K)         600 |@                                                   |
[256K, 512K)         340 |                                                    |
[512K, 1M)         22518 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@|
[1M, 2M)            2472 |@@@@@                                               |
[2M, 4M)              99 |                                                    |
[4M, 8M)              30 |                                                    |
[8M, 16M)             22 |                                                    |
[16M, 32M)            14 |                                                    |
[32M, 64M)            15 |                                                    |
[64M, 128M)           61 |                                                    |
[128M, 256M)          38 |                                                    |
[256M, 512M)         193 |                                                    |
[512M, 1G)            32 |                                                    |
[1G, 2G)              40 |                                                    |
[2G, 4G)             103 |                                                    |

@wait_start[19487]: 43509359281023
@wait_start[19486]: 43509367106903
@wait_start[19488]: 43509367654722
@wait_start[19603]: 43511000245307
@wait_start[1144]: 43514228083553
@wait_start[19597]: 43521201671441
@wait_start[19762]: 43521856524751
@wait_start[1816]: 43522958253589
@wait_start[1815]: 43522958259019
@wait_start[1814]: 43522958555052
@wait_start[1817]: 43522958657803
@wait_start[19749]: 43525973035252
@wait_start[19543]: 43526000207254
@wait_start[19771]: 43526332309751
@wait_start[19772]: 43528832483431
@wait_start[19748]: 43529361804449
@wait_start[19750]: 43529362266967
@wait_start[19497]: 43529425487818
@wait_start[19496]: 43529425499337
@wait_start[19770]: 43531333407157
@wait_start[19797]: 43531505225051
@wait_start[1226]: 43531604242294
@wait_start[1142]: 43531614329141
@wait_start[19874]: 43531624569170
@wait_start[20545]: 43531624583916
@wait_start[19575]: 43533000529589
@wait_start[19573]: 43533000550806
@wait_start[19769]: 43533835467025
@wait_start[19741]: 43533857817533
@wait_start[19523]: 43533863273486
@wait_start[19604]: 43534097946303
@wait_start[19966]: 43534125228696
@wait_start[21484]: 43534125248301
@wait_start[23048]: 43534133671675
@wait_start[19802]: 43534275624022
@wait_start[19774]: 43534296958206
@wait_start[19776]: 43534296970369
@wait_start[19775]: 43534297014166
@wait_start[19777]: 43534297023543
@wait_start[19742]: 43534297080994
@wait_start[19499]: 43534438321829
@wait_start[19498]: 43534438325978
@wait_start[847]: 43534621140530
@wait_start[22655]: 43534742781995
@wait_start[19570]: 43534742848723
@wait_start[19807]: 43535197150415

@wait_time_ns[18708877863896]: 1813
@wait_time_ns[100658824005776]: 2274
@wait_time_ns[100230244264704]: 2976
@wait_time_ns[130403864150064]: 4788
@wait_time_ns[140069345435968]: 4979
@wait_time_ns[127437676601640]: 5931
@wait_time_ns[100658825090464]: 6101
@wait_time_ns[127437676601616]: 6191
@wait_time_ns[100658824055160]: 6412
@wait_time_ns[99650149572688]: 8225
@wait_time_ns[136268038021536]: 8906
@wait_time_ns[140071675073208]: 11590
@wait_time_ns[18708877772872]: 11781
@wait_time_ns[99402073285784]: 13624
@wait_time_ns[127437383981840]: 14555
@wait_time_ns[99650151063200]: 14677
@wait_time_ns[140069257581144]: 16278
@wait_time_ns[18709001824392]: 16599
@wait_time_ns[140069257581064]: 17961
@wait_time_ns[130404105253136]: 18262
@wait_time_ns[130404105253160]: 19987
@wait_time_ns[106851438097264]: 22189
@wait_time_ns[130404101237888]: 22821
@wait_time_ns[15204184933976]: 24062
@wait_time_ns[136268038105232]: 24824
@wait_time_ns[136268399075624]: 26106
@wait_time_ns[9225590426168]: 29332
@wait_time_ns[136268038062912]: 30104
@wait_time_ns[106376837255448]: 32758
@wait_time_ns[106376837360784]: 34061
@wait_time_ns[106376837293056]: 38698
@wait_time_ns[15204184489992]: 67829
@wait_time_ns[136268375764096]: 68790
@wait_time_ns[100658823945568]: 79241
@wait_time_ns[140726211580264]: 85351
@wait_time_ns[106851438508864]: 95438
@wait_time_ns[18708950110896]: 95689
@wait_time_ns[132649291086096]: 99251
@wait_time_ns[132649192519952]: 132254
@wait_time_ns[100230244423164]: 134738
@wait_time_ns[58360016102568]: 144535
@wait_time_ns[102366167137088]: 183043
@wait_time_ns[127731732538220]: 188964
@wait_time_ns[100230249621932]: 194502
@wait_time_ns[140722749159032]: 194815
@wait_time_ns[132649184127248]: 221412
@wait_time_ns[58360015880200]: 240505
@wait_time_ns[106376837248320]: 278613
@wait_time_ns[100230249621928]: 331741
@wait_time_ns[100230244423160]: 386592
@wait_time_ns[132914747146560]: 400689
@wait_time_ns[70282845172616]: 401640
@wait_time_ns[99905364722840]: 466886
@wait_time_ns[127731732666404]: 498247
@wait_time_ns[42112436]: 614705
@wait_time_ns[140732511318664]: 636186
@wait_time_ns[99905364722604]: 843130
@wait_time_ns[108669846476344]: 942907
@wait_time_ns[99402073285548]: 949788
@wait_time_ns[99650149505568]: 957080
@wait_time_ns[99905364722544]: 1269085
@wait_time_ns[99402073285488]: 1272708
@wait_time_ns[140069345436480]: 1381553
@wait_time_ns[99650149511448]: 1394486
@wait_time_ns[100658825087264]: 1479166
@wait_time_ns[100230244434792]: 1495074
@wait_time_ns[136268399075600]: 1812645
@wait_time_ns[136268360585472]: 1985751
@wait_time_ns[100658824055672]: 2305776
@wait_time_ns[100658824054904]: 3005620
@wait_time_ns[37766724]: 3234115
@wait_time_ns[140069368057784]: 5065463
@wait_time_ns[9225590653832]: 14472475
@wait_time_ns[9225590426252]: 27099704
@wait_time_ns[58360015960968]: 55882091
@wait_time_ns[15204184569736]: 72409428
@wait_time_ns[824634185312]: 99173733
@wait_time_ns[58360016530048]: 156631671
@wait_time_ns[140069331452680]: 1637665064
@wait_time_ns[139438967190280]: 1894543009
@wait_time_ns[140071658156808]: 6718745338
@wait_time_ns[15204184934060]: 7730430309
@wait_time_ns[108669846466788]: 8014675571
@wait_time_ns[100230249621956]: 10066924494
@wait_time_ns[58360016530132]: 11108431198
@wait_time_ns[58360016530128]: 11206196148
@wait_time_ns[140071675073288]: 11397417779
@wait_time_ns[140071675073336]: 12754951896
@wait_time_ns[108669846466784]: 14573153254
@wait_time_ns[140071658156856]: 16915420990
@wait_time_ns[103824434959136]: 17263550048
@wait_time_ns[824638572640]: 17275734951
@wait_time_ns[127731641985092]: 22501011642
@wait_time_ns[127731694807108]: 22502038035
@wait_time_ns[132648705972496]: 22674854738
@wait_time_ns[108669846481560]: 22691335836
@wait_time_ns[140730337290024]: 24000808522
@wait_time_ns[140735182348984]: 24001602316
@wait_time_ns[127731654535236]: 24779719736
@wait_time_ns[127731673114692]: 25279779022
@wait_time_ns[140722749159112]: 25357832433
@wait_time_ns[127731732531540]: 25434095224
@wait_time_ns[140726211580344]: 25792137157
@wait_time_ns[140732511318744]: 25939332649
@wait_time_ns[110917073343152]: 27007200554
@wait_time_ns[127731725449572]: 27515664961
@wait_time_ns[100230249621952]: 29994548467
@wait_time_ns[106851438097344]: 32054466344
@wait_time_ns[15204184934056]: 34423711130
@wait_time_ns[106851438097348]: 37872630572
@wait_time_ns[99402073285572]: 40149727627
@wait_time_ns[99905364722624]: 46951063317
@wait_time_ns[99905364722628]: 49844174839
@wait_time_ns[99402073285568]: 50184970898
@wait_time_ns[108669846466032]: 68075729967

@waits_by_tid[19543]: 1
@waits_by_tid[23093]: 1
@waits_by_tid[19574]: 1
@waits_by_tid[23095]: 1
@waits_by_tid[23094]: 1
@waits_by_tid[1145]: 1
@waits_by_tid[19799]: 1
@waits_by_tid[23097]: 1
@waits_by_tid[1142]: 1
@waits_by_tid[1226]: 1
@waits_by_tid[19772]: 2
@waits_by_tid[19750]: 2
@waits_by_tid[19486]: 2
@waits_by_tid[19769]: 2
@waits_by_tid[19488]: 2
@waits_by_tid[19474]: 3
@waits_by_tid[19770]: 3
@waits_by_tid[23096]: 3
@waits_by_tid[19748]: 3
@waits_by_tid[19771]: 3
@waits_by_tid[19603]: 3
@waits_by_tid[23088]: 4
@waits_by_tid[19749]: 4
@waits_by_tid[2124]: 4
@waits_by_tid[19656]: 4
@waits_by_tid[22655]: 5
@waits_by_tid[19496]: 7
@waits_by_tid[19523]: 7
@waits_by_tid[1816]: 8
@waits_by_tid[19499]: 8
@waits_by_tid[1817]: 8
@waits_by_tid[19498]: 8
@waits_by_tid[1815]: 9
@waits_by_tid[2126]: 9
@waits_by_tid[21484]: 10
@waits_by_tid[19578]: 10
@waits_by_tid[19604]: 10
@waits_by_tid[19497]: 10
@waits_by_tid[19874]: 10
@waits_by_tid[19775]: 11
@waits_by_tid[1814]: 11
@waits_by_tid[19774]: 12
@waits_by_tid[19777]: 12
@waits_by_tid[19966]: 12
@waits_by_tid[19776]: 12
@waits_by_tid[20545]: 13
@waits_by_tid[1999]: 15
@waits_by_tid[2040]: 18
@waits_by_tid[1793]: 19
@waits_by_tid[19575]: 20
@waits_by_tid[847]: 27
@waits_by_tid[19514]: 29
@waits_by_tid[1799]: 33
@waits_by_tid[19573]: 34
@waits_by_tid[19802]: 38
@waits_by_tid[19807]: 55
@waits_by_tid[1989]: 60
@waits_by_tid[1930]: 90
@waits_by_tid[19570]: 131
@waits_by_tid[19773]: 167
@waits_by_tid[19741]: 182
@waits_by_tid[19778]: 243
@waits_by_tid[19742]: 275
@waits_by_tid[19797]: 26551


