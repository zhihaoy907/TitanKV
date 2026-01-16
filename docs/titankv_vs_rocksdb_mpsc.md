# MPSC架构下 TitanKV 与 RocksDB 性能对比

本 README 总结了 MPSC架构下 TitanKV 与 RocksDB的基准测试结果，对比了两者的操作性能和吞吐量。

# 1. 测试对比
1.1 TitanKV (DirectIO + TPC)
<img width="1533" height="412" alt="image" src="https://github.com/user-attachments/assets/f5ac9f40-3937-46b8-848b-c53a25ed8e37" />


1.2 RocksDB (默认 WAL + MemTable)
<img width="1501" height="386" alt="image" src="https://github.com/user-attachments/assets/e83ddd1b-04a1-4d92-95ad-92aa96bee58d" />


# 分析：

**在 MPSC 架构下，TitanKV 的 IOPS 和吞吐量明显高于 RocksDB。**

**TitanKV 采用 DirectIO 和 TPC 的组合，可以减少内核缓存干扰，提高磁盘写入效率。**


# 2. eBPF 锁竞争与系统调用分析
为了深入理解性能差异背后的系统级原因，我们使用 eBPF 工具对两个数据库在测试期间的内核行为进行了追踪，重点关注锁竞争和线程调度。

测试方法

工具：tools/analyze_lock.bt

追踪对象：数据库进程在基准测试期间的全部 FUTEX_WAIT 事件（用户态锁竞争的主要指标）和上下文切换。

目标：量化评估两者在高并发写入压力下的同步效率和线程阻塞情况。

**2.1 锁竞争总览**
下表汇总了核心的锁与调度指标，直接反映程序并发效率：

<img width="1561" height="964" alt="image" src="https://github.com/user-attachments/assets/0a392f43-b187-46b3-9748-a85c7c463fe0" />


详细输出见下：

**TitanKV输出**

(base) zhihaoy@virtual-machine:~/workspace/github/TitanKV/build$ sudo ../tools/analyze_lock.bt 30027
[sudo] password for zhihaoy: 
Attaching 4 probes...
Tracing FUTEX_WAIT contention for PID 30042...
Hit Ctrl-C to end.

========== Futex Contention Summary (PID 30042) ==========

---- Total Futex Wait Count ----
@total_waits: 1198


---- Futex Wait Latency Histogram (ns) ----
@wait_ns: 
[512, 1K)              5 |@                                                   |
[1K, 2K)             172 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@           |
[2K, 4K)             213 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@|
[4K, 8K)              46 |@@@@@@@@@@@                                         |
[8K, 16K)            159 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@              |
[16K, 32K)            59 |@@@@@@@@@@@@@@                                      |
[32K, 64K)            30 |@@@@@@@                                             |
[64K, 128K)           97 |@@@@@@@@@@@@@@@@@@@@@@@                             |
[128K, 256K)         110 |@@@@@@@@@@@@@@@@@@@@@@@@@@                          |
[256K, 512K)          65 |@@@@@@@@@@@@@@@                                     |
[512K, 1M)            27 |@@@@@@                                              |
[1M, 2M)              13 |@@@                                                 |
[2M, 4M)              91 |@@@@@@@@@@@@@@@@@@@@@@                              |
[4M, 8M)               2 |                                                    |
[8M, 16M)              6 |@                                                   |
[16M, 32M)             0 |                                                    |
[32M, 64M)             0 |                                                    |
[64M, 128M)            2 |                                                    |
[128M, 256M)           8 |@                                                   |
[256M, 512M)          26 |@@@@@@                                              |
[512M, 1G)            28 |@@@@@@                                              |
[1G, 2G)              11 |@@                                                  |
[2G, 4G)              30 |@@@@@@@                                             |


---- Futex Wait Count by Address ----
@wait_count[100230244264704]: 1
@wait_count[127437383981840]: 1
@wait_count[136268038062912]: 1
@wait_count[110850608726976]: 1
@wait_count[100230244423164]: 1
@wait_count[100828501087148]: 1
@wait_count[131378041579792]: 1
@wait_count[100658825090464]: 1
@wait_count[136268038105232]: 1
@wait_count[131377949309200]: 1
@wait_count[131378049972496]: 1
@wait_count[106376837360784]: 1
@wait_count[100230249621928]: 1
@wait_count[135546706163780]: 1
@wait_count[135546725689412]: 1
@wait_count[106376837293056]: 1
@wait_count[131378033187088]: 1
@wait_count[130404105253160]: 2
@wait_count[94302283350976]: 2
@wait_count[110850608726896]: 2
@wait_count[135546694740036]: 2
@wait_count[135546699655236]: 2
@wait_count[67259188117512]: 2
@wait_count[136268375764096]: 2
@wait_count[37881846981296]: 2
@wait_count[100658824005776]: 2
@wait_count[107200748244864]: 3
@wait_count[100658824055672]: 3
@wait_count[94302283350956]: 3
@wait_count[140728945709496]: 3
@wait_count[138816928039688]: 3
@wait_count[130404101237888]: 4
@wait_count[136268038021536]: 4
@wait_count[140726541700776]: 4
@wait_count[138816944825096]: 4
@wait_count[140731460000600]: 4
@wait_count[100828501087384]: 5
@wait_count[136268399075600]: 5
@wait_count[100230244434792]: 5
@wait_count[100230249621952]: 5
@wait_count[100230249621956]: 5
@wait_count[136268105121840]: 5
@wait_count[130404105253136]: 6
@wait_count[100828501087172]: 6
@wait_count[100828501087168]: 8
@wait_count[136268360585472]: 8
@wait_count[100828501087088]: 8
@wait_count[100658824054904]: 9
@wait_count[140737130534424]: 9
@wait_count[94302283350896]: 9
@wait_count[94302283350980]: 9
@wait_count[110917073343152]: 12
@wait_count[135546731559252]: 14
@wait_count[140723330965768]: 15
@wait_count[18605799326600]: 15
@wait_count[136268399075624]: 15
@wait_count[100658823945568]: 17
@wait_count[48704930038664]: 17
@wait_count[106376837255448]: 18
@wait_count[106376837248320]: 19
@wait_count[99650149511448]: 24
@wait_count[67259188196232]: 25
@wait_count[99650149505568]: 25
@wait_count[135546724628836]: 26
@wait_count[140728945709576]: 28
@wait_count[100658825087264]: 31
@wait_count[131377747984432]: 359
@wait_count[131377613766704]: 368


---- Futex Total Wait Time by Address (ns) ----
@wait_time_ns[100658825090464]: 1623
@wait_time_ns[136268375764096]: 2845
@wait_time_ns[100658824005776]: 3044
@wait_time_ns[136268038021536]: 8355
@wait_time_ns[100230244264704]: 8444
@wait_time_ns[130404101237888]: 8675
@wait_time_ns[136268399075600]: 11950
@wait_time_ns[106376837360784]: 16068
@wait_time_ns[106376837293056]: 17320
@wait_time_ns[67259188117512]: 23631
@wait_time_ns[140728945709496]: 29433
@wait_time_ns[127437383981840]: 64953
@wait_time_ns[110850608726896]: 83046
@wait_time_ns[100230249621928]: 120081
@wait_time_ns[100828501087148]: 140045
@wait_time_ns[106376837255448]: 185738
@wait_time_ns[136268038062912]: 223972
@wait_time_ns[100828501087384]: 250189
@wait_time_ns[136268038105232]: 268220
@wait_time_ns[106376837248320]: 351396
@wait_time_ns[100658823945568]: 367935
@wait_time_ns[131378041579792]: 376769
@wait_time_ns[136268399075624]: 414577
@wait_time_ns[107200748244864]: 448105
@wait_time_ns[94302283350956]: 459894
@wait_time_ns[130404105253160]: 473930
@wait_time_ns[100658824055672]: 497210
@wait_time_ns[100230244423164]: 510410
@wait_time_ns[37881846981296]: 672928
@wait_time_ns[100658825087264]: 752267
@wait_time_ns[100230244434792]: 966310
@wait_time_ns[99650149511448]: 1363849
@wait_time_ns[130404105253136]: 1396012
@wait_time_ns[131378049972496]: 1510193
@wait_time_ns[100828501087088]: 2092735
@wait_time_ns[136268105121840]: 2291750
@wait_time_ns[94302283350896]: 2381842
@wait_time_ns[100658824054904]: 2597284
@wait_time_ns[67259188196232]: 2834874
@wait_time_ns[99650149505568]: 3847584
@wait_time_ns[18605799326600]: 4011318
@wait_time_ns[100230249621956]: 4276936
@wait_time_ns[136268360585472]: 4687645
@wait_time_ns[48704930038664]: 6485800
@wait_time_ns[131377949309200]: 15026819
@wait_time_ns[100828501087172]: 20612510
@wait_time_ns[100230249621952]: 47289705
@wait_time_ns[131377747984432]: 131474286
@wait_time_ns[131377613766704]: 154048973
@wait_time_ns[131378033187088]: 547713590
@wait_time_ns[138816944825096]: 1821470063
@wait_time_ns[138816928039688]: 1825993349
@wait_time_ns[135546706163780]: 5001073606
@wait_time_ns[135546725689412]: 5001158355
@wait_time_ns[94302283350976]: 8304393287
@wait_time_ns[140737130534424]: 9119319550
@wait_time_ns[135546694740036]: 10002269719
@wait_time_ns[135546699655236]: 10002463086
@wait_time_ns[110850608726976]: 10009738730
@wait_time_ns[100828501087168]: 10011166481
@wait_time_ns[135546731559252]: 10152318607
@wait_time_ns[140723330965768]: 10779448595
@wait_time_ns[140731460000600]: 12000650607
@wait_time_ns[140726541700776]: 12000722602
@wait_time_ns[110917073343152]: 12003503794
@wait_time_ns[140728945709576]: 12748320773
@wait_time_ns[135546724628836]: 13007477612
@wait_time_ns[94302283350980]: 32629596722


---- Futex Wait Count by Thread ----
@waits_by_tid[2040]: 1
@waits_by_tid[24615]: 1
@waits_by_tid[24599]: 1
@waits_by_tid[24624]: 2
@waits_by_tid[24620]: 2
@waits_by_tid[2126]: 2
@waits_by_tid[1815]: 3
@waits_by_tid[23702]: 3
@waits_by_tid[1817]: 3
@waits_by_tid[23896]: 3
@waits_by_tid[23699]: 4
@waits_by_tid[1816]: 4
@waits_by_tid[30027]: 4
@waits_by_tid[23611]: 4
@waits_by_tid[23504]: 4
@waits_by_tid[23873]: 4
@waits_by_tid[23842]: 4
@waits_by_tid[23872]: 5
@waits_by_tid[23474]: 5
@waits_by_tid[23505]: 5
@waits_by_tid[23874]: 5
@waits_by_tid[23875]: 6
@waits_by_tid[1814]: 7
@waits_by_tid[23507]: 7
@waits_by_tid[23506]: 7
@waits_by_tid[847]: 12
@waits_by_tid[24595]: 14
@waits_by_tid[23695]: 14
@waits_by_tid[23899]: 15
@waits_by_tid[23871]: 15
@waits_by_tid[23609]: 17
@waits_by_tid[1793]: 18
@waits_by_tid[1799]: 24
@waits_by_tid[1989]: 24
@waits_by_tid[23876]: 25
@waits_by_tid[24600]: 26
@waits_by_tid[23843]: 36
@waits_by_tid[2124]: 37
@waits_by_tid[1999]: 49
@waits_by_tid[1930]: 51
@waits_by_tid[30044]: 92
@waits_by_tid[30045]: 107
@waits_by_tid[30028]: 229
@waits_by_tid[30029]: 299



@total_waits: 1198

@wait_addr[23888]: 18605799032488
@wait_addr[23875]: 94302283350976
@wait_addr[23872]: 94302283350980
@wait_addr[23873]: 94302283350980
@wait_addr[23874]: 94302283350980
@wait_addr[1815]: 100230249621952
@wait_addr[1817]: 100230249621952
@wait_addr[1814]: 100230249621952
@wait_addr[1816]: 100230249621956
@wait_addr[23505]: 100828501087168
@wait_addr[23504]: 100828501087168
@wait_addr[23507]: 100828501087172
@wait_addr[23506]: 100828501087172
@wait_addr[23898]: 110850608726976
@wait_addr[23895]: 110850608726980
@wait_addr[23897]: 110850608726980
@wait_addr[23896]: 110850608726980
@wait_addr[847]: 110917073343152
@wait_addr[23687]: 126752329282152
@wait_addr[24624]: 135546694740036
@wait_addr[24620]: 135546699655236
@wait_addr[24615]: 135546706163780
@wait_addr[24600]: 135546724628836
@wait_addr[24599]: 135546725689412
@wait_addr[24595]: 135546731559252
@wait_addr[23702]: 138816928039736
@wait_addr[23699]: 138816944825144
@wait_addr[23871]: 140723330965768
@wait_addr[23842]: 140726541700776
@wait_addr[23843]: 140728945709576
@wait_addr[23611]: 140731460000600
@wait_addr[23695]: 140737130534424

@wait_count[100230244264704]: 1
@wait_count[127437383981840]: 1
@wait_count[136268038062912]: 1
@wait_count[110850608726976]: 1
@wait_count[100230244423164]: 1
@wait_count[100828501087148]: 1
@wait_count[131378041579792]: 1
@wait_count[100658825090464]: 1
@wait_count[136268038105232]: 1
@wait_count[131377949309200]: 1
@wait_count[131378049972496]: 1
@wait_count[106376837360784]: 1
@wait_count[100230249621928]: 1
@wait_count[135546706163780]: 1
@wait_count[135546725689412]: 1
@wait_count[106376837293056]: 1
@wait_count[131378033187088]: 1
@wait_count[130404105253160]: 2
@wait_count[94302283350976]: 2
@wait_count[110850608726896]: 2
@wait_count[135546694740036]: 2
@wait_count[135546699655236]: 2
@wait_count[67259188117512]: 2
@wait_count[136268375764096]: 2
@wait_count[37881846981296]: 2
@wait_count[100658824005776]: 2
@wait_count[107200748244864]: 3
@wait_count[100658824055672]: 3
@wait_count[94302283350956]: 3
@wait_count[140728945709496]: 3
@wait_count[138816928039688]: 3
@wait_count[130404101237888]: 4
@wait_count[136268038021536]: 4
@wait_count[140726541700776]: 4
@wait_count[138816944825096]: 4
@wait_count[140731460000600]: 4
@wait_count[100828501087384]: 5
@wait_count[136268399075600]: 5
@wait_count[100230244434792]: 5
@wait_count[100230249621952]: 5
@wait_count[100230249621956]: 5
@wait_count[136268105121840]: 5
@wait_count[130404105253136]: 6
@wait_count[100828501087172]: 6
@wait_count[100828501087168]: 8
@wait_count[136268360585472]: 8
@wait_count[100828501087088]: 8
@wait_count[100658824054904]: 9
@wait_count[140737130534424]: 9
@wait_count[94302283350896]: 9
@wait_count[94302283350980]: 9
@wait_count[110917073343152]: 12
@wait_count[135546731559252]: 14
@wait_count[140723330965768]: 15
@wait_count[18605799326600]: 15
@wait_count[136268399075624]: 15
@wait_count[100658823945568]: 17
@wait_count[48704930038664]: 17
@wait_count[106376837255448]: 18
@wait_count[106376837248320]: 19
@wait_count[99650149511448]: 24
@wait_count[67259188196232]: 25
@wait_count[99650149505568]: 25
@wait_count[135546724628836]: 26
@wait_count[140728945709576]: 28
@wait_count[100658825087264]: 31
@wait_count[131377747984432]: 359
@wait_count[131377613766704]: 368

@wait_ns: 
[512, 1K)              5 |@                                                   |
[1K, 2K)             172 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@           |
[2K, 4K)             213 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@|
[4K, 8K)              46 |@@@@@@@@@@@                                         |
[8K, 16K)            159 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@              |
[16K, 32K)            59 |@@@@@@@@@@@@@@                                      |
[32K, 64K)            30 |@@@@@@@                                             |
[64K, 128K)           97 |@@@@@@@@@@@@@@@@@@@@@@@                             |
[128K, 256K)         110 |@@@@@@@@@@@@@@@@@@@@@@@@@@                          |
[256K, 512K)          65 |@@@@@@@@@@@@@@@                                     |
[512K, 1M)            27 |@@@@@@                                              |
[1M, 2M)              13 |@@@                                                 |
[2M, 4M)              91 |@@@@@@@@@@@@@@@@@@@@@@                              |
[4M, 8M)               2 |                                                    |
[8M, 16M)              6 |@                                                   |
[16M, 32M)             0 |                                                    |
[32M, 64M)             0 |                                                    |
[64M, 128M)            2 |                                                    |
[128M, 256M)           8 |@                                                   |
[256M, 512M)          26 |@@@@@@                                              |
[512M, 1G)            28 |@@@@@@                                              |
[1G, 2G)              11 |@@                                                  |
[2G, 4G)              30 |@@@@@@@                                             |

@wait_start[23898]: 78040130264824
@wait_start[23687]: 78040514439124
@wait_start[23897]: 78042632690023
@wait_start[1814]: 78043956386137
@wait_start[1817]: 78043956790417
@wait_start[1815]: 78043957426520
@wait_start[1816]: 78043957570322
@wait_start[23895]: 78045134060468
@wait_start[23888]: 78045137703834
@wait_start[23504]: 78045171865443
@wait_start[23505]: 78045178008313
@wait_start[23507]: 78045945599455
@wait_start[23506]: 78045945671911
@wait_start[24615]: 78046068469236
@wait_start[24599]: 78046068481758
@wait_start[23874]: 78046555031592
@wait_start[23872]: 78046555041289
@wait_start[23873]: 78046555074556
@wait_start[23695]: 78046810012474
@wait_start[23702]: 78047000338339
@wait_start[23699]: 78047000410366
@wait_start[23875]: 78047410393159
@wait_start[23896]: 78047637482721
@wait_start[23871]: 78048411802759
@wait_start[24620]: 78048569242442
@wait_start[24624]: 78048569253082
@wait_start[24595]: 78048719460007
@wait_start[847]: 78049149739528
@wait_start[23611]: 78049156388126
@wait_start[23843]: 78049286159546
@wait_start[24600]: 78049711368009
@wait_start[23842]: 78050028421885

@wait_time_ns[100658825090464]: 1623
@wait_time_ns[136268375764096]: 2845
@wait_time_ns[100658824005776]: 3044
@wait_time_ns[136268038021536]: 8355
@wait_time_ns[100230244264704]: 8444
@wait_time_ns[130404101237888]: 8675
@wait_time_ns[136268399075600]: 11950
@wait_time_ns[106376837360784]: 16068
@wait_time_ns[106376837293056]: 17320
@wait_time_ns[67259188117512]: 23631
@wait_time_ns[140728945709496]: 29433
@wait_time_ns[127437383981840]: 64953
@wait_time_ns[110850608726896]: 83046
@wait_time_ns[100230249621928]: 120081
@wait_time_ns[100828501087148]: 140045
@wait_time_ns[106376837255448]: 185738
@wait_time_ns[136268038062912]: 223972
@wait_time_ns[100828501087384]: 250189
@wait_time_ns[136268038105232]: 268220
@wait_time_ns[106376837248320]: 351396
@wait_time_ns[100658823945568]: 367935
@wait_time_ns[131378041579792]: 376769
@wait_time_ns[136268399075624]: 414577
@wait_time_ns[107200748244864]: 448105
@wait_time_ns[94302283350956]: 459894
@wait_time_ns[130404105253160]: 473930
@wait_time_ns[100658824055672]: 497210
@wait_time_ns[100230244423164]: 510410
@wait_time_ns[37881846981296]: 672928
@wait_time_ns[100658825087264]: 752267
@wait_time_ns[100230244434792]: 966310
@wait_time_ns[99650149511448]: 1363849
@wait_time_ns[130404105253136]: 1396012
@wait_time_ns[131378049972496]: 1510193
@wait_time_ns[100828501087088]: 2092735
@wait_time_ns[136268105121840]: 2291750
@wait_time_ns[94302283350896]: 2381842
@wait_time_ns[100658824054904]: 2597284
@wait_time_ns[67259188196232]: 2834874
@wait_time_ns[99650149505568]: 3847584
@wait_time_ns[18605799326600]: 4011318
@wait_time_ns[100230249621956]: 4276936
@wait_time_ns[136268360585472]: 4687645
@wait_time_ns[48704930038664]: 6485800
@wait_time_ns[131377949309200]: 15026819
@wait_time_ns[100828501087172]: 20612510
@wait_time_ns[100230249621952]: 47289705
@wait_time_ns[131377747984432]: 131474286
@wait_time_ns[131377613766704]: 154048973
@wait_time_ns[131378033187088]: 547713590
@wait_time_ns[138816944825096]: 1821470063
@wait_time_ns[138816928039688]: 1825993349
@wait_time_ns[135546706163780]: 5001073606
@wait_time_ns[135546725689412]: 5001158355
@wait_time_ns[94302283350976]: 8304393287
@wait_time_ns[140737130534424]: 9119319550
@wait_time_ns[135546694740036]: 10002269719
@wait_time_ns[135546699655236]: 10002463086
@wait_time_ns[110850608726976]: 10009738730
@wait_time_ns[100828501087168]: 10011166481
@wait_time_ns[135546731559252]: 10152318607
@wait_time_ns[140723330965768]: 10779448595
@wait_time_ns[140731460000600]: 12000650607
@wait_time_ns[140726541700776]: 12000722602
@wait_time_ns[110917073343152]: 12003503794
@wait_time_ns[140728945709576]: 12748320773
@wait_time_ns[135546724628836]: 13007477612
@wait_time_ns[94302283350980]: 32629596722

@waits_by_tid[2040]: 1
@waits_by_tid[24615]: 1
@waits_by_tid[24599]: 1
@waits_by_tid[24624]: 2
@waits_by_tid[24620]: 2
@waits_by_tid[2126]: 2
@waits_by_tid[1815]: 3
@waits_by_tid[23702]: 3
@waits_by_tid[1817]: 3
@waits_by_tid[23896]: 3
@waits_by_tid[23699]: 4
@waits_by_tid[1816]: 4
@waits_by_tid[30027]: 4
@waits_by_tid[23611]: 4
@waits_by_tid[23504]: 4
@waits_by_tid[23873]: 4
@waits_by_tid[23842]: 4
@waits_by_tid[23872]: 5
@waits_by_tid[23474]: 5
@waits_by_tid[23505]: 5
@waits_by_tid[23874]: 5
@waits_by_tid[23875]: 6
@waits_by_tid[1814]: 7
@waits_by_tid[23507]: 7
@waits_by_tid[23506]: 7
@waits_by_tid[847]: 12
@waits_by_tid[24595]: 14
@waits_by_tid[23695]: 14
@waits_by_tid[23899]: 15
@waits_by_tid[23871]: 15
@waits_by_tid[23609]: 17
@waits_by_tid[1793]: 18
@waits_by_tid[1799]: 24
@waits_by_tid[1989]: 24
@waits_by_tid[23876]: 25
@waits_by_tid[24600]: 26
@waits_by_tid[23843]: 36
@waits_by_tid[2124]: 37
@waits_by_tid[1999]: 49
@waits_by_tid[1930]: 51
@waits_by_tid[30044]: 92
@waits_by_tid[30045]: 107
@waits_by_tid[30028]: 229
@waits_by_tid[30029]: 299

**RocksDB输出：**
(base) zhihaoy@virtual-machine:~/workspace/github/TitanKV/build$ sudo ../tools/analyze_lock.bt 30088
Attaching 4 probes...
Tracing FUTEX_WAIT contention for PID 30091...
Hit Ctrl-C to end.

^C
========== Futex Contention Summary (PID 30091) ==========

---- Total Futex Wait Count ----
@total_waits: 32868


---- Futex Wait Latency Histogram (ns) ----
@wait_ns: 
[512, 1K)              1 |                                                    |
[1K, 2K)              44 |                                                    |
[2K, 4K)              95 |                                                    |
[4K, 8K)              12 |                                                    |
[8K, 16K)            181 |                                                    |
[16K, 32K)           109 |                                                    |
[32K, 64K)           337 |                                                    |
[64K, 128K)         2303 |@@@@@@                                              |
[128K, 256K)        1230 |@@@                                                 |
[256K, 512K)         303 |                                                    |
[512K, 1M)         17751 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@|
[1M, 2M)            9696 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@                        |
[2M, 4M)             433 |@                                                   |
[4M, 8M)              26 |                                                    |
[8M, 16M)              7 |                                                    |
[16M, 32M)             0 |                                                    |
[32M, 64M)             0 |                                                    |
[64M, 128M)            1 |                                                    |
[128M, 256M)          23 |                                                    |
[256M, 512M)          91 |                                                    |
[512M, 1G)            68 |                                                    |
[1G, 2G)              19 |                                                    |
[2G, 4G)             139 |                                                    |


---- Futex Wait Count by Address ----
@wait_count[100230244423160]: 1
@wait_count[140115182363400]: 1
@wait_count[100658824055160]: 1
@wait_count[132361815570704]: 1
@wait_count[98105927893016]: 1
@wait_count[100658823976176]: 1
@wait_count[18605798588424]: 1
@wait_count[100230245088192]: 1
@wait_count[135083659432760]: 1
@wait_count[136268398682040]: 1
@wait_count[56985626345480]: 1
@wait_count[103356424437976]: 1
@wait_count[98105927924460]: 1
@wait_count[100230244264704]: 1
@wait_count[100828501087384]: 1
@wait_count[110850608726896]: 1
@wait_count[132361823963408]: 1
@wait_count[100230245059600]: 1
@wait_count[140119427437320]: 1
@wait_count[100230249621932]: 1
@wait_count[127437676601616]: 1
@wait_count[132361228386352]: 1
@wait_count[132361345784080]: 1
@wait_count[132361354176784]: 1
@wait_count[132361849141520]: 1
@wait_count[100230249621928]: 1
@wait_count[140726541700696]: 2
@wait_count[130404105253136]: 2
@wait_count[98105927889124]: 2
@wait_count[98105927889120]: 2
@wait_count[136268105121840]: 2
@wait_count[100658824055672]: 2
@wait_count[824638025568]: 2
@wait_count[100230244423164]: 2
@wait_count[96887506556344]: 2
@wait_count[100658825090464]: 2
@wait_count[130404105253160]: 2
@wait_count[98105927888288]: 2
@wait_count[98105927924456]: 2
@wait_count[67259188117512]: 3
@wait_count[130404101237888]: 3
@wait_count[98105927888368]: 3
@wait_count[100230249621956]: 3
@wait_count[136268038105232]: 3
@wait_count[106376837255448]: 3
@wait_count[100828501087088]: 4
@wait_count[138816944825096]: 4
@wait_count[136268399075600]: 4
@wait_count[94302283351192]: 4
@wait_count[100230244458784]: 4
@wait_count[136268375764096]: 4
@wait_count[56985626424200]: 4
@wait_count[135083659432712]: 4
@wait_count[100658823945568]: 4
@wait_count[106376837248320]: 4
@wait_count[136268038062912]: 5
@wait_count[127437249708080]: 5
@wait_count[110850608726976]: 5
@wait_count[127437676601640]: 5
@wait_count[136268038021536]: 5
@wait_count[100828501087172]: 6
@wait_count[140723330965688]: 6
@wait_count[110850608726980]: 6
@wait_count[100658824054904]: 6
@wait_count[99650149505568]: 6
@wait_count[100828501087168]: 6
@wait_count[140737130534424]: 6
@wait_count[135546725689412]: 7
@wait_count[99650149572688]: 7
@wait_count[100230244434792]: 7
@wait_count[135546694740036]: 7
@wait_count[135546706163780]: 7
@wait_count[100230249621952]: 7
@wait_count[135546699655236]: 7
@wait_count[94302283350956]: 7
@wait_count[136268399075624]: 7
@wait_count[140731460000600]: 8
@wait_count[140728945709496]: 9
@wait_count[94302283350976]: 10
@wait_count[136268360585472]: 10
@wait_count[140726541700776]: 13
@wait_count[99650149511448]: 15
@wait_count[100658825087264]: 17
@wait_count[48704930038664]: 24
@wait_count[94302283350896]: 28
@wait_count[94302283350980]: 29
@wait_count[110917073343152]: 37
@wait_count[135546731559252]: 47
@wait_count[18605799326600]: 48
@wait_count[140723330965768]: 50
@wait_count[140728945709576]: 69
@wait_count[135546724628836]: 77
@wait_count[67259188196232]: 79
@wait_count[132361354171568]: 91
@wait_count[132361345778864]: 99
@wait_count[132361345778956]: 1596
@wait_count[132361354171660]: 1874
@wait_count[132361345778952]: 14003
@wait_count[132361354171656]: 14396


---- Futex Total Wait Time by Address (ns) ----
@wait_time_ns[100658823976176]: 1442
@wait_time_ns[127437676601616]: 2385
@wait_time_ns[100230244264704]: 2935
@wait_time_ns[136268105121840]: 5190
@wait_time_ns[130404105253136]: 5379
@wait_time_ns[100658825090464]: 5671
@wait_time_ns[130404105253160]: 5770
@wait_time_ns[130404101237888]: 6102
@wait_time_ns[103356424437976]: 8064
@wait_time_ns[110850608726896]: 8375
@wait_time_ns[136268399075600]: 9196
@wait_time_ns[136268398682040]: 10238
@wait_time_ns[56985626345480]: 10558
@wait_time_ns[18605798588424]: 11761
@wait_time_ns[136268038021536]: 12431
@wait_time_ns[100658823945568]: 13232
@wait_time_ns[99650149572688]: 17330
@wait_time_ns[67259188117512]: 33488
@wait_time_ns[100658824055160]: 41693
@wait_time_ns[106376837248320]: 51119
@wait_time_ns[132361823963408]: 55567
@wait_time_ns[100828501087384]: 69772
@wait_time_ns[136268038105232]: 74741
@wait_time_ns[100230244423160]: 86441
@wait_time_ns[136268399075624]: 95409
@wait_time_ns[106376837255448]: 111575
@wait_time_ns[100828501087088]: 111605
@wait_time_ns[140728945709496]: 113518
@wait_time_ns[100230249621932]: 123737
@wait_time_ns[140726541700696]: 153949
@wait_time_ns[100230244458784]: 161182
@wait_time_ns[132361849141520]: 168457
@wait_time_ns[100230249621928]: 177721
@wait_time_ns[94302283351192]: 208995
@wait_time_ns[132361815570704]: 236324
@wait_time_ns[127437676601640]: 274300
@wait_time_ns[140723330965688]: 278457
@wait_time_ns[96887506556344]: 328607
@wait_time_ns[56985626424200]: 473257
@wait_time_ns[100230244423164]: 520742
@wait_time_ns[100658825087264]: 934706
@wait_time_ns[100658824054904]: 1019003
@wait_time_ns[136268375764096]: 1103801
@wait_time_ns[98105927888288]: 1384813
@wait_time_ns[100658824055672]: 1473538
@wait_time_ns[94302283350956]: 1569566
@wait_time_ns[824638025568]: 1667948
@wait_time_ns[136268360585472]: 1739853
@wait_time_ns[100230244434792]: 1789271
@wait_time_ns[99650149511448]: 2304927
@wait_time_ns[98105927924456]: 2673890
@wait_time_ns[136268038062912]: 2706214
@wait_time_ns[48704930038664]: 4511453
@wait_time_ns[98105927924460]: 4579295
@wait_time_ns[132361345778864]: 4621655
@wait_time_ns[132361354171568]: 5541174
@wait_time_ns[132361228386352]: 5832094
@wait_time_ns[94302283350896]: 6589711
@wait_time_ns[127437249708080]: 6981667
@wait_time_ns[67259188196232]: 9620617
@wait_time_ns[99650149505568]: 11467095
@wait_time_ns[18605799326600]: 15248216
@wait_time_ns[100230249621956]: 19383551
@wait_time_ns[100230249621952]: 54462282
@wait_time_ns[132361345778956]: 224001338
@wait_time_ns[132361354171660]: 265250068
@wait_time_ns[132361354176784]: 272899549
@wait_time_ns[100230245088192]: 500252407
@wait_time_ns[140119427437320]: 1287638359
@wait_time_ns[140115182363400]: 1675939045
@wait_time_ns[138816944825096]: 1676249578
@wait_time_ns[135083659432712]: 2782156011
@wait_time_ns[98105927889124]: 12008429520
@wait_time_ns[132361345778952]: 14085608196
@wait_time_ns[100230245059600]: 15000075462
@wait_time_ns[132361354171656]: 15427063768
@wait_time_ns[98105927889120]: 18210286661
@wait_time_ns[135083659432760]: 18890003377
@wait_time_ns[94302283350976]: 29286917257
@wait_time_ns[132361345784080]: 30376224901
@wait_time_ns[98105927893016]: 30663365720
@wait_time_ns[140731460000600]: 32002415286
@wait_time_ns[140737130534424]: 34260360368
@wait_time_ns[135546706163780]: 35005487932
@wait_time_ns[135546725689412]: 35006101626
@wait_time_ns[135546699655236]: 35006133525
@wait_time_ns[135546694740036]: 35006693025
@wait_time_ns[140723330965768]: 35989322725
@wait_time_ns[140726541700776]: 36001004572
@wait_time_ns[110917073343152]: 37011519215
@wait_time_ns[135546731559252]: 37655533201
@wait_time_ns[140728945709576]: 37994580347
@wait_time_ns[135546724628836]: 38524658513
@wait_time_ns[110850608726976]: 50049154980
@wait_time_ns[110850608726980]: 60059265150
@wait_time_ns[100828501087168]: 60142915687
@wait_time_ns[100828501087172]: 60146450283
@wait_time_ns[98105927888368]: 91991561769
@wait_time_ns[94302283350980]: 111808252909


---- Futex Wait Count by Thread ----
@waits_by_tid[2607]: 1
@waits_by_tid[23487]: 1
@waits_by_tid[30097]: 1
@waits_by_tid[23665]: 1
@waits_by_tid[30095]: 1
@waits_by_tid[30116]: 2
@waits_by_tid[23523]: 2
@waits_by_tid[30094]: 2
@waits_by_tid[23895]: 2
@waits_by_tid[30093]: 2
@waits_by_tid[23526]: 2
@waits_by_tid[1817]: 3
@waits_by_tid[23505]: 3
@waits_by_tid[23897]: 3
@waits_by_tid[23896]: 3
@waits_by_tid[23507]: 3
@waits_by_tid[23699]: 4
@waits_by_tid[23898]: 4
@waits_by_tid[1814]: 4
@waits_by_tid[23506]: 4
@waits_by_tid[23881]: 4
@waits_by_tid[1816]: 5
@waits_by_tid[30088]: 5
@waits_by_tid[23636]: 5
@waits_by_tid[23695]: 6
@waits_by_tid[24624]: 7
@waits_by_tid[30096]: 7
@waits_by_tid[24615]: 7
@waits_by_tid[2124]: 7
@waits_by_tid[23504]: 7
@waits_by_tid[24599]: 7
@waits_by_tid[24620]: 7
@waits_by_tid[23611]: 8
@waits_by_tid[1815]: 8
@waits_by_tid[1989]: 12
@waits_by_tid[23872]: 16
@waits_by_tid[23873]: 16
@waits_by_tid[23842]: 16
@waits_by_tid[2040]: 17
@waits_by_tid[23875]: 18
@waits_by_tid[23874]: 20
@waits_by_tid[1793]: 21
@waits_by_tid[1999]: 22
@waits_by_tid[23609]: 24
@waits_by_tid[1799]: 27
@waits_by_tid[1930]: 28
@waits_by_tid[847]: 37
@waits_by_tid[24595]: 47
@waits_by_tid[23899]: 48
@waits_by_tid[23871]: 57
@waits_by_tid[24600]: 77
@waits_by_tid[23876]: 79
@waits_by_tid[23843]: 89
@waits_by_tid[30113]: 15696
@waits_by_tid[30114]: 16364



@total_waits: 32868

@wait_addr[23526]: 824638025568
@wait_addr[23522]: 824638964064
@wait_addr[23523]: 824638967648
@wait_addr[23875]: 94302283350976
@wait_addr[23874]: 94302283350980
@wait_addr[23873]: 94302283350980
@wait_addr[23872]: 94302283350980
@wait_addr[23515]: 96887506556320
@wait_addr[1814]: 100230249621952
@wait_addr[1815]: 100230249621956
@wait_addr[1816]: 100230249621956
@wait_addr[1817]: 100230249621956
@wait_addr[23504]: 100828501087168
@wait_addr[23506]: 100828501087168
@wait_addr[23505]: 100828501087172
@wait_addr[23507]: 100828501087172
@wait_addr[23895]: 110850608726976
@wait_addr[23896]: 110850608726976
@wait_addr[23898]: 110850608726976
@wait_addr[23897]: 110850608726980
@wait_addr[847]: 110917073343152
@wait_addr[23687]: 126752329282152
@wait_addr[23636]: 135083659432760
@wait_addr[24624]: 135546694740036
@wait_addr[24620]: 135546699655236
@wait_addr[24615]: 135546706163780
@wait_addr[24600]: 135546724628836
@wait_addr[24599]: 135546725689412
@wait_addr[24595]: 135546731559252
@wait_addr[23699]: 138816944825144
@wait_addr[23665]: 140115182363448
@wait_addr[23487]: 140119427437368
@wait_addr[23871]: 140723330965768
@wait_addr[23842]: 140726541700776
@wait_addr[23843]: 140728945709576
@wait_addr[23611]: 140731460000600
@wait_addr[23695]: 140737130534424

@wait_count[100230244423160]: 1
@wait_count[140115182363400]: 1
@wait_count[100658824055160]: 1
@wait_count[132361815570704]: 1
@wait_count[98105927893016]: 1
@wait_count[100658823976176]: 1
@wait_count[18605798588424]: 1
@wait_count[100230245088192]: 1
@wait_count[135083659432760]: 1
@wait_count[136268398682040]: 1
@wait_count[56985626345480]: 1
@wait_count[103356424437976]: 1
@wait_count[98105927924460]: 1
@wait_count[100230244264704]: 1
@wait_count[100828501087384]: 1
@wait_count[110850608726896]: 1
@wait_count[132361823963408]: 1
@wait_count[100230245059600]: 1
@wait_count[140119427437320]: 1
@wait_count[100230249621932]: 1
@wait_count[127437676601616]: 1
@wait_count[132361228386352]: 1
@wait_count[132361345784080]: 1
@wait_count[132361354176784]: 1
@wait_count[132361849141520]: 1
@wait_count[100230249621928]: 1
@wait_count[140726541700696]: 2
@wait_count[130404105253136]: 2
@wait_count[98105927889124]: 2
@wait_count[98105927889120]: 2
@wait_count[136268105121840]: 2
@wait_count[100658824055672]: 2
@wait_count[824638025568]: 2
@wait_count[100230244423164]: 2
@wait_count[96887506556344]: 2
@wait_count[100658825090464]: 2
@wait_count[130404105253160]: 2
@wait_count[98105927888288]: 2
@wait_count[98105927924456]: 2
@wait_count[67259188117512]: 3
@wait_count[130404101237888]: 3
@wait_count[98105927888368]: 3
@wait_count[100230249621956]: 3
@wait_count[136268038105232]: 3
@wait_count[106376837255448]: 3
@wait_count[100828501087088]: 4
@wait_count[138816944825096]: 4
@wait_count[136268399075600]: 4
@wait_count[94302283351192]: 4
@wait_count[100230244458784]: 4
@wait_count[136268375764096]: 4
@wait_count[56985626424200]: 4
@wait_count[135083659432712]: 4
@wait_count[100658823945568]: 4
@wait_count[106376837248320]: 4
@wait_count[136268038062912]: 5
@wait_count[127437249708080]: 5
@wait_count[110850608726976]: 5
@wait_count[127437676601640]: 5
@wait_count[136268038021536]: 5
@wait_count[100828501087172]: 6
@wait_count[140723330965688]: 6
@wait_count[110850608726980]: 6
@wait_count[100658824054904]: 6
@wait_count[99650149505568]: 6
@wait_count[100828501087168]: 6
@wait_count[140737130534424]: 6
@wait_count[135546725689412]: 7
@wait_count[99650149572688]: 7
@wait_count[100230244434792]: 7
@wait_count[135546694740036]: 7
@wait_count[135546706163780]: 7
@wait_count[100230249621952]: 7
@wait_count[135546699655236]: 7
@wait_count[94302283350956]: 7
@wait_count[136268399075624]: 7
@wait_count[140731460000600]: 8
@wait_count[140728945709496]: 9
@wait_count[94302283350976]: 10
@wait_count[136268360585472]: 10
@wait_count[140726541700776]: 13
@wait_count[99650149511448]: 15
@wait_count[100658825087264]: 17
@wait_count[48704930038664]: 24
@wait_count[94302283350896]: 28
@wait_count[94302283350980]: 29
@wait_count[110917073343152]: 37
@wait_count[135546731559252]: 47
@wait_count[18605799326600]: 48
@wait_count[140723330965768]: 50
@wait_count[140728945709576]: 69
@wait_count[135546724628836]: 77
@wait_count[67259188196232]: 79
@wait_count[132361354171568]: 91
@wait_count[132361345778864]: 99
@wait_count[132361345778956]: 1596
@wait_count[132361354171660]: 1874
@wait_count[132361345778952]: 14003
@wait_count[132361354171656]: 14396

@wait_ns: 
[512, 1K)              1 |                                                    |
[1K, 2K)              44 |                                                    |
[2K, 4K)              95 |                                                    |
[4K, 8K)              12 |                                                    |
[8K, 16K)            181 |                                                    |
[16K, 32K)           109 |                                                    |
[32K, 64K)           337 |                                                    |
[64K, 128K)         2303 |@@@@@@                                              |
[128K, 256K)        1230 |@@@                                                 |
[256K, 512K)         303 |                                                    |
[512K, 1M)         17751 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@|
[1M, 2M)            9696 |@@@@@@@@@@@@@@@@@@@@@@@@@@@@                        |
[2M, 4M)             433 |@                                                   |
[4M, 8M)              26 |                                                    |
[8M, 16M)              7 |                                                    |
[16M, 32M)             0 |                                                    |
[32M, 64M)             0 |                                                    |
[64M, 128M)            1 |                                                    |
[128M, 256M)          23 |                                                    |
[256M, 512M)          91 |                                                    |
[512M, 1G)            68 |                                                    |
[1G, 2G)              19 |                                                    |
[2G, 4G)             139 |                                                    |

@wait_start[1815]: 78110962974377
@wait_start[1817]: 78110963937591
@wait_start[1816]: 78110964753970
@wait_start[1814]: 78110964850991
@wait_start[23665]: 78117000243269
@wait_start[23487]: 78117000264831
@wait_start[23699]: 78117000355946
@wait_start[23522]: 78128727856317
@wait_start[23523]: 78128729730146
@wait_start[23526]: 78128729795509
@wait_start[23515]: 78128737726366
@wait_start[23687]: 78130515591959
@wait_start[23895]: 78135231534101
@wait_start[23507]: 78136165817329
@wait_start[23505]: 78136165829219
@wait_start[23636]: 78137000297671
@wait_start[23896]: 78137734686943
@wait_start[23898]: 78140238043548
@wait_start[23695]: 78141075518667
@wait_start[24615]: 78141086784961
@wait_start[24599]: 78141086797352
@wait_start[23611]: 78141168480895
@wait_start[23504]: 78141178264825
@wait_start[23506]: 78141178331293
@wait_start[23872]: 78141747699476
@wait_start[23874]: 78141747706467
@wait_start[23873]: 78141747760207
@wait_start[23842]: 78142042586722
@wait_start[23875]: 78142569079226
@wait_start[23897]: 78142740775711
@wait_start[23871]: 78142891575824
@wait_start[847]: 78143184866044
@wait_start[23843]: 78143298793702
@wait_start[24620]: 78143587902010
@wait_start[24624]: 78143587920111
@wait_start[24595]: 78143738313430
@wait_start[24600]: 78143782286629

@wait_time_ns[100658823976176]: 1442
@wait_time_ns[127437676601616]: 2385
@wait_time_ns[100230244264704]: 2935
@wait_time_ns[136268105121840]: 5190
@wait_time_ns[130404105253136]: 5379
@wait_time_ns[100658825090464]: 5671
@wait_time_ns[130404105253160]: 5770
@wait_time_ns[130404101237888]: 6102
@wait_time_ns[103356424437976]: 8064
@wait_time_ns[110850608726896]: 8375
@wait_time_ns[136268399075600]: 9196
@wait_time_ns[136268398682040]: 10238
@wait_time_ns[56985626345480]: 10558
@wait_time_ns[18605798588424]: 11761
@wait_time_ns[136268038021536]: 12431
@wait_time_ns[100658823945568]: 13232
@wait_time_ns[99650149572688]: 17330
@wait_time_ns[67259188117512]: 33488
@wait_time_ns[100658824055160]: 41693
@wait_time_ns[106376837248320]: 51119
@wait_time_ns[132361823963408]: 55567
@wait_time_ns[100828501087384]: 69772
@wait_time_ns[136268038105232]: 74741
@wait_time_ns[100230244423160]: 86441
@wait_time_ns[136268399075624]: 95409
@wait_time_ns[106376837255448]: 111575
@wait_time_ns[100828501087088]: 111605
@wait_time_ns[140728945709496]: 113518
@wait_time_ns[100230249621932]: 123737
@wait_time_ns[140726541700696]: 153949
@wait_time_ns[100230244458784]: 161182
@wait_time_ns[132361849141520]: 168457
@wait_time_ns[100230249621928]: 177721
@wait_time_ns[94302283351192]: 208995
@wait_time_ns[132361815570704]: 236324
@wait_time_ns[127437676601640]: 274300
@wait_time_ns[140723330965688]: 278457
@wait_time_ns[96887506556344]: 328607
@wait_time_ns[56985626424200]: 473257
@wait_time_ns[100230244423164]: 520742
@wait_time_ns[100658825087264]: 934706
@wait_time_ns[100658824054904]: 1019003
@wait_time_ns[136268375764096]: 1103801
@wait_time_ns[98105927888288]: 1384813
@wait_time_ns[100658824055672]: 1473538
@wait_time_ns[94302283350956]: 1569566
@wait_time_ns[824638025568]: 1667948
@wait_time_ns[136268360585472]: 1739853
@wait_time_ns[100230244434792]: 1789271
@wait_time_ns[99650149511448]: 2304927
@wait_time_ns[98105927924456]: 2673890
@wait_time_ns[136268038062912]: 2706214
@wait_time_ns[48704930038664]: 4511453
@wait_time_ns[98105927924460]: 4579295
@wait_time_ns[132361345778864]: 4621655
@wait_time_ns[132361354171568]: 5541174
@wait_time_ns[132361228386352]: 5832094
@wait_time_ns[94302283350896]: 6589711
@wait_time_ns[127437249708080]: 6981667
@wait_time_ns[67259188196232]: 9620617
@wait_time_ns[99650149505568]: 11467095
@wait_time_ns[18605799326600]: 15248216
@wait_time_ns[100230249621956]: 19383551
@wait_time_ns[100230249621952]: 54462282
@wait_time_ns[132361345778956]: 224001338
@wait_time_ns[132361354171660]: 265250068
@wait_time_ns[132361354176784]: 272899549
@wait_time_ns[100230245088192]: 500252407
@wait_time_ns[140119427437320]: 1287638359
@wait_time_ns[140115182363400]: 1675939045
@wait_time_ns[138816944825096]: 1676249578
@wait_time_ns[135083659432712]: 2782156011
@wait_time_ns[98105927889124]: 12008429520
@wait_time_ns[132361345778952]: 14085608196
@wait_time_ns[100230245059600]: 15000075462
@wait_time_ns[132361354171656]: 15427063768
@wait_time_ns[98105927889120]: 18210286661
@wait_time_ns[135083659432760]: 18890003377
@wait_time_ns[94302283350976]: 29286917257
@wait_time_ns[132361345784080]: 30376224901
@wait_time_ns[98105927893016]: 30663365720
@wait_time_ns[140731460000600]: 32002415286
@wait_time_ns[140737130534424]: 34260360368
@wait_time_ns[135546706163780]: 35005487932
@wait_time_ns[135546725689412]: 35006101626
@wait_time_ns[135546699655236]: 35006133525
@wait_time_ns[135546694740036]: 35006693025
@wait_time_ns[140723330965768]: 35989322725
@wait_time_ns[140726541700776]: 36001004572
@wait_time_ns[110917073343152]: 37011519215
@wait_time_ns[135546731559252]: 37655533201
@wait_time_ns[140728945709576]: 37994580347
@wait_time_ns[135546724628836]: 38524658513
@wait_time_ns[110850608726976]: 50049154980
@wait_time_ns[110850608726980]: 60059265150
@wait_time_ns[100828501087168]: 60142915687
@wait_time_ns[100828501087172]: 60146450283
@wait_time_ns[98105927888368]: 91991561769
@wait_time_ns[94302283350980]: 111808252909

@waits_by_tid[2607]: 1
@waits_by_tid[23487]: 1
@waits_by_tid[30097]: 1
@waits_by_tid[23665]: 1
@waits_by_tid[30095]: 1
@waits_by_tid[30116]: 2
@waits_by_tid[23523]: 2
@waits_by_tid[30094]: 2
@waits_by_tid[23895]: 2
@waits_by_tid[30093]: 2
@waits_by_tid[23526]: 2
@waits_by_tid[1817]: 3
@waits_by_tid[23505]: 3
@waits_by_tid[23897]: 3
@waits_by_tid[23896]: 3
@waits_by_tid[23507]: 3
@waits_by_tid[23699]: 4
@waits_by_tid[23898]: 4
@waits_by_tid[1814]: 4
@waits_by_tid[23506]: 4
@waits_by_tid[23881]: 4
@waits_by_tid[1816]: 5
@waits_by_tid[30088]: 5
@waits_by_tid[23636]: 5
@waits_by_tid[23695]: 6
@waits_by_tid[24624]: 7
@waits_by_tid[30096]: 7
@waits_by_tid[24615]: 7
@waits_by_tid[2124]: 7
@waits_by_tid[23504]: 7
@waits_by_tid[24599]: 7
@waits_by_tid[24620]: 7
@waits_by_tid[23611]: 8
@waits_by_tid[1815]: 8
@waits_by_tid[1989]: 12
@waits_by_tid[23872]: 16
@waits_by_tid[23873]: 16
@waits_by_tid[23842]: 16
@waits_by_tid[2040]: 17
@waits_by_tid[23875]: 18
@waits_by_tid[23874]: 20
@waits_by_tid[1793]: 21
@waits_by_tid[1999]: 22
@waits_by_tid[23609]: 24
@waits_by_tid[1799]: 27
@waits_by_tid[1930]: 28
@waits_by_tid[847]: 37
@waits_by_tid[24595]: 47
@waits_by_tid[23899]: 48
@waits_by_tid[23871]: 57
@waits_by_tid[24600]: 77
@waits_by_tid[23876]: 79
@waits_by_tid[23843]: 89
@waits_by_tid[30113]: 15696
@waits_by_tid[30114]: 16364

