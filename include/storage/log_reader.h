#pragma once
/*
文件恢复，用于异常中断的文件恢复。
此代码仍能优化，后续考虑改为一次预读1M，而不是每次都读取13字节
*/
#include <string>

#include "common/common.h"
#include "storage/log_entry.h"

TITANKV_NAMESPACE_OPEN

class LogReader
{
public:
    LogReader(const std::string filename);

    ~LogReader();

    TITANKV_NODISCARD bool Next(LogHeader& out_header, std::string& out_key, uint64_t& out_offser);

    uint64_t GetValidEndOffset() const;

private:
    int fd_;
    uint64_t current_offset_;
    uint64_t valid_offset_;

};

TITANKV_NAMESPACE_CLOSE