# TitanKV 写聚合功能说明
## 概述
**该功能通过将多个离散的小写请求在内存中聚合成一个连续的大块写操作，显著提升了高并发场景下的吞吐量与IO效率。**

测试代码参考：tests/bench_titankv_standalone.cpp

## 工作原理
<img width="1420" height="418" alt="image" src="https://github.com/user-attachments/assets/8a475fbb-b651-4069-b4d9-df8ccfd04e13" />

## 与不使用批量写的测试对比结果
**不使用批量写**
<img width="1533" height="412" alt="image" src="https://github.com/user-attachments/assets/9d192ecc-e843-49d8-9f6b-7ae40a7b1741" />

**使用批量写**
<img width="1561" height="404" alt="image" src="https://github.com/user-attachments/assets/abeb87ae-4405-4a78-8d4d-cc739ec20a85" />

## 结论
在极端情况下，写入同样的数据，使用批量写的吞吐量是不使用批量写的 **11** 倍，节省约 **90%** 时间

