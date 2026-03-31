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
#include <pqxx/pqxx> // libpqxx数据库
#include <sstream>  // 字符串流，用于格式化

#include "CConfig.h" // 你的配置管理类
#include "logging.h" // 公共日志库封装

bool bExit = false;

// 信号处理函数
void signalHandler(int signum)
{
    if (signum == SIGINT)
    {
        if (getLogger())
        {
            getLogger()->warn("收到 SIGINT 信号 (Ctrl+C)，准备退出...");
        }
        std::cout << "\n收到 SIGINT 信号，程序即将退出..." << std::endl;
    }
    else if (signum == SIGTERM)
    {
        if (getLogger())
        {
            getLogger()->warn("收到 SIGTERM 信号 (kill命令)，准备退出...");
        }
        std::cout << "\n收到 SIGTERM 信号，程序即将退出..." << std::endl;
    }

    bExit = true;

}

// 辅助函数：获取当前线程ID的字符串表示
std::string get_thread_id_str()
{
    std::stringstream ss;
    ss << std::this_thread::get_id();
    return ss.str();
}

// 转换为守护进程的函数
void becomeDaemon()
{
    // 保存当前工作目录，确保日志文件路径正确
    std::string original_cwd = std::filesystem::current_path().string();
    
    getLogger()->info("开始转换为守护进程...");
    getLogger()->info("原始工作目录: {}", original_cwd);

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

    getLogger()->info("已转换为守护进程运行（PID: {}）", getpid());
    getLogger()->info("工作目录: {}", std::filesystem::current_path().string());
}

// 循环创建线程的线程任务
void threadTask(int id)
{
    // 获取当前线程ID的字符串
    std::string thread_id_str = get_thread_id_str();

    // 记录线程开始（只在日志中记录，不在控制台输出）
    getLogger()->info("线程 {} 开始执行 (PID: {}, TID: {})",
                   id, getpid(), thread_id_str);

    // int loop_count = 0;
    // while(!bExit)
    // {
    //     getLogger()->info("线程 {} 正在运行执行第 {} 次任务 (PID: {}, TID: {})",
    //                id, ++loop_count, getpid(), thread_id_str);
    //     std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // }

    getLogger()->info("线程 {} 正在运行执行完毕", id);    

    // 打印线程执行信息
    std::cout << "线程 " << id << " 执行完毕 (PID: " << getpid() << ", TID: " << thread_id_str << ")" << std::endl;
}

// 数据库线程任务：在独立连接中执行简单的读写操作
void dbThreadTask(const std::string &conn_str)
{
    std::string thread_id_str = get_thread_id_str();
    getLogger()->info("数据库线程 {} 启动", thread_id_str);

    try
    {
        pqxx::connection conn(conn_str);
        if (!conn.is_open())
        {
            getLogger()->error("数据库连接未打开");
            return;
        }else
        {
            getLogger()->info("数据库连接成功 (数据库: {})", conn.dbname());
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
                    getLogger()->info("查询结果 - id: {}, username: {}, full_name: {}, email: {}, phone: {}",
                                   rid, username, full_name, email, phone);
                }
                catch (const std::exception &e)
                {
                    getLogger()->warn("解析查询结果失败: {}", e.what());
                }
            }
            else
            {
                std::cout << "No data found for ID = " << user_id << std::endl;
                getLogger()->info("No rows for user id {}", user_id);
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
        //         getLogger()->info("成功插入新用户，ID: {}", new_id);
        //     }
        //     else
        //     {
        //         getLogger()->warn("插入新用户失败，未返回ID");
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
        //         getLogger()->info("成功插入新用户，ID: {}", new_id);
        //     }
        //     else
        //     {
        //         getLogger()->warn("插入新用户失败，未返回ID");
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
        //     getLogger()->info("成功更新用户 ID: {} 的用户名为 {}", user_id, new_username);
        // }

    }
    catch (const std::exception &e)
    {
        getLogger()->error("数据库线程异常: {}", e.what());
    }

    getLogger()->info("数据库线程 {} 结束");
}

int main()
{
    // 读取配置
    auto &config = CConfig::GetInstance();

    // 使用绝对路径（相对于可执行文件）
    std::string configPath = "../config/subproject2.yaml";

    // 加载配置文件
    if (!config.Load(configPath))
    {
        std::cerr << "警告: " << config.GetLastError() << std::endl;
        std::cerr << "将使用默认配置运行" << std::endl;
    }

    // 初始化日志系统
    LogConfig logCfg;
    logCfg.log_console     = config.GetBoolDefault("log_console", logCfg.log_console);
    logCfg.level           = config.GetStringDefault("level", logCfg.level);
    logCfg.pattern         = config.GetStringDefault("pattern", logCfg.pattern);
    logCfg.filename        = config.GetStringDefault("filename", logCfg.filename);
    logCfg.immediate_flush = config.GetBoolDefault("immediate_flush", logCfg.immediate_flush);
    logCfg.max_size_mb     = config.GetIntDefault("max_size", logCfg.max_size_mb);
    logCfg.max_files       = config.GetIntDefault("max_files", logCfg.max_files);
    logCfg.logger_name     = "multi_thread_logger";

    if (!initLogging(logCfg))
    {
        std::cerr << "日志系统初始化失败，程序退出" << std::endl;
        return EXIT_FAILURE;
    }

    // 直接获取配置值（使用默认值保证安全）
    int threadCount = config.GetIntDefault("thread_count", 2);     // 默认2个线程
    bool daemonMode = config.GetBoolDefault("daemon_mode", false); // 默认前台运行

    std::cout << "配置信息:" << std::endl;
    std::cout << "  - 线程数: " << threadCount << std::endl;
    std::cout << "  - 运行模式: " << (daemonMode ? "守护进程" : "前台进程") << std::endl;

    // 注册信号处理器
    signal(SIGINT, signalHandler);   // 处理 Ctrl+C
    signal(SIGTERM, signalHandler);  // 处理 kill 命令
    getLogger()->info("信号处理器已注册 (SIGINT, SIGTERM)");

    getLogger()->info("========== 应用程序启动 ==========");
    getLogger()->info("程序启动 (PID: {})", getpid());
    getLogger()->info("线程数: {}", threadCount);
    getLogger()->info("运行模式: {}", daemonMode ? "守护进程" : "前台进程");

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
        getLogger()->info("将使用数据库连接 - dbname: {}, hostaddr: {}, port: {}", dbname, hostaddr, dbport);

        // 数据库线程
        threads.emplace_back(dbThreadTask, db_conn_str);
    }
    else
    {
        getLogger()->warn("数据库连接信息不完整，跳过数据库线程创建");
    }

    // 等待所有线程完成（在主线程中 join）
    for (auto &t : threads)
    {
        if (t.joinable())
            t.join();
    }

    // 记录和显示最终状态
    getLogger()->info("所有线程执行完毕");
    getLogger()->info("========== 应用程序结束 ==========");

    std::cout << "\n所有线程执行完毕，程序即将退出。" << std::endl;

    // 清理日志系统
    shutdownLogging();
    std::cout << "程序正常退出" << std::endl;

    return 0;
}