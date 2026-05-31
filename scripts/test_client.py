import socket
import struct
import time

def send_request_and_wait_response(client, msg_type, payload_str, req_id):
    """
    封装好的请求-响应流：发送 Request -> 阻塞等待 -> 解析 Response
    """
    # ==========================================
    # 阶段 1：打包并发送 Request
    # ==========================================
    payload = payload_str.encode('utf-8')
    version = 1
    
    # 计算 Body 总长度 (version 1字节 + type 1字节 + req_id 8字节 + payload 长度)
    body_length = 1 + 1 + 8 + len(payload)
    
    # 组装网络包 (包头 4 字节 + 10 字节元数据 + 变长 payload)
    header_and_meta = struct.pack('!IBBQ', body_length, version, msg_type, req_id)
    packet = header_and_meta + payload
    
    print(f"\n[-> 发送请求] Type={msg_type}, ReqID={req_id}, Payload='{payload_str}'")
    client.sendall(packet)
    
    # ==========================================
    # 阶段 2：接收并解析 Response (任务 5.1)
    # ==========================================
    
    # 任务 5.2：获取 4 字节响应头，解析 body_length
    header = client.recv(4)
    if not header or len(header) < 4:
        print("[!] 错误：服务端断开连接或未返回完整头部")
        return
        
    (resp_body_length,) = struct.unpack('!I', header)
    
    # 任务 5.3：接着读取 body_length 长度的 Body
    # 注意细节：网络环境下 recv 不一定一次能收满期望的字节数，严谨的做法是用 while 循环收满为止
    body = b''
    while len(body) < resp_body_length:
        chunk = client.recv(resp_body_length - len(body))
        if not chunk:
            break
        body += chunk
        
    if len(body) < resp_body_length:
        print("[!] 错误：数据体接收不完整")
        return
        
    # 解构 Body 前 10 个字节 (version, type, request_id)
    resp_version, resp_type, resp_req_id = struct.unpack('!BBQ', body[:10])
    
    # 剩余的字节全部是 payload
    resp_payload = body[10:].decode('utf-8')
    
    # 任务 5.4：终端打印验证
    print(f"[<- 收到响应] Version={resp_version}, Type={resp_type}, ReqID={resp_req_id}, Payload='{resp_payload}'")


if __name__ == '__main__':
    # 连接到你的 C++ Epoll 服务端
    client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    client.connect(('127.0.0.1', 8080))
    print("✅ 成功连接到服务器！")

    # ---------------------------------------------------------
    # 测试场景 1：发送 PING (MessageType::PING = 1)
    # 期望：服务端返回 PONG (Type = 5)，Payload 包含 {"message":"pong"}
    # ---------------------------------------------------------
    send_request_and_wait_response(client, msg_type=1, payload_str="ping from client", req_id=1001)
    time.sleep(1)

    # ---------------------------------------------------------
    # 测试场景 2：发送 ECHO (MessageType::ECHO = 2)
    # 期望：服务端原样返回 ECHO_RESP (Type = 6) 以及你发过去的 Payload
    # ---------------------------------------------------------
    send_request_and_wait_response(client, msg_type=2, payload_str="Hello Epoll Server!", req_id=1002)
    
# ... 原来的 PING 和 ECHO 测试 ...

    # ---------------------------------------------------------
    # 测试场景 3：发送 LOG_PUSH (MessageType::LOG_PUSH = 3)
    # 期望：服务端将这条消息落盘，并返回 LOG_ACK (Type = 8)
    # ---------------------------------------------------------
    log_payload = '{"level":"INFO", "service":"auth-service", "message":"user login success"}'
    send_request_and_wait_response(client, msg_type=3, payload_str=log_payload, req_id=1003)
    
    time.sleep(1)
    client.close()
    print("\n✅ 所有测试流执行完毕，连接关闭。")