/*
TITANKV 输入输出的基准测试，本次测试不测试性能，只验证功能性。
本次测试包含三个部分：
1、测试最基本的输入输出路径，判断从磁盘读取的数据跟刚刚提交给 TitanKV 的数据是否一致
2、针对不存在的key值， TitanKV 是否可以返回 “”
3、是否可以实时更新 key 对应的 value 值
在 test_concurrency.cpp 中完成了并发场景下针对该功能的压力测试
*/
#include <future>
#include <iostream>
#include <cassert>
#include <filesystem>
#include <vector>

#include "muti_thread/titan_engine.h"

using namespace titankv;

void test_rw_integrity()
{
    std::string path = "./test_io";
    if(std::filesystem::exists(path))
        std::filesystem::remove_all(path);
    std::filesystem::create_directory(path);

    TitanEngine db(path, 2);

    auto sync_put = [&](std::string k, std::string v){
        // promise 有点影响性能，后续优化
        std::promise<void> p;
        auto f = p.get_future();
        db.Put(k, v, [&](int res){
            assert(res > 0);
            p.set_value();
        });
        f.get();
    };
    
    auto sync_get = [&](std::string k) -> std::string {
        std::promise<std::string> p;
        auto f = p.get_future();
        db.Get(k, [&](std::string val) {
            p.set_value(val);
        });
        return f.get();
    };
    
    // sleep(5);
    std::cout << "[Test] 1. Basic Put/Get..." << std::endl;
    sync_put("key1", "value1");
    std::string v1 = sync_get("key1");
    assert(v1 == "value1");
    std::cout << " -> Pass" << std::endl;

    std::cout << "[Test] 2. Key Not Found..." << std::endl;
    std::string v_none = sync_get("key_not_exist");
    assert(v_none == "");
    std::cout << "-> Pass" << std::endl; 

    std::cout << "[Test] 3. Update Key..." << std::endl;
    sync_put("key1", "value_new");
    std::string v1_new = sync_get("key1");
    assert(v1_new == "value_new");
    std::cout << "  -> Pass (Got: " << v1_new << ")" << std::endl;
}

int main()
{
    test_rw_integrity();
    return 0;
}