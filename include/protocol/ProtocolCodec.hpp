#pragma once

#include <string>
#include <vector>

#include "protocol/Request.hpp"
#include "protocol/Response.hpp"

constexpr uint32_t MAX_PAYLOAD_SIZE = 4 * 1024 * 1024;

enum class DecodeStatus
{
    OK,
    NEED_MORE_DATA, // 数据不够 4 字节，或者不够完整的 body_length
    INVALID_LENGTH, // 长度字段异常巨大 (防 OOM)
};

class ProtocolCodec
{
public:
    // 解码接口：
    static DecodeStatus decode(std::string &input_buffer, int fd, std::vector<Request> &out_requests);

    // 编码接口：
    static std::string encode(const Response &response);
};