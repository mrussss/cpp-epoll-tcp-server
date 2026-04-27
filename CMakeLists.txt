# 1. 声明最低要求的 CMake 版本和你的项目名称
cmake_minimum_required(VERSION 3.10)
project(GatewayBase)

# ✅ 第一步：查找系统线程库（Linux 下自动找到 pthread）
find_package(Threads REQUIRED)

# 2. 编译目标
add_executable(server_tcp src/server_tcp.cpp)

# 3. 头文件、编译、链接配置
target_include_directories(server_tcp PUBLIC include)

target_compile_options(server_tcp PRIVATE -O0 -g -fsanitize=address )

# 链接选项上膛：必须同步注入 ASan (严格分离)
 target_link_options(server_tcp PRIVATE -fsanitize=address)

# 第二步：链接 CMake 封装的跨平台线程目标
target_link_libraries(server_tcp PRIVATE Threads::Threads)