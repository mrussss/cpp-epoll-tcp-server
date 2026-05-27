#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <algorithm>
#include <atomic>
#include <csignal>
#include <cerrno>
#include "net/TcpServer.hpp"
#include "net/SocketUtil.hpp"
#include "protocol/Request.hpp"
#include "protocol/ProtocolCodec.hpp"
#include "common/Logger.hpp"
// =========================================================
// 1. 构造与析构
// =========================================================
TcpServer::TcpServer(int port) : port_(port), listen_fd_(-1), epfd_(-1), running_(false) {}

TcpServer::~TcpServer()
{
    stop();
}

// =========================================================
// 2. 启动服务器核心流程
// =========================================================

TcpServer *TcpServer::instance_ = nullptr;
void TcpServer::static_sigint_handler([[maybe_unused]] int sig)
{
    if (instance_)
    {
        instance_->running_ = false; // 按下 Ctrl+C 时，把 running_ 设为 false
    }
}
void TcpServer::start()
{
    // [你的搬运任务]
    // 1. 调用 initServer() 完成底层的 bind/listen/epoll 挂载
    // 2. 将 running_ 设为 true
    // 3. 把原来 server_tcp.cpp 里 "获取核心数硬件并发" 和 "拉起 Worker 线程池" 的 for 循环代码搬到这里。
    // 4. 最后调用 loop() 进入事件死循环监听。
    instance_ = this;
    if (std::signal(SIGINT, static_sigint_handler) == SIG_ERR)
    {
        std::cerr << "signal registration failed" << std::endl;
    }
    std::cout << "C++ server: press Ctrl+C to exit" << std::endl;
    initServer();
    running_ = true;

    unsigned int hw_cores = std::thread::hardware_concurrency();
    if (hw_cores == 0)
    {
        hw_cores = 4;
    }

    unsigned int worker_count = std::min(hw_cores, 4u);
    LOG_INFO("System detected %u cores, starting %u worker threads...", hw_cores, worker_count);
    for (unsigned i = 0; i < worker_count; ++i)
    {
        workers_.emplace_back([this, worker_id = i]()
                              {
             while (true)
            {
                Request req;
                bool ok = request_queue_.pop(req);
                if (!ok)
                {
                    LOG_INFO("Worker %u received shutdown signal, exiting gracefully.",worker_id);
                     break;
                }
                LOG_INFO("Worker %u successfully parsed Request! fd=%d, type=%d, id=%llu, payload=%s", 
                worker_id,
                req.fd, 
                static_cast<int>(req.type),  
                (unsigned long long)req.request_id,
                req.payload.c_str());
            } });
    }
    loop();
}

void TcpServer::stop()
{
    // [你的搬运任务]
    // 这里放程序退出的清理逻辑。
    // 1. 检查 running_ 状态，如果已经是 false 就直接 return（防止重复关闭）。
    // 2. 设 running_ 为 false。
    // 3. 把原来 main() 最后面 "准备执行全链路大扫除" 下方的代码搬过来：
    //    - 遍历 connections__，调用 close() 关掉存活的 fd。
    //    - 关闭 listen_fd_ 和 epfd__。
    //    - 调用 request_queue_.stop()。
    //    - 遍历 workers_ 并调用 join()。
    if (is_stopped_)
    {
        return;
    }
    is_stopped_ = true;
    running_ = false;
    for (auto &[fd, conn] : connections_)
    {
        LOG_INFO("closing active client fd=%d...", fd);
        close(fd);
    }
    close(listen_fd_);
    close(epfd_);
    request_queue_.stop();
    LOG_INFO("task queue stopped, waiting for all workers to exit");

    for (auto &w : workers_)
    {
        if (w.joinable())
        {
            w.join();
        }
    }

    LOG_INFO("all worker threads exited, system shutdown complete.");
    return;
}

// =========================================================
// 3. 底层网络细节抽象
// =========================================================
void TcpServer::initServer()
{
    // [你的搬运任务]
    // 把原来散落在 main() 最开头的网络底座动作搬过来：
    // 1. socket() 创建 listen_fd_
    // 2. setsockopt() 设置 SO_REUSEADDR 端口复用
    // 3. bind() 绑定端口 (注意：端口用 port_ 成员变量替代原来的 8080)
    // 4. listen()
    // 5. 调用 setNonBlocking(listen_fd_)
    // 6. epoll_create1() 创建 epfd__
    // 7. 声明 epoll_event，把 listen_fd_ 加入 epoll (EPOLLIN | EPOLLET)
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_);

    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);

    if (listen_fd_ == -1)
    {
        perror("socket failed");
        exit(1);
    }

    int opt = 1;
    if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        LOG_INFO("[FATAL] setsockopt(SO_REUSEADDR) failed! Port might be locked.");
        exit(EXIT_FAILURE);
    }
    LOG_INFO("[INFO] SO_REUSEADDR enabled.");

    int bind_return = bind(listen_fd_, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (bind_return == -1)
    {
        perror("bind failed");
        exit(1);
    }
    int listen_retuen = listen(listen_fd_, 10);
    if (listen_retuen == -1)
    {
        perror("listen failed");
        exit(1);
    }
    setNonBlocking(listen_fd_);

    epfd_ = epoll_create1(0);
    if (epfd_ == -1)
    {
        perror("epoll_creael failed");
        exit(EXIT_FAILURE);
    }
    struct epoll_event event;
    memset(&event, 0, sizeof(event));
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = listen_fd_;
    if (epoll_ctl(epfd_, EPOLL_CTL_ADD, listen_fd_, &event) == -1)
    {
        perror("epoll_ctl:listen_fd_ add failed");
        close(epfd_);
        exit(EXIT_FAILURE);
    }
    LOG_INFO("Success: listen_fd_ successfully mounted to epoll instance with EPOLLET.");
}

void TcpServer::loop()
{

    epoll_event events[1024];

    while (running_)
    {
        int nfds = epoll_wait(epfd_, events, 1024, 1000);
        if (nfds == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }
            perror("epoll_wait error");
            break;
        }
        for (int i = 0; i < nfds; i++)
        {
            int fd = events[i].data.fd;
            if (fd == listen_fd_)
            {
                handleAccept();
            }
            else
            {

                handleRead(fd);
            }
        }
    }
}

// =========================================================
// 4. IO 事件处理抽象
// =========================================================
void TcpServer::handleAccept()
{
    // [你的搬运任务]
    // 把原来 if (fd == listen_fd) 里面的 while(true) { accept... } 循环搬过来。
    // 逻辑流：accept 成功 -> setNonBlocking -> 挂载到 epoll -> connections__.insert()。
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int fd;
    while (true)
    {
        fd = accept(listen_fd_, (struct sockaddr *)&client_addr, &addr_len);
        if (fd == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                break;
            }
            else
            {
                perror("accept error");
                break;
            }
        }
        setNonBlocking(fd);
        struct epoll_event event1;
        memset(&event1, 0, sizeof(event1));
        event1.events = EPOLLIN | EPOLLET;
        event1.data.fd = fd;
        if (epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &event1) == -1)
        {
            perror("epoll_ctl:fd add failed");
            close(epfd_);
            exit(EXIT_FAILURE);
        }
        LOG_INFO("New connection: fd=%d", fd);
        connections_.insert({fd, Connection(fd)});
    }
}

void TcpServer::handleRead(int fd)
{
    // [你的搬运任务]
    // 把原来处理普通客户端 fd (else 分支) 里的 while(true) { recv... } 逻辑搬过来。
    // 注意调整：
    // 1. 原来是用 fd，现在函数参数叫 fd。
    // 2. 先从 connections__ 找到对应的 Connection 对象。
    // 3. recv 数据，如果返回 0 (断开) 或者解析出错触发防御熔断，不要再到处写那三行关闭代码了，直接调用 closeConnection(fd)。
    auto it = connections_.find(fd);
    if (it == connections_.end())
        return;
    Connection &conn = it->second;
    while (true)
    {
        char buf[4096];
        memset(buf, 0, sizeof(buf));
        ssize_t bytes_read = recv(fd, buf, sizeof(buf), 0);
        if (bytes_read > 0)
        {
            conn.input_buffer.append(buf, bytes_read);
            LOG_INFO("fd=%d received %zd bytes", fd, bytes_read);
        }
        else if (bytes_read == 0)
        {
            std::vector<Request> out_requests;
            ProtocolCodec::decode(conn.input_buffer, fd, out_requests);
            for (const auto &req : out_requests)
            {
                request_queue_.push(req);
            }
            closeConnection(fd);
            break;
        }
        else if (bytes_read == -1)
        {

            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                std::vector<Request> out_requests;
                DecodeStatus status = ProtocolCodec::decode(conn.input_buffer, fd, out_requests);
                if (status == DecodeStatus::INVALID_LENGTH)
                {
                    // 🚨 触发防御：打印日志，干掉这个连接
                    closeConnection(fd);
                    break;
                }

                // 正常：遍历 out_requests，依次推入任务队列
                for (auto &req : out_requests)
                {
                    request_queue_.push(req);
                }
                break;
            }
            else
            {
                perror("recv error");
                closeConnection(fd);
                break;
            }
        }
    }
}
void TcpServer::closeConnection(int fd)
{
    // [你的搬运任务]
    // 这个动作在原来代码里被复制粘贴了三次，现在统一收拢在这里：
    // 1. epoll_ctl(epfd__, EPOLL_CTL_DEL, fd, nullptr);
    // 2. close(fd);
    // 3. connections__.erase(fd);
    epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
    connections_.erase(fd);
}