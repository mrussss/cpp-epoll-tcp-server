#pragma once

struct Connection
{
    int fd;
    std::string input_buffer;
    std::string output_buffer;
    bool closing = false;

    Connection(int fd_) : fd(fd_), input_buffer("") {}
};
