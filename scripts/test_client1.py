import socket
import struct
import time

client = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
client .connect(('127.0.0.1',8080))
print("成功连接服务器！")

text = "这是一条安全的数据测试"
payload = text.encode('utf-8')

header = struct.pack('!I',len(payload))

packet = header + payload

client.send(header)
print("包头已发送，假装网络极度卡顿 3 秒...")

time.sleep(3) # 强行延迟

# 3 秒后再发剩下的包体
client.send(payload)
print("包体终于发送完毕！")

client.send(packet + packet)
print(f"粘包发送完毕！总大小: {len(packet) * 2} 字节")

time.sleep(2)
client.close()