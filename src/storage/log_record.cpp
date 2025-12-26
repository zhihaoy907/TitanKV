#include "storage/log_entry.h"

#include <cstring>
#include <stdexcept>

TITANKV_NAMESPACE_OPEN

void LogRecord::encode(std::string_view key,
                        std::string_view val,
                        LogOp type,
                        std::span<uint8_t> dest_buf)
{
    size_t total_len = size_of(key, val);
    if(dest_buf.size() < total_len)
        throw std::length_error("Buffer too small for Log Record");

    uint8_t* ptr = dest_buf.data();

    auto* header = reinterpret_cast<LogHeader*>(ptr);
    header->key_len = static_cast<uint32_t>(key.size());
    header->val_len = static_cast<uint32_t>(val.size());
    header->type = type;
    header->crc = 0;

    ptr += sizeof(LogHeader);

    std::memcpy(ptr, val.data(), val.size());
}


uint32_t LogRecord::calculate_crc(const uint8_t* data, size_t len) 
{
    // 临时
    return 0; 
}


TITANKV_NAMESPACE_CLOSE