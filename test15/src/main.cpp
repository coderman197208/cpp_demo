// 1、发布订阅方式
// 订阅WATCHDOG状态，当状态发生变化时，接收通知并处理
// WATCHDOG

#include <thread>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <iostream>
#include <string.h>

#include "../../common_include/higplat.h"

// 初始化原子变量为 true
std::atomic<bool> g_running{true};

// --- 线程 1：订阅者 & 看门狗监控器(读取数据) ---
void threadFunction1()
{
    // 连接本地PLC模拟器，获取连接句柄
    int conngplat;
    conngplat = connectgplat("127.0.0.1", 8777);
    unsigned int error;

    // 订阅看门狗标签
    subscribe(conngplat, "WATCHDOG", &error);
    printf("[Thread 1] 监控线程已启动，正在监听信号...\n");

    int last_hb_value = 0;       // 保存上一次读取的值
    bool has_last_value = false; // 标记是否已经有上一次的值
    std::string eventname;
    while (g_running)
    {
        char value[4096] = {0};
        // 使用 200ms 超时，而不是 -1，避免立即返回
        waitpostdata(conngplat, eventname, value, 4096, 200, &error);

        if (error == 110) // 超时，继续循环
        {
            continue;
        }
        else if (error != 0) // 其他错误
        {
            printf("waitpostdata 错误: %d\n", error);
            continue;
        }

        if (error != 0)
        {
            printf("waitpostdata failed, eventname=%s, error=%d\n", eventname.c_str(), error);
            continue;
        }

        if (eventname == "WATCHDOG")
        {

            int hb_value = *(int *)value;
            // 如果这是第一次读取，或者值发生了变化，则输出当前值
            if (!has_last_value || hb_value != last_hb_value)
            {
                printf("[Thread 1] 收到 WATCHDOG 信号，当前值: %d\n", hb_value);
                last_hb_value = hb_value;                                                                     // 更新上一次的值
                has_last_value = readb(conngplat, "WATCHDOG", &last_hb_value, sizeof(last_hb_value), &error); // 接收数据
            }
        }
    }

    printf("Thread 1 exit\n");
}

// --- 线程 2：发布者 & 喂狗执行者 (写入数据) ---
void threadFunction2()
{
    int conngplat = connectgplat("127.0.0.1", 8777);
    unsigned int error;
    int hb_count = 0;

    printf("[Thread 2] 喂狗线程已启动...\n");

    while (g_running)
    {
        hb_count++;
        // 模拟向平台写入自增的心跳值
        writeb(conngplat, "WATCHDOG", &hb_count, sizeof(hb_count), &error);

        // 每 200ms 喂一次狗
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    printf("Thread 2 exit\n");
}

int main()
{
    std::thread t1(threadFunction1);
    // std::thread t2(threadFunction2);

    printf("程序启动。按 'q' 键退出。\n");

    std::string input;
    while (g_running)
    {
        std::cin >> input;
        if (input == "q")
        {
            g_running = false;
        }
    }

    t1.join();
    // t2.join();

    printf("Main thread exit\n");
    return 0;
}