#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string_view>
#include <span>

#include "common/common.h"
#include "common/buffer.h"

TITANKV_NAMESPACE_OPEN

enum class LogOp : uint8_t
{
    PUT = 1,
    DELETE = 2
};

// 强制1字节对齐
#pragma pack(push, 1)
struct LogHeader
{
    // crc32 校验码
    uint32_t crc;
    // key 长度
    uint32_t key_len;
    // value 长度
    uint32_t val_len;
    // 操作类型
    LogOp type;
};
#pragma pack(pop)



class LogRecord
{
public:
    static size_t size_of(std::string_view key, std::string_view val)
    {
        return sizeof(LogHeader) + key.size() + val.size();
    }

    static void encode(std::string_view key, 
                        std::string_view val,
                        LogOp type,
                        std::span<uint8_t> dest_buf);

    static uint32_t calculate_crc(const uint8_t* data, size_t len);


};


TITANKV_NAMESPACE_CLOSE