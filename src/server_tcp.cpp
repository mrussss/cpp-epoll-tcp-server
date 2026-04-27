#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include "Logger.hpp"
#include <arpa/inet.h>
#include <iostream>
#include <fcntl.h>
#include <sys/epoll.h>
#include <cerrno>
#include <cstring>
#include <csignal>
#include <unordered_map>
#include <thread>
#include <vector>
#include <algorithm>
#include "Connection.hpp"
#include "BlockQueue.hpp"

volatile sig_atomic_t g_running = 1;
void sigint_handler(int sig)
{
    g_running = 0;
}
void setNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);
}

int main()
{
    BlockQueue<std::string> task_queue;
    if (std::signal(SIGINT, sigint_handler) == SIG_ERR)
    {
        std::cerr << "signal 注册失败" << std::endl;
    }
    std::cout << "C++ 程序：按 Ctrl+C 退出" << std::endl;
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080);

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (listen_fd == -1)
    {
        perror("socket failed");
        exit(1);
    }

    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        LOG_INFO("[错误] 致命:setsockopt(SO_REUSEADDR) 失败！端口可能仍处于锁定状态。");
        exit(EXIT_FAILURE);
    }
    LOG_INFO("[信息] SO_REUSEADDR 特权授予成功，端口复用已开启。");

    int bind_return = bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (bind_return == -1)
    {
        perror("bind failed");
        exit(1);
    }
    int listen_retuen = listen(listen_fd, 10);
    if (listen_retuen == -1)
    {
        perror("listen failed");
        exit(1);
    }
    setNonBlocking(listen_fd);

    int epfd = epoll_create1(0);
    if (epfd == -1)
    {
        perror("epoll_creael failed");
        exit(EXIT_FAILURE);
    }
    struct epoll_event event = {0};
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = listen_fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &event) == -1)
    {
        perror("epoll_ctl:listen_fd add failed");
        close(epfd);
        exit(EXIT_FAILURE);
    }
    LOG_INFO("Success: listen_fd successfully mounted to epoll instance with EPOLLET.");
    epoll_event events[1024];
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int conn_fd = 0;

    std::unordered_map<int, Connection> connections;

    unsigned int hw_cores = std::thread::hardware_concurrency();
    if (hw_cores == 0)
    {
        hw_cores = 4;
    }

    unsigned int worker_count = std::min(hw_cores, 4u);
    LOG_INFO("系统检测到 %u 个核心，准备拉起 %u 个 Worker 线程...", hw_cores, worker_count);
    std::vector<std::thread> workers;
    for (unsigned i = 0; i < worker_count; ++i)
    {
        workers.emplace_back([&task_queue, worker_id = i]()
                             {
                                 while (true)
                                 {
                                     std::string item;
                                     bool ok = task_queue.pop(item);
                                     if (!ok)
                                     {
                                        LOG_INFO("Worker %u 收到队列关闭信号，优雅退出。",worker_id);
                                         break;
                                     }
                                     LOG_INFO("Worker %u 收到解包数据: %s",worker_id, item.c_str());
                                 } });
    }

    while (g_running)
    {
        int nfds = epoll_wait(epfd, events, 1024, 1000);
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
            int curr_fd = events[i].data.fd;
            if (curr_fd == listen_fd)
            {
                while (true)
                {
                    conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &addr_len);
                    if (conn_fd == -1)
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
                    setNonBlocking(conn_fd);
                    struct epoll_event event1 = {0};
                    event1.events = EPOLLIN | EPOLLET;
                    event1.data.fd = conn_fd;
                    if (epoll_ctl(epfd, EPOLL_CTL_ADD, conn_fd, &event1) == -1)
                    {
                        perror("epoll_ctl:conn_fd add failed");
                        close(epfd);
                        exit(EXIT_FAILURE);
                    }
                    LOG_INFO("New connection: fd=%d", conn_fd);
                    connections.insert({conn_fd, Connection(conn_fd, &task_queue)});
                }
            }
            else
            {
                auto it = connections.find(curr_fd);
                if (it == connections.end())
                    continue;
                Connection &conn = it->second;
                while (true)
                {
                    char buf[4096];
                    memset(buf, 0, sizeof(buf));
                    ssize_t bytes_read = recv(curr_fd, buf, sizeof(buf), 0);
                    if (bytes_read > 0)
                    {
                        conn.input_buffer.append(buf, bytes_read);
                        LOG_INFO("fd=%d received %zd bytes", curr_fd, bytes_read);
                    }
                    else if (bytes_read == 0)
                    {
                        conn.parse();
                        LOG_INFO("Client disconnected: fd=%d, cleaning up...", curr_fd);
                        epoll_ctl(epfd, EPOLL_CTL_DEL, curr_fd, nullptr);
                        close(curr_fd);
                        connections.erase(curr_fd);
                        break;
                    }
                    else
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                        {
                            int ret = conn.parse();
                            if (ret == -1)
                            {
                                LOG_ERROR("fd=%d 触发防御熔断，物理拔线！", curr_fd);
                                epoll_ctl(epfd, EPOLL_CTL_DEL, curr_fd, nullptr);
                                close(curr_fd);
                                connections.erase(curr_fd);
                            }
                            break;
                        }
                        else
                        {
                            perror("recv error");
                            epoll_ctl(epfd, EPOLL_CTL_DEL, curr_fd, nullptr);
                            close(curr_fd);
                            connections.erase(curr_fd);
                            break;
                        }
                    }
                }
            }
        }
    }
    LOG_INFO("捕获中止信号，准备执行全链路大扫除...");
    for (auto &[fd, conn] : connections)
    {
        LOG_INFO("正在关闭存活的客户端 fd=%d...", fd);
        close(fd);
    }
    close(listen_fd);
    close(epfd);
    task_queue.stop();
    LOG_INFO("触发内部队列 stop,等待所有Worker撤退");

    for (auto &w : workers)
    {
        if (w.joinable())
        {
            w.join();
        }
    }

    LOG_INFO("Worker军团已彻底安全销毁,系统完美退出。");
    return 0;
}