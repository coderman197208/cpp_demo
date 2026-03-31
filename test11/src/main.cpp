// 目标实现：
// 通过配置文件设置日志输出，可以选择后台文件输出和控制台输出
// - 默认：后台日志文件输出
// - 配置文件里设置开关打开：控制台和后台日志文件一起输出

#include <iostream>
#include <filesystem>
#include <vector>
#include <thread>
#include <unistd.h>   // fork, getpid
#include <signal.h>   // signal handling
#include <fcntl.h>    // open, O_RDWR
#include <sys/stat.h> // umask

// 配置类头文件
#include "CConfig.h"
#include "logging.h" // 公共日志库封装

bool bExit = false; //  程序退出

// 信号处理函数
void signalHandler(int signum)
{
    if (signum == SIGINT)
    {
        getLogger()->warn("收到 SIGINT 信号 (Ctrl+C)，准备退出...");
    }
    else if (signum == SIGTERM)
    {
        getLogger()->warn("收到 SIGTERM 信号 (kill命令)，准备退出...");
    }

    bExit = true;
}

// --- 核心：真守护进程转换函数(标准流程) ---
void becomeDaemon(bool keep_console = false)
{
    // 1. 保存原始工作目录（防止切换到 / 后找不到配置文件和日志路径）
    std::string original_cwd = std::filesystem::current_path().string();

    getLogger()->info("开始转换为守护进程...");

    // 2. 第一次 Fork
    pid_t pid = fork();
    if (pid < 0)
        exit(EXIT_FAILURE);
    if (pid > 0)
        exit(EXIT_SUCCESS); // 父进程退出

    // 3. 创建新会话，脱离终端
    if (setsid() < 0)
        exit(EXIT_FAILURE);

    // 4. 第二次 Fork（可选，防止进程重新取得终端控制权）
    pid = fork();
    if (pid < 0)
        exit(EXIT_FAILURE);
    if (pid > 0)
        exit(EXIT_SUCCESS);

    // 5. 设置文件权限掩码
    umask(0);

    // 6. 切换回原目录（或者根据需要切换到 /）
    chdir(original_cwd.c_str());

    // 7. 关闭并重定向标准流
    int null_fd = open("/dev/null", O_RDWR);
    if (null_fd != -1)
    {
        // 永远关闭 stdin（守护进程不需要输入）
        dup2(null_fd, STDIN_FILENO);

        if (!keep_console)
        {
            // 纯后台模式：重定向到黑洞
            dup2(null_fd, STDOUT_FILENO);
            dup2(null_fd, STDERR_FILENO);
            getLogger()->info("纯后台模式运行，控制台输出已关闭");
        }
        else
        {
            getLogger()->info("调试模式运行，控制台输出已保留");
        }

        close(null_fd);
    }

    getLogger()->info("已转换为守护进程运行（PID: {}）", getpid());

    // 重新注册信号处理器
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
}

int main()
{
    // 1. 加载配置文件
    auto &config = CConfig::GetInstance();
    std::string configPath = "../config/spdlog.yaml"; // 使用绝对路径（相对于可执行文件）
    if (!config.Load(configPath))
    {
        std::cerr << "警告: 配置文件加载失败 (" << config.GetLastError()
                  << ")，将使用代码默认参数" << std::endl;
    }

    // 2. 初始化日志系统
    LogConfig logCfg;
    logCfg.log_console     = config.GetBoolDefault("log_console", false);
    logCfg.level           = config.GetStringDefault("level", logCfg.level);
    logCfg.pattern         = config.GetStringDefault("pattern", logCfg.pattern);
    logCfg.filename        = config.GetStringDefault("filename", "logs/myservice.log");
    logCfg.immediate_flush = config.GetBoolDefault("immediate_flush", logCfg.immediate_flush);
    logCfg.max_size_mb     = config.GetIntDefault("max_size", logCfg.max_size_mb);
    logCfg.max_files       = config.GetIntDefault("max_files", logCfg.max_files);
    logCfg.logger_name     = "daemon_logger";

    if (!initLogging(logCfg))
    {
        return EXIT_FAILURE;
    }

    // 3. 根据配置决定是否进入"守护进程"模式
    bool daemonMode = config.GetBoolDefault("daemonMode", false);   // 默认前台运行
    bool log_console = config.GetBoolDefault("log_console", false); // 默认关闭控制台
    // 注册信号处理器
    signal(SIGINT, signalHandler);  // 处理 Ctrl+C
    signal(SIGTERM, signalHandler); // 处理 kill 命令
    getLogger()->info("信号处理器已注册 (SIGINT, SIGTERM)");

    // 判断前台还是守护进程模式
    if (daemonMode)
    {
        getLogger()->info("检测到守护进程配置，准备脱离终端...");
        becomeDaemon(log_console);
    }
    else
    {
        getLogger()->info("未配置守护进程模式，继续在前台运行...");
    }

    // 4. 正式业务逻辑
    getLogger()->info("进程初始化完成，当前 PID: {}, 工作目录: {}",
                   getpid(), std::filesystem::current_path().string());

    while (!bExit)
    {
        getLogger()->info("服务运行中...");

        if (daemonMode && log_console)
        {
            std::cerr << "[调试] 守护进程直接写 stderr" << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::seconds(10));
    }

    getLogger()->info("程序正常退出");

    // 清理日志系统
    shutdownLogging();

    return 0;
}