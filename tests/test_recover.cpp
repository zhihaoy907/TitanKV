/*
异常退出的功能性测试代码
*/

#include <future>
#include <iostream>
#include <cassert>
#include <filesystem>
#include <vector>
#include <thread>

#include "muti_thread/titan_engine.h"

using namespace titankv;

const std::string TEST_DIR = "./test_recover_data";
const int NUM_KEYS = 1000;

void sync_put(TitanEngine& db, int i)
{
    std::string key = "key_" + std::to_string(i);
    std::string val = "val_" + std::to_string(i);

    std::promise<void> p;
    auto f = p.get_future();
    db.Put(key, val, [&](int res){
        if (res <= 0) std::cerr << "Put failed: " << key << std::endl;
        p.set_value(); 
    });
    f.get();
}

void sync_check(TitanEngine& db, int i)
{
    std::string key = "key_" + std::to_string(i);
    std::string expected_val = "val_" + std::to_string(i);

    std::promise<std::string> p;
    auto f = p.get_future();
    db.Get(key, [&](std::string val){
        p.set_value(val);
    });

    std::string actual_val = f.get();
    if(actual_val !=  expected_val)
    {
        std::cerr << "Mismatch! Key=" << key 
                  << " Expected=" << expected_val 
                  << " Actual=" << actual_val << std::endl;
        std::terminate();
    }
}

std::string sync_get_safe(TitanEngine& db, int i) {
    std::string key = "key_" + std::to_string(i);
    
    std::promise<std::string> p;
    auto f = p.get_future();
    db.Get(key, [&](std::string val) {
        p.set_value(val);
    });
    
    return f.get();
}

void test_crash_recovery()
{
    if(std::filesystem::exists(TEST_DIR))
        std::filesystem::remove_all(TEST_DIR);
    std::filesystem::create_directory(TEST_DIR);

    std::cout << "[Phase 1] Writing " << NUM_KEYS << "keys..." << std::endl;
    {
        TitanEngine db(TEST_DIR, 2);
        for(unsigned i = 0; i < NUM_KEYS; i++)
            sync_put(db, i);

        std::cout << "[Phase 1] Write Done. Closing DB..." << std::endl;
    }

    std::cout << "[Hack] Simulating a Crash (Corrupting file tail)..." << std::endl;
    {
        std::string filename = TEST_DIR + "/data_0.log"; 
        
        int fd = ::open(filename.c_str(), O_RDWR);
        assert(fd > 0);
        
        // 获取当前大小
        off_t size = ::lseek(fd, 0, SEEK_END);
        
        // 模拟只写了一半的 LogEntry
        if (size > 0) 
        {
            ::ftruncate(fd, size - 5); 
            std::cout << "  -> Truncated 5 bytes from " << filename << std::endl;
        }
        ::close(fd);
    }

    std::cout << "[Phase 2] Restarting DB (Recovery)..." << std::endl;
    {
        TitanEngine db(TEST_DIR, 2);

        std::cout << "[Phase 3] Verifying Data..." << std::endl;
        for (int i = 0; i < NUM_KEYS - 1; ++i) {
            sync_check(db, i);
        }
        std::cout << "[Phase 3] All keys verified!" << std::endl;

        std::string last_val = sync_get_safe(db, NUM_KEYS);
        
        if (last_val == "") 
        {
            std::cout << "  -> [OK] Last record was successfully truncated." << std::endl;
        } 
        else 
        {
            std::cout << "  -> [Info] Last val is: " << last_val << " ,record survived (Maybe padding saved it)." << std::endl;
        }

        std::cout << "[Phase 4] Testing Delete..." << std::endl;

        for(unsigned i = 0; i < 100; ++i)
        {
            std::string key = "key_" + std::to_string(i);

            std::promise<void> p;
            auto f = p.get_future();
            db.Delete(key, [&](int res){
                assert(res > 0);
                p.set_value();
            });
            f.get();
        }
        std::cout << " -> Deleted 100 keys. " << std::endl;

        for(int i = 0; i < 100; ++i)
        {
            std::string val = sync_get_safe(db, i);
            assert(val == "");
        }
        std::cout << "  -> Memory check passed." << std::endl;
    }

    std::cout << "[Phase 5] Restarting to Verify Delete Persistence..." << std::endl;
    {
        TitanEngine db(TEST_DIR, 2);
        
        for (int i = 0; i < 100; ++i) 
        {
            std::string val = sync_get_safe(db, i);
            if (val != "") 
            {
                std::cerr << "Zombie Key Found! " << "key_" << i << std::endl;
                std::terminate();
            }
        }
        
        for(int i = 100; i < NUM_KEYS - 1; ++i) 
            sync_check(db, i);
        
        std::cout << "  -> Persistence check passed." << std::endl;
    }

}

int main() 
{
    test_crash_recovery();
    std::cout << "PASS: Crash Recovery Test" << std::endl;
    return 0;
}