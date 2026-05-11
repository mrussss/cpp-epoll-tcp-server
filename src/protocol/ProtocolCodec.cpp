#include "protocol/ProtocolCodec.hpp"
#include <cstring>
#include <arpa/inet.h>
#include <endian.h>

std::string ProtocolCodec::encode(const Response &response)
{

    std::string result;

    // 步骤 B：计算 body_length (等于版本1 + 类型1 + id8 + payload的长度)
    uint32_t body_length = 1 + 1 + 8 + response.payload.size();

    // 步骤 C：核心！将整型转为网络字节序（大端）
    uint32_t net_body_len = htonl(body_length);
    uint64_t net_req_id = htobe64(response.request_id);

    // 步骤 D：转换强类型枚举
    uint8_t version = response.version;
    uint8_t msg_type = static_cast<uint8_t>(response.type);

    // 步骤 E：按严格顺序追加到结果字符串中
    // 提示：使用 reinterpret_cast<const char*> 取地址，并用 sizeof 获取字节数
    result.append(reinterpret_cast<const char *>(&net_body_len), sizeof(net_body_len));
    result.append(reinterpret_cast<const char *>(&version), sizeof(version));
    result.append(reinterpret_cast<const char *>(&msg_type), sizeof(msg_type));
    result.append(reinterpret_cast<const char *>(&net_req_id), sizeof(net_req_id));

    // 最后把变长的 payload 直接追加进去
    result.append(response.payload);

    return result;
}

DecodeStatus ProtocolCodec::decode(std::string &input_buffer, int fd, std::vector<Request> &out_requests)
{
    size_t read_index = 0;
    while (true)
    {

        size_t remaining = input_buffer.size() - read_index;
        // --- 边界条件 1：连“头盔”都没凑齐 ---
        // 1. 判断 input_buffer 的大小是否小于 4 字节
        //    如果是，说明半包，直接 break 跳出循环，等下次数据来
        if (remaining < 4)
        {
            break;
        }
        // --- 提取长度 ---
        // 2. 从 buffer 最前面 4 个字节中，用 memcpy 把长度抠出来存进局部变量
        // 3. 将网络字节序长度 ntohl 转换为主机字节序，得到 host_body_length
        uint32_t network_body_len = 0;
        std::memcpy(&network_body_len, input_buffer.data() + read_index, sizeof(uint32_t));
        uint32_t host_body_length = ntohl(network_body_len);

        // --- 边界条件 2：恶意攻击防御 ---
        // 4. 判断 host_body_length 是否大于 MAX_PAYLOAD_SIZE (比如设定为 4MB)
        //    如果是，不要再等了，直接 return DecodeStatus::INVALID_LENGTH;
        if (host_body_length > MAX_PAYLOAD_SIZE)
        {
            return DecodeStatus::INVALID_LENGTH;
        }
        // --- 边界条件 3：“身子”还没到齐 ---
        // 5. 判断 input_buffer 的总大小是否小于 (4 + host_body_length)
        //    如果是，说明包裹只到了一半（半包），直接 break 跳出循环
        if (remaining < (4 + host_body_length))
        {
            break;
        }

        // ==========================================
        // 恭喜！能走到这里，说明 buffer 里【至少】包含了一个极其完整的包！
        // ==========================================

        // --- 精准解剖 ---
        // 6. 准备一个 Request 对象 req
        // 7. 从 buffer 的第 4 字节开始，提取 version (1字节) 给 req
        // 8. 从 buffer 的第 5 字节开始，提取 type (1字节)，记得用 static_cast 转成 MessageType 枚举 给 req
        // 9. 从 buffer 的第 6 字节开始，提取 8 字节的 request_id，用 be64toh 转回主机字节序 给 req
        // 10. 从 buffer 的第 14 字节开始，提取长度为 (host_body_length - 10) 的内容，作为 payload 字符串 给 req
        // 11. 把 req 的 fd 字段也赋上（传入的参数）
        Request req;
        req.version = static_cast<uint8_t>(input_buffer[read_index + 4]);
        req.type = static_cast<MessageType>(input_buffer[read_index + 5]);
        uint64_t request_id_net;
        std::memcpy(&request_id_net, input_buffer.data() + read_index + 6, 8); // 把 8 字节拷入 64位变量
        req.request_id = be64toh(request_id_net);                              // 转换
        req.payload = std::string(input_buffer.data() + read_index + 14, host_body_length - 10);
        req.fd = fd;

        // 12. 把拼装好的 req 塞进 out_requests 数组
        out_requests.push_back(req);
        // --- 剪掉废胶带（极其关键） ---
        // 13. 把刚刚消耗掉的 (4 + host_body_length) 个字节，从 input_buffer 中 erase 掉！
        read_index += (4 + host_body_length);
        if (read_index > 0)
        {
            input_buffer.erase(0, read_index);
        }
        //     然后随着 while 循环进入下一轮，看看剩下的数据还能不能再凑出一个包！
    }

    // 循环因为 break 结束，说明剩下的数据不够一个包了，状态是正常的等待
    return DecodeStatus::OK;
}