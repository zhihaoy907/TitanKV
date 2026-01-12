#include "storage/log_reader.h"

#include <filesystem>
#include <fcntl.h>
#include <unistd.h>

using namespace titankv;

LogReader::LogReader(const std::string filename)
: current_offset_(0), valid_offset_(0)
{
    fd_ = ::open(filename.c_str(), O_RDONLY);
}

LogReader::~LogReader() 
{
    if (fd_ > 0) ::close(fd_);
}

bool LogReader::Next(LogHeader& out_header, std::string& out_key, uint64_t& out_offset)
{
    ssize_t n = ::pread(fd_, &out_header, sizeof(LogHeader), current_offset_);
    // 文件结束/文件尾部损坏
    if(n == 0 || n < (ssize_t)sizeof(LogHeader)) [[unlikely]]
        return false;
    
    if(out_header.key_len > 1024 * 1024) [[unlikely]]
        return false;
    
    std::string key(out_header.key_len, '\0');
    n = ::pread(fd_, key.data(), out_header.key_len, current_offset_ + sizeof(LogHeader));

    // Key 是断的
    if(n < (ssize_t)out_header.key_len) [[unlikely]]
        return false;

    size_t real_size = sizeof(LogHeader) + out_header.key_len + out_header.val_len;
    size_t aligned_size = (real_size + 4095) & ~4095;
    out_key = std::move(key);
    out_offset = current_offset_;

    current_offset_ += aligned_size;
    valid_offset_ = current_offset_;
    
    return true;
}

uint64_t LogReader::GetValidEndOffset() const
{
    return valid_offset_;
}