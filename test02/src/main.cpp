// 掌握C++多线程框架程序的开发
// 并可以通过YAML格式配置文件配置成前台或守护进程运行

#include <iostream> // 输入输出流，用于控制台打印
#include <vector>   // 动态数组容器，存储线程对象
#include <thread>   // C++11线程库，创建和管理线程
#include <chrono>   // 时间库，处理时间间隔
#include <unistd.h> // Unix标准头文件，提供fork、getpid等系统调用
#include <signal.h> // 信号处理库，用于处理SIGINT和SIGTERM
#include <fcntl.h>  // 文件控制，提供open函数和O_RDWR等标志

#include <filesystem> // 文件系统库，用于创建目录
#include "spdlog/spdlog.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <pqxx/pqxx> // libpqxx数据库
#include <sstream>  // 字符串流，用于格式化
#include <sys/stat.h>  // 添加这个头文件

//#include "CConfig.h" // 你的配置管理类
#include "iniconfig.h" // 你的配置管理类

bool bExit = false;

// 全局日志记录器
std::shared_ptr<spdlog::logger> g_logger;

// 信号处理函数
void signalHandler(int signum)
{
    if (signum == SIGINT)
    {
        if (g_logger)
        {
            g_logger->warn("收到 SIGINT 信号 (Ctrl+C)，准备退出...");
        }
        std::cout << "\n收到 SIGINT 信号，程序即将退出..." << std::endl;
    }
    else if (signum == SIGTERM)
    {
        if (g_logger)
        {
            g_logger->warn("收到 SIGTERM 信号 (kill命令)，准备退出...");
        }
        std::cout << "\n收到 SIGTERM 信号，程序即将退出..." << std::endl;
    }
    
    bExit = true;

}

// 辅助函数：日志级别字符串转换为spdlog级别
spdlog::level::level_enum stringToLevel(const std::string &level_str)
{
    if (level_str == "trace")
        return spdlog::level::trace;
    if (level_str == "debug")
        return spdlog::level::debug;
    if (level_str == "info")
        return spdlog::level::info;
    if (level_str == "warn")
        return spdlog::level::warn;
    if (level_str == "error")
        return spdlog::level::err;
    if (level_str == "critical")
        return spdlog::level::critical;
    if (level_str == "off")
        return spdlog::level::off;
    return spdlog::level::info; // 默认级别
}

// 辅助函数：获取当前线程ID的字符串表示
std::string get_thread_id_str()
{
    std::stringstream ss;
    ss << std::this_thread::get_id();
    return ss.str();
}

// 辅助函数：获取文件大小
size_t get_file_size(const std::string& filename) {
    struct stat stat_buf;
    int rc = stat(filename.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : 0;
}


// 初始化日志系统函数
bool initLogging(CConfig &config)
{
    try
    {
        // 1. 获取日志配置参数
        bool log_console = config.GetBoolDefault("log_console", true);
        std::string level_str = config.GetStringDefault("level", "info");
        std::string pattern = config.GetStringDefault("pattern",
                                                      "[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
        std::string filename = config.GetStringDefault("filename", "logs/app.log");
        bool immediate_flush = config.GetBoolDefault("immediate_flush", true);
        int max_size = config.GetIntDefault("max_size", 1); 
        int max_files = config.GetIntDefault("max_files", 3);

        // 添加调试信息
        std::cout << "当前工作目录: " << std::filesystem::current_path() << std::endl;
        std::cout << "日志文件名: " << filename << std::endl;

        // 2. 确保日志目录存在
        std::filesystem::path file_path(filename);
        std::cout << "日志文件完整路径: " << std::filesystem::absolute(file_path) << std::endl;
       
        if (file_path.has_parent_path()) {
            std::cout << "创建日志目录: " << file_path.parent_path() << std::endl;
            bool created = std::filesystem::create_directories(file_path.parent_path());
            std::cout << "目录创建" << (created ? "成功" : "失败或已存在") << std::endl;
        }

        // 3. 创建sinks
        std::vector<spdlog::sink_ptr> sinks;

        // 控制台sink
        if (log_console)
        {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_pattern(pattern);
            sinks.push_back(console_sink);
        }

        // 创建按大小滚动的日志文件
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            filename, max_size * 1024, max_files);
        file_sink->set_pattern(pattern);
        sinks.push_back(file_sink);

        std::cout << "日志文件: " << filename
                  << " (最大 " << max_size << " KB, 保留 " << max_files << " 个文件)" << std::endl;

        // 4. 创建logger
        g_logger = std::make_shared<spdlog::logger>("multi_thread_logger",
                                                    sinks.begin(), sinks.end());

        // 5. 设置日志级别
        g_logger->set_level(stringToLevel(level_str));

        // 6. 设置立即刷新
        if (immediate_flush)
        {
            g_logger->flush_on(stringToLevel(level_str));
        }

        // 7. 设置为全局默认日志器（可选）
        spdlog::set_default_logger(g_logger);

        // 记录初始化成功的日志
        g_logger->info("日志系统初始化成功!");
        g_logger->info("日志级别: {}", level_str);
        g_logger->info("日志文件: {}", filename);
        g_logger->info("最大文件大小: {} MB", max_size);
        g_logger->info("最大文件数: {}", max_files);

        return true;
    }
    catch (const spdlog::spdlog_ex &e)
    {
        std::cerr << "spdlog异常: " << e.what() << std::endl;
        return false;
    }
    catch (const std::exception &e)
    {
        std::cerr << "标准异常: " << e.what() << std::endl;
        return false;
    }
    catch (...)
    {
        std::cerr << "未知异常" << std::endl;
        return false;
    }
}

// 转换为守护进程的函数
void becomeDaemon()
{
    // 保存当前工作目录，确保日志文件路径正确
    std::string original_cwd = std::filesystem::current_path().string();
    
    g_logger->info("开始转换为守护进程...");
    g_logger->info("原始工作目录: {}", original_cwd);

    pid_t pid = fork(); // 1. 创建子进程
    if (pid < 0)        // pid < 0: 创建失败
    {
        std::cout << "创建子进程失败！" << std::endl,
        exit(EXIT_FAILURE);
    }
    if (pid > 0) // pid > 0: 当前是父进程，pid是子进程ID
    {
        std::cout << "父进程退出，子进程继续运行（PID: " << pid << "）" << std::endl,
        exit(EXIT_SUCCESS); // 2. 父进程退出，让子进程继续运行
    }

    setsid(); // 3. 创建新的会话，使进程脱离终端控制，在新会话中成为领导进程
    std::cout << "新会话创建成功，进程已脱离终端控制。" << std::endl;

    // 重定向标准输入输出
    int null_fd = open("/dev/null", O_RDWR);
    if (null_fd != -1)
    {
        dup2(null_fd, STDIN_FILENO);
        dup2(null_fd, STDOUT_FILENO);
        dup2(null_fd, STDERR_FILENO);
        close(null_fd);
    }
    std::cout << "标准输入输出已关闭。" << std::endl; // 不会显示

    // 5. 保持在原工作目录，而不是切换到/tmp，这样可以确保相对路径的日志文件能够正确访问
    // chdir("/tmp");  // 注释掉：避免影响日志文件路径
    chdir(original_cwd.c_str());  // 切换回原工作目录

    g_logger->info("已转换为守护进程运行（PID: {}）", getpid());
    g_logger->info("工作目录: {}", std::filesystem::current_path().string());
}

// 循环创建线程的线程任务
void threadTask(int id)
{
    // 获取当前线程ID的字符串
    std::string thread_id_str = get_thread_id_str();

    // 记录线程开始（只在日志中记录，不在控制台输出）
    g_logger->info("线程 {} 开始执行 (PID: {}, TID: {})",
                   id, getpid(), thread_id_str);

    // int loop_count = 0;
    // while(!bExit)
    // {
    //     g_logger->info("线程 {} 正在运行执行第 {} 次任务 (PID: {}, TID: {})",
    //                id, ++loop_count, getpid(), thread_id_str);
    //     std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // }

    g_logger->info("线程 {} 正在运行执行完毕", id);    

    // 打印线程执行信息
    std::cout << "线程 " << id << " 执行完毕 (PID: " << getpid() << ", TID: " << thread_id_str << ")" << std::endl;
}

// 数据库线程任务：在独立连接中执行简单的读写操作
void dbThreadTask(const std::string &conn_str)
{
    std::string thread_id_str = get_thread_id_str();
    g_logger->info("数据库线程 {} 启动", thread_id_str);

    try
    {
        pqxx::connection conn(conn_str);
        if (!conn.is_open())
        {
            g_logger->error("数据库连接未打开");
            return;
        }else
        {
            g_logger->info("数据库连接成功 (数据库: {})", conn.dbname());
        }

        // 执行查询操作
        {
            // 示例查询：按 user_id 获取用户信息（避免与函数参数 id 冲突）
            int user_id = 1; 
            const std::string sql = "SELECT * FROM users WHERE id = $1;";
            pqxx::nontransaction txn(conn);
            pqxx::result result = txn.exec(sql, pqxx::params{user_id});

            if (!result.empty())
            {
                const auto &r = result[0];
                try {
                    int rid = r["id"].as<int>();
                    std::string username = r["username"].as<std::string>();
                    std::string full_name = r["full_name"].as<std::string>();
                    std::string email = r["email"].as<std::string>();
                    std::string phone = r["phone"].as<std::string>();

                    // std::cout << "id = " << rid << std::endl;
                    // std::cout << "username = " << username << std::endl;
                    // std::cout << "full_name = " << full_name << std::endl;
                    // std::cout << "email = " << email << std::endl;
                    // std::cout << "phone = " << phone << std::endl;
                    g_logger->info("查询结果 - id: {}, username: {}, full_name: {}, email: {}, phone: {}",
                                   rid, username, full_name, email, phone);
                }
                catch (const std::exception &e)
                {
                    g_logger->warn("解析查询结果失败: {}", e.what());
                }
            }
            else
            {
                std::cout << "No data found for ID = " << user_id << std::endl;
                g_logger->info("No rows for user id {}", user_id);
            }

        }

        // // 执行添加操作
        // {
        //     const std::string insert_sql = 
        //         "INSERT INTO users (username, full_name, email, phone) "
        //         "VALUES ($1, $2, $3, $4) RETURNING id;";
        //     pqxx::work txn(conn);

        //     std::string username = "lzy";
        //     std::string full_name = "刘昭媛";
        //     std::string email = "liuzhaoyuan@baowu.ren"; 
        //     std::string phone = "15215607035";
        //     pqxx::result insert_result = txn.exec(
        //         insert_sql, pqxx::params{username, full_name, email, phone});
        //     txn.commit();
        //     if (!insert_result.empty())
        //     {
        //         int new_id = insert_result[0]["id"].as<int>();
        //         g_logger->info("成功插入新用户，ID: {}", new_id);
        //     }
        //     else
        //     {
        //         g_logger->warn("插入新用户失败，未返回ID");
        //     }
        // }
        
        // // 执行添加操作
        // {
        //     const std::string insert_sql = 
        //         "INSERT INTO users (username, full_name, email, phone) "
        //         "VALUES ($1, $2, $3, $4) RETURNING id;";
        //     pqxx::work txn(conn);

        //     std::string username = "zhangsan";
        //     std::string full_name = "张三";
        //     std::string email = "zhangsan@example.com"; 
        //     std::string phone = "13800138000";
        //     pqxx::result insert_result = txn.exec(
        //         insert_sql, pqxx::params{username, full_name, email, phone});
        //     txn.commit();
        //     if (!insert_result.empty())
        //     {
        //         int new_id = insert_result[0]["id"].as<int>();
        //         g_logger->info("成功插入新用户，ID: {}", new_id);
        //     }
        //     else
        //     {
        //         g_logger->warn("插入新用户失败，未返回ID");
        //     }
        // }

        // // 更新数据操作
        // {
        //     const std::string update_sql = 
        //         "UPDATE users SET username = $1 WHERE id = $2;";
        //     pqxx::work txn(conn);
        //     int user_id = 6; // 假设要更新ID为6的用户
        //     std::string new_username = "zs";
        //     pqxx::result insert_result = txn.exec(
        //         update_sql, pqxx::params{new_username, user_id});
        //     txn.commit();
        //     g_logger->info("成功更新用户 ID: {} 的用户名为 {}", user_id, new_username);
        // }

    }
    catch (const std::exception &e)
    {
        g_logger->error("数据库线程异常: {}", e.what());
    }

    g_logger->info("数据库线程 {} 结束");
}

int main()
{
    // 读取配置
    auto &config = CConfig::GetInstance();

    // 使用绝对路径（相对于可执行文件）
    std::string configPath = "../config/test02.ini";

    // 加载配置文件
    if (!config.Load(configPath))
    {
        std::cerr << "警告: " << config.GetLastError() << std::endl;
        std::cerr << "将使用默认配置运行" << std::endl;
    }

    // 初始化日志系统
    std::cout << "正在初始化日志系统..." << std::endl;
    if (!initLogging(config))
    {
        std::cerr << "日志系统初始化失败，程序退出" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "日志系统初始化成功" << std::endl;

    // 直接获取配置值（使用默认值保证安全）
    int threadCount = config.GetIntDefault("thread_count", 2);     // 默认2个线程
    bool daemonMode = config.GetBoolDefault("daemon_mode", false); // 默认前台运行

    std::cout << "配置信息:" << std::endl;
    std::cout << "  - 线程数: " << threadCount << std::endl;
    std::cout << "  - 运行模式: " << (daemonMode ? "守护进程" : "前台进程") << std::endl;

    // 注册信号处理器
    signal(SIGINT, signalHandler);   // 处理 Ctrl+C
    signal(SIGTERM, signalHandler);  // 处理 kill 命令
    g_logger->info("信号处理器已注册 (SIGINT, SIGTERM)");

    g_logger->info("========== 应用程序启动 ==========");
    g_logger->info("程序启动 (PID: {})", getpid());
    g_logger->info("线程数: {}", threadCount);
    g_logger->info("运行模式: {}", daemonMode ? "守护进程" : "前台进程");

    // 判断是否转为守护进程
    if (daemonMode)
    {
        std::cout << "正在转换为守护进程..." << std::endl;
        becomeDaemon();
    }

    // 创建并运行常规工作线程 + 可选数据库线程（并行启动）
    // 创建线程容器（主线程创建并在末尾 join）
    std::vector<std::thread> threads; 

    // 创建常规工作线程，ID = 0 .. threadCount-1
    for (int i = 0; i < threadCount; ++i)
    {
        //emplace_back() 方法在容器末尾直接构造线程对象，无需先创建再复制
        threads.emplace_back(threadTask, i);
    }
    // 数据库连接信息读取
    std::string dbname = config.GetStringDefault("dbname", "");
    std::string dbuser = config.GetStringDefault("user", "");
    std::string dbpass = config.GetStringDefault("password", "");
    std::string hostaddr = config.GetStringDefault("hostaddr", "127.0.0.1");
    int dbport = config.GetIntDefault("port", 5432);
    
    if (!dbname.empty() && !dbuser.empty() && !dbpass.empty() && !hostaddr.empty() && dbport > 0)
    {
        // 构建连接字符串：数据库连接信息
        std::stringstream conn_ss;
        conn_ss << " dbname=" << dbname
                << " user=" << dbuser
                << " password='" << dbpass << "'"
                << " hostaddr=" << hostaddr
                << " port=" << dbport;

        std::string db_conn_str = conn_ss.str();
        g_logger->info("将使用数据库连接 - dbname: {}, hostaddr: {}, port: {}", dbname, hostaddr, dbport);

        // 数据库线程
        threads.emplace_back(dbThreadTask, db_conn_str);
    }
    else
    {
        g_logger->warn("数据库连接信息不完整，跳过数据库线程创建");
    }

    // 等待所有线程完成（在主线程中 join）
    for (auto &t : threads)
    {
        if (t.joinable())
            t.join();
    }

    // 记录和显示最终状态
    g_logger->info("所有线程执行完毕");
    g_logger->info("========== 应用程序结束 ==========");

    std::cout << "\n所有线程执行完毕，程序即将退出。" << std::endl;

    // 确保所有日志写入文件
    g_logger->flush();
    std::cout << "程序正常退出" << std::endl;

    // 清理日志系统
    spdlog::shutdown();

    return 0;
}