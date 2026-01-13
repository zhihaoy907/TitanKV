/*
压缩模块的功能性测试
*/
#include <iostream>
#include <filesystem>
#include <cassert>
#include <future>
#include "muti_thread/titan_engine.h"

using namespace titankv;

void sync_put(TitanEngine& db, std::string k, std::string v) {
    std::promise<void> p;
    auto f = p.get_future();
    db.Put(k, v, [&](int res){
        if (res < 0) { std::cerr << "Put error" << std::endl; exit(1); }
        p.set_value();
    });
    f.get();
}

void sync_delete(TitanEngine& db, std::string k) {
    std::promise<void> p;
    auto f = p.get_future();
    db.Delete(k, [&](int res){
        if (res < 0) { std::cerr << "Delete error" << std::endl; exit(1); }
        p.set_value();
    });
    f.get();
}

std::string sync_get(TitanEngine& db, std::string k) {
    std::promise<std::string> p;
    auto f = p.get_future();
    db.Get(k, [&](std::string val){
        p.set_value(val);
    });
    return f.get();
}

void test_compaction() {
    std::string path = "./test_compact_data";
    if (std::filesystem::exists(path)) std::filesystem::remove_all(path);
    std::filesystem::create_directory(path);

    TitanEngine db(path, 1); 
    
    std::cout << "[Step 1] Filling data (Generating Garbage)..." << std::endl;
    
    // 写入 k1
    sync_put(db, "k1", "val1_old");
    
    // 写入 k1
    sync_put(db, "k1", "val1_new");
    
    // 写入 k2
    sync_put(db, "k2", "val2");
    
    // 删除 k2
    sync_delete(db, "k2");
    
    // 写入 k3
    sync_put(db, "k3", "val3");
    
    std::string log_file = path + "/data_0.log";
    size_t size_before = std::filesystem::file_size(log_file);
    std::cout << "Size Before: " << size_before << " (Expected 20480)" << std::endl;
    assert(size_before == 20480);

    // 2. 压缩
    std::cout << "[Step 2] Compacting..." << std::endl;
    db.Compact();

    // 验证大小
    size_t size_after = std::filesystem::file_size(log_file);
    std::cout << "Size After:  " << size_after << " (Expected 8192)" << std::endl;
    
    assert(size_after == 8192);
    assert(size_after < size_before);

    // 验证数据正确性
    std::cout << "[Step 3] Verifying Data..." << std::endl;
    
    // k1 应该是新值
    assert(sync_get(db, "k1") == "val1_new");
    
    // k2 应该找不到
    assert(sync_get(db, "k2") == "");
    
    // k3 应该在
    assert(sync_get(db, "k3") == "val3");

    std::cout << "PASS: Compaction Test" << std::endl;
}

int main() {
    test_compaction();
    return 0;
}