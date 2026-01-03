/*
功能性测试，测试Encode接口是否ok
*/
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cassert>
#include <iomanip>

#include "common/buffer.h"
#include "storage/log_entry.h"

using namespace titankv;

void hexdump(const void* ptr, size_t len)
{
    const uint8_t* p = static_cast<const uint8_t*>(ptr);
    for(size_t i = 0; i < len; i++)
    {
        std::cout << std::hex << std::setw(2) << std::setfill('0')
            << (int)p[i] << " ";
        
        if((i+1)%16 == 0)
            std::cout << std::endl;
    }
}

void test_encode_basic()
{
    std::cout << "[Test] Encoding Basic Key-Value..." << std::endl;

    std::string key = "titan";
    std::string val = "kv";
    LogOp type = LogOp::PUT;

    size_t header_size = sizeof(LogHeader);
    size_t excepted_size = header_size + key.size() + val.size();

    // 验证 Header 大小是否紧凑排列
    assert(header_size == 13);
    assert(LogRecord::size_of(key, val) == excepted_size);

    std::vector<uint8_t> buffer(excepted_size);
    
    LogRecord::encode(key, val, type, buffer);

    auto* header = reinterpret_cast<LogHeader*>(buffer.data());

    assert(header->key_len == key.size());
    assert(header->val_len == val.size());
    assert(header->type == type);

    char* key_ptr = reinterpret_cast<char*>(buffer.data() + header_size);
    // 必须用 memcmp，不能用 strcmp，因为 buffer 里没有 \0 结尾
    assert(std::memcmp(key_ptr, key.data(), key.size()) == 0);

    char* val_ptr = key_ptr + key.size();
    assert(std::memcmp(val_ptr, val.data(), val.size()) == 0);

    std::cout << "Raw Memory Dump: " << std::endl;
    hexdump(buffer.data(), buffer.size());

    std::cout << "PASSED: BASIC Encode" << std::endl;
}


int main()
{
    test_encode_basic();
    return 0;
}