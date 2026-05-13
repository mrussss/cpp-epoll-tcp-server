#pragma once
#include <iostream>
#include <thread>
#include <cstdio>
#include <functional>
#include <mutex>

inline std::mutex g_log_mutex;

// 【现代 C++ 升级】使用可变参数模板替换掉老旧的 C 风格宏
template <typename... Args>
inline void LOG_INFO(const char *fmt, Args... args)
{
    std::lock_guard<std::mutex> lock(g_log_mutex);
    printf("[INFO] [Thread %zu] ", std::hash<std::thread::id>{}(std::this_thread::get_id()));
    printf(fmt, args...);
    printf("\n");
}

template <typename... Args>
inline void LOG_ERROR(const char *fmt, Args... args)
{
    std::lock_guard<std::mutex> lock(g_log_mutex);
    fprintf(stderr, "[ERROR] [Thread %zu] ", std::hash<std::thread::id>{}(std::this_thread::get_id()));
    fprintf(stderr, fmt, args...);
    fprintf(stderr, "\n");
}