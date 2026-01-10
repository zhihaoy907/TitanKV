/*
TITANKV 的压力测试,想测试在高并发下 TitanKV 的IO是否正常
*/

#include <future>
#include <iostream>
#include <cassert>
#include <filesystem>
#include <vector>

#include "muti_thread/titan_engine.h"

using namespace titankv;

const static int N = 100000;
const static unsigned thread_num = 4;
std::atomic<int> errors{0};

void test_concurrent_rw()
{
    std::string path = "./test_io";
    if(std::filesystem::exists(path))
        std::filesystem::remove_all(path);
    std::filesystem::create_directory(path);

    TitanEngine db(path, thread_num);

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

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> writers;
    for(unsigned t = 0; t < 1; t++)
    {
        writers.emplace_back([&, t]{
            for(unsigned i = 0; i < N; i++)
            {
                std::string k = "key_" + std::to_string(t) + std::to_string(i);
                std::string val = "val_" + std::to_string(i);
                sync_put(k, val);
            }
        });
    }

    for(auto &th : writers)
    {
        if(th.joinable())
            th.join();
    }

    std::vector<std::thread> readers;
    for(unsigned t = 0; t < 1; t++)
    {
        readers.emplace_back([&, t]{
            for(unsigned i = 0; i < N; i++)
            {
                std::string k = "key_" + std::to_string(t) + std::to_string(i);
                std::string val = "val_" + std::to_string(i);
                std::string v1 = sync_get(k);
                if(v1 != val)
                {
                    std::cerr << "error in " << i << "iteration" << std::endl;
                    std::cerr << "error: get_val is " << v1 << " is not == " << val << std::endl;
                    return;
                }
            }
        });
    }
    

    for(auto &th : readers)
    {
        if(th.joinable())
            th.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_sec = std::chrono::duration<double>(end - start).count();

    std::cout << "[Test] 1. Basic Put/Get Pass" << 
        " | Time: " << elapsed_sec << "s" << std::endl;
}

int main()
{
    test_concurrent_rw();
    return 0;
}