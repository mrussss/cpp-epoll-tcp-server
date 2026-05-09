#pragma once

#include <cstdint>

enum class MessageType : uint8_t
{
    PING = 1,
    ECHO = 2,
    LOG_PUSH = 3,
    STATS = 4
};