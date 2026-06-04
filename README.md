# MessageServer — C++17 Epoll TCP Server

一个基于 Linux `epoll`（ET 边缘触发模式）和非阻塞 socket 实现的轻量级 TCP 消息服务端，支持多消息类型、异步 Worker 线程池、日志落盘与实时监控统计。

---

## 📂 项目结构

```
cpp-epoll-tcp-server
├── include/
│   ├── common/
│   │   └── Logger.hpp              # 线程安全日志模块（可变参数模板）
│   ├── concurrent/
│   │   └── BlockQueue.hpp           # 阻塞/非阻塞队列，IO 与 Worker 解耦
│   ├── net/
│   │   ├── Connection.hpp           # 连接对象（含 conn_id 世代校验）
│   │   ├── SocketUtil.hpp           # Socket 工具函数（非阻塞设置）
│   │   └── TcpServer.hpp            # 核心服务器接口
│   ├── protocol/
│   │   ├── MessageType.hpp          # 消息类型枚举（PING/ECHO/LOG_PUSH/STATS…）
│   │   ├── ProtocolCodec.hpp        # 编解码器（定长头+变长体协议）
│   │   ├── Request.hpp              # 请求数据结构
│   │   └── Response.hpp             # 响应数据结构
│   ├── business/
│   │   ├── Dispatcher.hpp           # 消息分发器
│   │   ├── Handlers.hpp             # 业务处理函数声明
│   │   ├── LogStorage.hpp           # 日志落盘存储
│   │   └── StatsManager.hpp         # 全局监控统计
│   └── nlohmann/
│       └── json.hpp                 # JSON 解析库（单头文件）
├── src/
│   ├── main.cpp                     # 服务端入口
│   ├── net/
│   │   └── TcpServer.cpp            # epoll 事件循环（核心）
│   ├── protocol/
│   │   └── ProtocolCodec.cpp        # 粘包/半包处理 & 编码解码
│   └── business/
│       ├── Dispatcher.cpp           # 请求分发
│       ├── Handlers.cpp             # 业务处理实现
│       ├── LogStorage.cpp           # 日志追加写入
│       └── StatsManager.cpp         # 监控指标管理
├── scripts/
│   ├── test_client.py               # 基础连通性测试
│   ├── test_client1.py              # 半包/粘包模拟
│   ├── test_client2.py              # 恶意毒包测试（OOM 熔断）
│   └── stress_test.py               # 高并发压测（100线程 × 100消息）
├── logs/
│   └── access.log                   # 业务日志文件
├── docs/
│   └── v4.1修改建议.md              # 稳定性打磨技术文档
├── CMakeLists.txt
└── README.md
```

---

## 🛠️ 技术栈

| 层级 | 技术 |
|---|---|
| 语言 | **C++17** |
| 网络 IO | **Linux epoll**（Edge Triggered）+ 非阻塞 socket |
| 并发模型 | **多线程**：`std::thread` + `std::mutex` + `std::condition_variable` |
| 协议 | **自定义长度前缀协议**（4字节网络序长度 + body） |
| 构建 | **CMake** + g++ |
| JSON | **nlohmann/json**（单头文件） |
| 测试 | **Python 3**（socket + struct 发包） |

---

## ✨ 核心特性

### 网络层
- **epoll ET 事件循环**：采用边缘触发模式，while 循环榨干缓冲区直到 EAGAIN。
- **非阻塞 accept**：空的 namespace 里一次性处理完所有待 accept 的连接。
- **IO 与 Worker 解耦**：IO 线程只负责 epoll_wait + recv + 解码，将完整 Request 入队；Worker 线程从队列取任务执行业务，避免耗时操作阻塞网络循环。
- **容量自适应的 Worker 线程池**：根据 CPU 核心数动态决定 Worker 数量（上限 4），队列空时休眠，stop 时优雅退出。

### 协议层
- **长度前缀协议**：`| 4 字节网络序 body_length | 1 字节 version | 1 字节 type | 8 字节 request_id | 变长 payload |`
- **粘包/半包处理**：解码器逐字节游标解析，剩余数据回退到 input_buffer 等待下次事件。
- **NEED_MORE_DATA 语义**：精确区分「数据不够」和「解析完成」，避免半包误处理。
- **协议边界校验**：校验最小固定头（10 字节）和最大 body（`FIXED_BODY_SIZE + MAX_PAYLOAD_SIZE`），恶意超长包直接拒绝并断开。

### 业务层
- **PING / PONG**：基础连通性探测。
- **ECHO**：回显测试。
- **LOG_PUSH / LOG_ACK**：JSON 格式日志上报、校验与落盘。
- **STATS / STATS_RESP**：实时服务器监控统计（请求数、错误数、流量、队列积压等）。

### 可靠性打磨（V4.1）
- **conn_id 世代校验**：每个连接分配独立递增 ID，响应发送前校验 `conn_id` 是否匹配，根除 fd 复用导致的跨连接数据串台。
- **Worker 异常隔离**：双层 try-catch 防护，业务异常不会击穿线程边界导致进程退出。
- **SIGPIPE 防护**：进程级 `signal(SIGPIPE, SIG_IGN)` + 每路 send 使用 `MSG_NOSIGNAL`。
- **输出缓冲区背压**：单连接 output_buffer 上限 2MB，超限直接断开慢客户端防止 OOM。
- **幂等关闭**：`closeConnection()` 先确认 fd 存在再操作，`decrementConnections()` CAS 下溢保护。
- **优雅停机**：`stop()` 先收集 fd 再逐个关闭，确保 epfd 在 closeConnection 之后才关闭。
- **epoll 错误处理**：`EPOLL_CTL_ADD` 失败仅关闭 client fd 不 exit 进程；`modifyConnectionEvents()` 统一封装返回值检查。
- **JSON 字段强校验**：使用 nlohmann/json 解析器代替字符串 find，确保 key 存在且类型正确。

---

## 🚀 快速开始

### 前置条件
- Linux 环境（WSL2 亦可）
- g++ 支持 C++17
- CMake >= 3.10
- Python 3（运行测试脚本）

### 编译

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### 运行服务端

```bash
cd build
./message_server
# 服务端默认监听 8080 端口，Ctrl+C 触发优雅关闭
```

### 运行测试

```bash
# 新开一个终端，进入 scripts/ 目录

# 1. 基础连通性测试（PING / ECHO / LOG_PUSH）
python3 test_client.py

# 2. 半包与粘包模拟
python3 test_client1.py

# 3. 恶意毒包测试（伪造超大 body_length，测试 OOM 熔断）
python3 test_client2.py

# 4. 高并发压测（100 个并发连接各发 100 条消息，共计 10000 条）
python3 stress_test.py
```

---

## 📊 消息协议

### 请求格式（Request）

```
| 4 字节 body_length (网络序) | 1 字节 version | 1 字节 type | 8 字节 request_id (网络序) | N 字节 payload |
```

### 支持的 MessageType

| Type | 请求 | 响应 | 说明 |
|---|---|---|---|
| 1 → 5 | `PING` | `PONG` | 连通性探测 |
| 2 → 6 | `ECHO` | `ECHO_RESP` | 回显（原样返回 payload） |
| 3 → 8 | `LOG_PUSH` | `LOG_ACK` | JSON 日志上报与落盘 |
| 4 → 9 | `STATS` | `STATS_RESP` | 实时服务器监控统计 |
| 其他 → 7 | — | `ERROR_RESP` | 未知消息类型兜底 |

### 错误响应格式

```json
{"status": 400, "message": "具体的错误描述"}
```

### LOG_PUSH 要求的 JSON 格式

```json
{
  "level": "INFO",
  "service": "auth-service",
  "message": "user login success"
}
```

三个字段均为必填且必须为字符串类型，否则返回 `400 invalid log format`。

---

## 📈 监控统计

请求 `STATS`（type=4）可获得 JSON 格式的实时服务器状态：

```json
{
  "total_requests": 1024,
  "total_logs": 512,
  "total_errors": 0,
  "total_recv_bytes": 65536,
  "total_sent_bytes": 65536,
  "active_connections": 5,
  "total_request_queue_backlog": 0,
  "total_response_queue_backlog": 0
}
```

---

## 🔧 构建配置

`CMakeLists.txt` 编译目标：`message_server`

| 选项 | 值 |
|---|---|
| C++ 标准 | C++17 |
| 编译选项 | `-Wall -Wextra -O2` |
| 外部依赖 | `Threads::Threads`（系统多线程库） |

---

## 📝 License

MIT
