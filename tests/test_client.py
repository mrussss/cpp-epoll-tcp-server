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

client.send(packet)
print(f"数据发送完毕！总大小：{len(packet)} 字节")

time.sleep(2)
client.close()