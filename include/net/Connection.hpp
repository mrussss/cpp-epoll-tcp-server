#pragma once

struct Connection
{
    int fd;
    std::string input_buffer;

    Connection(int fd_) : fd(fd_), input_buffer("") {}
};
