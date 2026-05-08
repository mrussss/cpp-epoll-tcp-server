#pragma once
#include <string>
#include "concurrent/BlockQueue.hpp"
#include "common/Logger.hpp"

constexpr uint32_t MAX_PAYLOAD_SIZE = 4 * 1024 * 1024;
struct Connection
{
    int fd;
    std::string input_buffer;
    BlockQueue<std::string> *task_queue_ptr;
    Connection(int fd_, BlockQueue<std::string> *q) : fd(fd_), input_buffer(""), task_queue_ptr(q) {}
    int parse()
    {
        // LOG_INFO("entering parse, current buffer size = %zu", input_buffer.size());
        size_t read_index = 0;

        while (true)
        {
            size_t remaining = input_buffer.size() - read_index;
            // [Step 1: Check for half-packet (remaining < 4)]
            if (remaining < 4)
            {
                break;
            }

            // [Step 2: Extract 4-byte length, OOM protection]
            uint32_t network_len = 0;
            std::memcpy(&network_len, input_buffer.data() + read_index, sizeof(uint32_t));
            uint32_t host_len = ntohl(network_len);
            // LOG_INFO("fd=%d parsed payload length: %u bytes", fd, host_len);
            if (host_len > MAX_PAYLOAD_SIZE)
            {
                LOG_ERROR("fd=%d invalid payload length (%u bytes), triggering OOM protection, closing connection!", fd, host_len);
                return -1;
            }

            // [Step 3: Check for half-packet (remaining < 4 + host_len)]
            if (remaining < 4 + host_len)
            {
                break;
            }

            // [Step 4: Successfully parsed a packet, advance read_index]
            std::string payload(input_buffer.data() + read_index + 4, host_len);
            // LOG_INFO("successfully parsed a complete packet, payload length: %u, content: %s", host_len, payload.c_str());
            read_index += (4 + host_len);
            if (task_queue_ptr != nullptr)
            {
                task_queue_ptr->push(payload);
            }
        }
        if (read_index > 0)
        {
            input_buffer.erase(0, read_index);
        }

        return 0;
    }
};
