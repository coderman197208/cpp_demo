#pragma once

/*
 * logging.h — spdlog 日志系统封装（单头文件）
 *
 * 提供统一的日志初始化、全局访问和关闭接口，
 * 通过 LogConfig 结构体传参，不依赖任何特定配置类。
 *
 * 用法：
 *   LogConfig cfg;
 *   cfg.filename = "logs/myapp.log";
 *   cfg.level    = "debug";
 *   initLogging(cfg);
 *
 *   getLogger()->info("hello {}", "world");
 *   // 或: spdlog::info("hello {}", "world");
 *
 *   shutdownLogging();
 */

#include <memory>
#include <string>
#include <vector>
#include <filesystem>
#include <iostream>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

// ============================================================
//  日志配置结构体
// ============================================================
struct LogConfig
{
    bool        log_console     = true;                                 // 是否同时输出到控制台
    std::string level           = "info";                               // 日志级别
    std::string pattern         = "[%Y-%m-%d %H:%M:%S.%e] [%l] %v";    // 日志格式
    std::string filename        = "logs/app.log";                       // 日志文件路径
    bool        immediate_flush = true;                                 // 是否立即刷新
    int         max_size_mb     = 10;                                   // 单个日志文件最大大小(MB)
    int         max_files       = 3;                                    // 最大保留文件数
    std::string logger_name     = "app_logger";                         // 日志器名称
};

// ============================================================
//  内部辅助：字符串 → spdlog 日志级别
// ============================================================
namespace logging_detail {

inline spdlog::level::level_enum stringToLevel(const std::string &level_str)
{
    if (level_str == "trace")    return spdlog::level::trace;
    if (level_str == "debug")    return spdlog::level::debug;
    if (level_str == "info")     return spdlog::level::info;
    if (level_str == "warn")     return spdlog::level::warn;
    if (level_str == "error")    return spdlog::level::err;
    if (level_str == "critical") return spdlog::level::critical;
    if (level_str == "off")      return spdlog::level::off;
    return spdlog::level::info;
}

} // namespace logging_detail

// ============================================================
//  获取全局 logger 引用
// ============================================================
inline std::shared_ptr<spdlog::logger>& getLogger()
{
    static std::shared_ptr<spdlog::logger> instance;
    return instance;
}

// ============================================================
//  初始化日志系统
// ============================================================
inline bool initLogging(const LogConfig &cfg)
{
    try
    {
        // 1. 确保日志目录存在
        std::filesystem::path file_path(cfg.filename);
        if (file_path.has_parent_path())
        {
            std::filesystem::create_directories(file_path.parent_path());
        }

        // 2. 构建 sinks
        std::vector<spdlog::sink_ptr> sinks;

        if (cfg.log_console)
        {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_pattern(cfg.pattern);
            sinks.push_back(console_sink);
        }

        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            cfg.filename,
            static_cast<std::size_t>(cfg.max_size_mb) * 1024 * 1024,
            cfg.max_files);
        file_sink->set_pattern(cfg.pattern);
        sinks.push_back(file_sink);

        // 3. 创建 logger
        auto logger = std::make_shared<spdlog::logger>(
            cfg.logger_name, sinks.begin(), sinks.end());

        // 4. 设置日志级别
        auto lvl = logging_detail::stringToLevel(cfg.level);
        logger->set_level(lvl);

        // 5. 设置立即刷新
        if (cfg.immediate_flush)
        {
            logger->flush_on(lvl);
        }

        // 6. 注册为全局默认 logger
        spdlog::set_default_logger(logger);
        getLogger() = logger;

        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "日志系统初始化失败: " << e.what() << std::endl;
        return false;
    }
}

// ============================================================
//  关闭日志系统
// ============================================================
inline void shutdownLogging()
{
    auto &logger = getLogger();
    if (logger)
    {
        logger->flush();
        logger.reset();
    }
    spdlog::shutdown();
}
