// 1、轮询方式
// 周期性读取 WATCHDOG 状态，并在值变化时输出

#include <atomic>      // 原子操作库
#include <chrono>      // 时间库
#include <cstdio>      // C标准输入输出（printf）
#include <iostream>    // C++输入输出流（cin, cout）
#include <string>      // 字符串类
#include <thread>      // 线程库

#include "../../common_include/higplat.h"

std::atomic<bool> g_running{true};

// 轮询函数，定期读取 WATCHDOG 标签的值
void pollingWatchdogStatus()
{
    // 连接本地PLC模拟器，获取连接句柄
    int conngplat = connectgplat("127.0.0.1", 8777);
    if (conngplat < 0)
    {
        std::printf("[Polling] connectgplat failed\n");
        g_running = false;
        return;
    }

    std::printf("[Polling] WATCHDOG 轮询线程已启动...\n");

    int last_hb_value = 0;       // 保存上一次读取的值
    bool has_last_value = false; // 标记是否已经有上一次的值

    while (g_running)
    {
        unsigned int error = 0;
        int hb_value = 0; // 用于存储读取的值
        // 读取 WATCHDOG 标签的值
        bool ok = readb(conngplat, "WATCHDOG", &hb_value, sizeof(hb_value), &error);
        // 只有在读取成功且没有错误时才处理值
        if (ok && error == 0)
        {
            //如果这是第一次读取，或者值发生了变化，则输出当前值
            if (!has_last_value || hb_value != last_hb_value)
            {
                std::printf("[Polling] WATCHDOG 当前值: %d\n", hb_value);
                last_hb_value = hb_value;// 更新上一次的值
                has_last_value = true;// 标记已有值
            }
        }
        else    
        {
            std::printf("[Polling] readb failed, error=%u\n", error);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    disconnectgplat(conngplat);
    std::printf("[Polling] 轮询线程退出\n");
}

int main()
{
    // 创建并启动轮询线程
    std::thread polling_thread(pollingWatchdogStatus);

    std::printf("程序启动。采用轮询方式查看 WATCHDOG，按 'q' 键退出。\n");

    std::string input;
    while (g_running)
    {
        std::cin >> input;
        if (input == "q")
        {
            g_running = false;
        }
    }

    polling_thread.join();

    std::printf("Main thread exit\n");
    return 0;
}
