#pragma once

#include <atomic>
#include <vector>
#include <thread>
#include <unordered_map>
#include <string>
#include <sys/epoll.h>
#include "net/Connection.hpp"
#include "protocol/Request.hpp"
#include "concurrent/BlockQueue.hpp"

class TcpServer
{
public:
    TcpServer(int port);
    ~TcpServer();

    void start();
    void stop();
    static void static_sigint_handler(int sig);

private:
    void initServer();
    void loop();
    void handleAccept();
    void handleRead(int fd);
    void closeConnection(int fd);

    int port_;
    int listen_fd_;
    int epfd_;
    std::atomic<bool> running_{false};
    std::atomic<bool> is_stopped_{false};
    static TcpServer *instance_;

    BlockQueue<Request> task_queue_;
    std::unordered_map<int, Connection> connections_;
    std::vector<std::thread> workers_;
};