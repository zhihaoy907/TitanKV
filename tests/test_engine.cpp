#include <future>
#include <iostream>
#include <cassert>
#include <filesystem>
#include "muti_thread/titan_engine.h"

using namespace titankv;

void test_put_get()
{
    std::string path = "./test_data_engine";
    if (std::filesystem::exists(path)) 
        std::filesystem::remove_all(path);

    std::filesystem::create_directory(path);

    {
        titankv::TitanEngine db(path, 2);

        std::promise<int> promise;
        auto future = promise.get_future();

        std::cout << "[Test] Putting key..." << std::endl;

        db.Put("user_id", "123456", [&](int res)
        {
            if(res < 0) 
                std::cerr << "IO error: " << -res << std::endl;
            
            promise.set_value(res);
        });

        int result = future.get();

        assert(result > 0 && "Write should return bytes written");
        std::cout << "[Test] Put Success! Bytes written: " << result << std::endl;
    } 
}

int main()
{
    test_put_get();
    return 0;
}