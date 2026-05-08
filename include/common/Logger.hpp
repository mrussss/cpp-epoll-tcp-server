#pragma once
#include <iostream>
#include <thread>
#include <cstdio>
#include <functional> // 必须引入这个来使用 std::hash
#include <mutex>

inline std::mutex g_log_mutex;

// 使用哈希函数将 thread::id 转换为唯一的无符号整数 (size_t)
#define LOG_INFO(fmt, ...)                                                                                                \
    do                                                                                                                    \
    {                                                                                                                     \
        std::lock_guard<std::mutex> lock(g_log_mutex);                                                                    \
                                                                                                                          \
        printf("[INFO] [Thread %zu] " fmt "\n", std::hash<std::thread::id>{}(std::this_thread::get_id()), ##__VA_ARGS__); \
    } while (0)

#define LOG_ERROR(fmt, ...)                                                                                                         \
    do                                                                                                                              \
    {                                                                                                                               \
        std::lock_guard<std::mutex> lock(g_log_mutex);                                                                              \
                                                                                                                                    \
        fprintf(stderr, "[ERROR] [Thread %zu] " fmt "\n", std::hash<std::thread::id>{}(std::this_thread::get_id()), ##__VA_ARGS__); \
    } while (0)
