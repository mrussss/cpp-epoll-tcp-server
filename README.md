# cpp-epoll-tcp-server

本项目是一个基于 Linux `epoll` 与非阻塞 socket 实现的 TCP 服务端框架。

项目围绕服务端网络编程中的核心流程展开，主要用于学习和实践 C++ 服务端开发中的网络 IO、事件驱动模型、多线程协作和基础工程组织。

---

## 📂 仓库结构

```text
cpp-epoll-tcp-server
├── include/
│   ├── BlockQueue.hpp      # 阻塞队列，用于 IO 线程与 worker 线程解耦
│   ├── Connection.hpp      # 连接对象，维护连接状态与输入缓冲区 (处理半包/粘包/OOM拦截)
│   └── Logger.hpp          # 基于 std::mutex 的线程安全日志模块
├── src/
│   └── server_tcp.cpp      # 服务端主程序 (epoll 事件循环、线程池分配)
├── tests/                  # Python 测试脚本 (包含基础发包、毒包测试与高并发压测)
├── CMakeLists.txt          # CMake 构建配置文件
└── .gitignore
```

## 🛠️ 技术栈

* **C++17**
* **Linux 网络编程**: TCP socket, `epoll` (ET 边缘触发模式), 非阻塞 IO (Non-blocking IO)
* **多线程并发**: `std::thread`, `std::mutex`, `std::condition_variable`
* **构建与调试**: CMake, GDB, AddressSanitizer (ASan)
* **测试**: Python 

## ✨ 核心功能与亮点

1.  **基于 epoll ET 的事件循环**：服务端使用 `epoll` 监听连接事件和读事件，并采用 ET (Edge Triggered) 边缘触发模式。配合非阻塞 socket，避免单个连接阻塞整个事件循环。
2.  **非阻塞 accept 与 recv**：在 ET 模式下，采用 `while(true)` 循环读取的方式，直到返回 `EAGAIN` 或 `EWOULDBLOCK`，确保一次性榨干缓冲区数据，防止事件遗漏。
3.  **Connection 连接档案封装**：通过 `Connection` 类维护单个客户端连接的上下文。每个连接独立管理自己的客户端 fd 和 `input_buffer`（输入缓冲区），彻底解耦底层 IO 与上层协议解析。
4.  **长度前缀协议解析 (解决粘包/半包)**：
    * **协议格式**：`| 4字节网络字节序长度 | 变长消息体 |`
    * **处理机制**：数据先统一写入 `input_buffer`，解析模块再循环判断长度。完美解决 TCP 字节流传输中产生的**半包**（分片到达）和**粘包**（多条连发）问题。
    * **防御编程**：内置最大 Payload 拦截机制，遇到恶意超大报文直接触发 OOM 熔断并物理断开连接。
5.  **IO 线程与 worker 线程彻底解耦**：
    * 使用线程安全的 `BlockQueue<std::string>` 作为任务传送带。
    * IO 线程只负责 `epoll_wait`、数据读取和切包，将切好的完整报文直接 Push 入队。
    * 业务（Worker）线程只负责从队列 Pop 任务并处理，避免耗时业务卡死网络 IO。
6.  **智能 worker 线程池**：启动时根据服务器物理核心数动态拉起 Worker 线程。队列为空时自动休眠（避免 CPU 空转），队列关闭时优雅退出。
7.  **优雅退出与资源大扫除**：捕获 `SIGINT` (Ctrl+C) 信号，依次安全关闭所有存活的客户端 fd、释放 epoll fd、停止任务队列并 `join` 等待所有 worker 线程安全撤退。

## 🚀 快速开始

### 1. 编译项目

请确保你的 Linux 环境已安装 `g++` 和 `cmake`。

```bash
# 在项目根目录下创建 build 文件夹
mkdir build
cd build

# 执行 CMake 生成 Makefile 并编译
cmake ..
make
```

### 2. 运行服务端

在 `build` 目录下运行编译生成的服务端程序：

```bash
./server_tcp
```
*服务端默认监听本地 `8080` 端口。启动后可以通过 `Ctrl+C` 触发服务端优雅退出流程。*

### 3. 执行测试

项目中的 `tests/` 目录存放了多个 Python 测试脚本，用于模拟极其恶劣的网络环境和并发场景：

```bash
# 新开一个终端，进入测试目录
cd tests/

# 1. 基础连通性测试
python3 test_client.py

# 2. 极度卡顿模拟 (测试半包与粘包处理能力)
python3 test_client1.py

# 3. 恶意毒包测试 (发送伪造的超大 Header，测试防 OOM 熔断)
python3 test_client2.py

# 4. 高并发压测 (拉起 100 个线程，狂轰 10000 条消息)
python3 stress_test.py
```