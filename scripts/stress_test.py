import socket
import struct
import threading
import time

def run_client(client_id, msg_count):
    """
    这是每个客户端具体干的活：连接服务器，疯狂发包，然后断开
    """
    try:
        # 1. 建立 TCP 连接 (对应 C++ 的 socket() 和 connect())
        client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client.connect(('127.0.0.1', 8080))
        
        for i in range(msg_count):
            text = f"[Client-{client_id} Msg-{i}] 这是一条高并发压测数据"
            payload = text.encode('utf-8')
            
            # V2 协议元数据
            version = 1
            msg_type = 1 # 对应 MessageType::PING
            req_id = client_id * 1000 + i # 随便生成一个唯一 ID
            
            # 计算 Body 总长度 = 版本(1) + 类型(1) + ID(8) + 字符串长度
            body_length = 1 + 1 + 8 + len(payload)
            
            # 使用 '!I B B Q' 打包: 
            # ! = 大端网络字节序
            # I = 4字节无符号整型 (body_length)
            # B = 1字节无符号整型 (version)
            # B = 1字节无符号整型 (msg_type)
            # Q = 8字节无符号长整型 (req_id)
            header_and_meta = struct.pack('!IBBQ', body_length, version, msg_type, req_id)
            
            packet = header_and_meta + payload
            client.send(packet)
            time.sleep(0.001)
            
        # 发送完毕，正常断开连接。这会向 C++ 发送一个 EOF，触发 recv == 0
        client.close()
        
    except Exception as e:
        print(f"Client-{client_id} 炸了: {e}")

if __name__ == '__main__':
    print("🚀 开始启动高并发压测...")
    start_time = time.time()
    
    threads = []
    
    # 拉起 100 个并发连接
    for i in range(100):
        # target 是要执行的函数，args 是传给函数的参数 (client_id=i, msg_count=100)
        t = threading.Thread(target=run_client, args=(i, 100))
        threads.append(t)
        t.start()
        
    # 主线程在这里等待，直到那 100 个线程全部干完活
    for t in threads:
        t.join()
        
    end_time = time.time()
    print(f"✅ 压测结束！总计 10000 条消息，耗时: {end_time - start_time:.2f} 秒")