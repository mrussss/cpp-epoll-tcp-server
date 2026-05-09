#pragma once

#include <cstdint>
#include <string>
#include "protocol/MessageType.hpp"

struct Response
{
    int fd;
    uint8_t version;
    MessageType type;
    uint64_t request_id;
    int status_code;
    std::string payload;
};
