#pragma once
#include <string>
#include "BlockQueue.hpp"

constexpr uint32_t MAX_PAYLOAD_SIZE = 4 * 1024 * 1024;
struct Connection
{
    int fd;
    std::string input_buffer;
    BlockQueue<std::string> *task_queue_ptr;
    Connection(int fd_, BlockQueue<std::string> *q) : fd(fd_), input_buffer(""), task_queue_ptr(q) {}
    int parse()
    {
        // LOG_INFO("进入专属 parse,当前 buffer size = %zu", input_buffer.size());
        size_t read_index = 0;

        while (true)
        {
            size_t remaining = input_buffer.size() - read_index;
            // [占位：单元三的第一关 - 判断 remaining < 4 的半包拦截]
            if (remaining < 4)
            {
                break;
            }

            // [占位：单元三的第二关 - 提取 4 字节，防 OOM 熔断]
            uint32_t network_len = 0;
            std::memcpy(&network_len, input_buffer.data() + read_index, sizeof(uint32_t));
            uint32_t host_len = ntohl(network_len);
            // LOG_INFO("fd=%d 解析出 Payload 长度为: %u 字节", fd, host_len);
            if (host_len > MAX_PAYLOAD_SIZE)
            {
                LOG_ERROR("fd=%d 报文长度异常 (%u bytes)，触发 OOM 熔断，强杀连接！", fd, host_len);
                return -1;
            }

            // [占位：单元三的第三关 - 判断 remaining < 4 + expected_len 的半包拦截]
            if (remaining < 4 + host_len)
            {
                break;
            }

            // [占位：单元四 - 成功切出一个完整包，read_index 往后挪]
            std::string payload(input_buffer.data() + read_index + 4, host_len);
            // LOG_INFO("成功切出一个完整包,Payload 长度: %u, 内容: %s", host_len, payload.c_str());
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
