编译和构建：在cpp_demo文件夹下输入指令
cmake --preset linux-debug
cmake --build build

# 一、libpqxx

### test1

- [main.cpp]— 展示了基本的 libpqxx 使用模式：连接、建表、事务与非事务、结果遍历与异常处理、INSERT/SELECT/UPDATE/DELETE、结果遍历与异常处理。

- [test.cpp]

  - 建立并验证数据库连接（有 Windows 控制台编码处理）。
  - 使用非事务/事务执行 SELECT、UPDATE、COUNT、聚合（MAX）等操作的示例。
  - 参数化查询与异常捕获演示。

- 这里链接第三方库：

  ```cmake
  # 统一使用 find_package 查找所有第三方库
  find_package(libpqxx REQUIRED)
  # 链接 libpqxx
  target_link_libraries(${PROJECT_NAME}
      PRIVATE
          libpqxx::pqxx
          ${PostgreSQL_LIBRARIES}
  )
  ```

### test2

- 目的：一个可由 YAML 配置的多线程示例程序 —— 支持前台/守护进程模式、日志（spdlog）、以及在独立线程中用 libpqxx 访问 PostgreSQL。
- 入口：main()；主要功能：配置加载、日志初始化、信号处理、线程创建（常规任务 + 可选 DB 线程）、可选守护化。

主要结构与要点

- 配置：通过 CConfig 读取 ../config/subproject2.yaml（thread_count、daemon_mode、dbname/user/password/hostaddr/port、日志相关字段等）。
- 日志：initLogging(...) 使用 stdout + rotating_file_sink；全局 logger g_logger。
- 守护进程：becomeDaemon() 使用 fork()/setsid()/重定向 /dev/null（POSIX 实现）。
- 线程：
  - threadTask(int): 记录线程启动并立即结束（原有的循环被注释）。
  - dbThreadTask(conn_str): 在独立 pqxx::connection 上执行 SELECT（解析 users 表的字段）；包含 INSERT/UPDATE 示例（被注释）。
- 信号：SIGINT/SIGTERM 设置为 signalHandler，设置 bExit 标志（但现有线程并未持续检查该标志）。
- 依赖：spdlog、libpqxx、CConfig（项目 CMakeLists 显示类似依赖）。
- 平台：源码包含 <unistd.h>, fork(), /dev/null 等 POSIX-only 操作 — 在 Windows 上不可用/不可编译。

# 二、nlohmann/json

### test3

- 该文件演示了使用 nlohmann::json 对自定义结构体 ns::person 进行手动序列化与反序列化的基础用法。

要点

- 功能：构造 ns::person，手动把字段写入 json（j[...] = ...），再从 json 里逐字段取出并构造对象以验证。

### test4

- 作用：演示 nlohmann::json 对自定义类型 ns::person 的 ADL（to_json/from_json）序列化与反序列化，并打印结果以验证。

要点

- 实现了 ns::person 及其 to_json/from_json（使用 at(...).get_to(...)，在字段缺失时会抛出异常）。
- 在 main() 中进行：
  - 将 ns::person 转为 json（序列化）并打印；
  - 从 json 恢复 ns::person（反序列化）并打印字段值。

### test5

- 作用：读取 CConfig 配置，使用 libpqxx 从 PostgreSQL 的 users 表查询记录，利用 nlohmann::json 把结果以 JSON 输出（支持全表查询与按 id 的参数化查询）。

要点

- 输出：将查询结果序列化为 JSON 数组（利用 ADL to_json）。
- 配置：从 ../config/CConfig.yaml 读取 DB 连接信息（若不完整则返回错误）。
- 两个主要函数：query_users_as_json（全表）、query_user_by_id（按 id，示例返回 id=1）。
- 错误码：2 —— 配置错误，3 —— DB 操作异常。

# 三、redis

### test6

- 使用 redis++ 演示连接、PING、SET/GET；

要点

- 功能：建立到 Redis 的连接（ConnectionOptions）、执行 ping、set/get 并打印结果；pub/sub 示例被注释掉。
- 依赖：sw/redis++（redis++）。
- 注意事项：代码中硬编码了远程 IP 和明文密码；对 redis.get("name") 直接解引用会在键不存在时崩溃；没有重连/超时或敏感信息处理逻辑。

### test7

- 作用：基于 redis++ 实现的订阅者示例 —— 读取 CConfig、订阅 chat_room、按消息回调打印时间戳，支持 SIGINT/SIGTERM 优雅退出（通过 socket_timeout + 捕获 TimeoutError 实现非阻塞检查退出标志）。

主要要点

- 信号与退出：使用 atomic running 标志，SIGINT/SIGTERM 设置 running=false，consume() 的 socket_timeout 保证能及时响应退出。
- 错误处理：TimeoutError 被吞掉继续循环；Redis 错误会打印并短暂停顿后重试。
- 配置：优先从 CConfig 加载，存在默认常量回退。
- 稳健性：已处理超时与部分异常、在退出时调用 unsubscribe。

### test8

- 交互式 Redis 发布者，读取 CConfig，向指定频道发布用户输入的消息并打印接收者数量。

主要行为要点

- 从 ../config/redis_test.yaml（或内置默认）读取连接信息并连接 Redis；
- 交互式循环：读取频道与消息，调用 redis.publish，并显示接收者数量；
- 对用户输入 "quit"/"exit" 退出；未显式处理 EOF（Ctrl+D / Ctrl+Z）或 publish 抛出的异常；
- 依赖 sw/redis++ 和 CConfig。

### test9

- 使用 SOCI 将 PostgreSQL 的 users 表映射为 C++ 的 User 类型并演示查询（单条 + 全表），同时展示了 type_conversion 的实现。

要点

- 功能：定义 User 结构体 + soci::type_conversion<User>（from_base / to_base），用 soci::session 执行参数化查询与 rowset 迭代并打印结果。
- 配置：通过 CConfig 读取数据库连接参数并拼接 libpq 风格连接串。
- 错误处理：对映射异常有局部 catch，主流程在捕获 std::exception 后返回非零退出码。

### test10

- 使用 SOCI + `CConfig` 演示对 PostgreSQL 的同步访问：配置验证、按列绑定的单行查询、`rowset` 全表遍历与简单计数查询。

主要行为要点

- 配置：从 `../config/CConfig.yaml` 读取 `dbname`、`user`、`password`、`hostaddr`、`port`（若配置不完整程序会打印错误并以退出码 `2` 退出）。
- 连接：将配置拼成 libpq 风格连接串并用 `soci::session` 建立 PostgreSQL 连接（依赖 SOCI 的 postgresql 后端）。
- 单行查询：演示使用多个 `soci::into` 分别绑定每个字段（比 `type_conversion` 更直接、可靠）的参数化查询（示例：按 `id` 查询并打印 `User`）。
- 全表遍历：使用 `soci::rowset<soci::row>` 迭代 `users` 表并按字段读取（`id`、`username`、`full_name`、`email`、`phone`）并打印结果。
- 计数示例：演示 `SELECT COUNT(*)` 到 `int` 的用法。
- 错误与退出：捕获 std::exception 并返回非零退出码（一般异常返回 `1`）；配置错误返回 `2`。

快速运行提示

- 依赖：SOCI (PostgreSQL backend)、项目的 `CConfig`（请确保 `users` 表存在并含示例字段）。
- 运行：先配置好 `../config/CConfig.yaml`（填写 DB 连接信息），再构建并运行生成的 `test10` 可执行文件以查看示例输出。

### test11

- 作用：基于 `CConfig + spdlog` 的日志服务示例，支持配置化日志级别/格式、滚动文件输出、可选控制台输出，以及前台/守护进程两种运行模式。

主要行为要点

- 配置：从 `../config/spdlog.yaml` 读取 `daemonMode`、`log_console`、`level`、`pattern`、`filename`、`max_size`、`max_files` 等参数；加载失败时回退代码默认值。
- 日志：始终启用 `rotating_file_sink`，可按 `log_console` 追加彩色控制台 sink；支持按级别即时刷新。
- 守护进程：当 `daemonMode=true` 时执行双重 `fork + setsid`，并根据 `log_console` 决定是否保留 `stdout/stderr`。
- 信号与退出：注册 `SIGINT/SIGTERM`，通过全局退出标志结束主循环，退出前 `flush` 并 `spdlog::shutdown()`。
- 平台说明：代码依赖 `<unistd.h>`、`fork()`、`/dev/null` 等 POSIX 能力，在 Windows 上不可直接运行。

快速运行提示

- 配置好 `../config/spdlog.yaml` 后构建并运行 `test11`；
- 前台调试建议：`daemonMode=false` 且 `log_console=true`；
- 纯后台运行建议：`daemonMode=true` 且 `log_console=false`（日志写入文件）。

# 四、HTTP / REST

### test12

- 作用：使用 `cpr` 发起 HTTP GET 请求，并用 `nlohmann::json` 解析返回结果。
- 交互：从标准输入读取 id（输入 `q` 或 `Q` 退出），然后对 `http://127.0.0.1:8080/users?id=<id>` 发起请求。
- 依赖：`cpr`（HTTP 客户端）+ `nlohmann::json`（JSON 解析）。

### test13

- 作用：使用 `httplib` 启动一个简单的 HTTP 服务，提供 `/users?id=<id>` 的查询接口。
- 特色：内置模拟用户数据库（`unordered_map<int, json>`），并对参数错误/未找到进行响应。
- 运行：启动程序后，可用浏览器或 `curl` 访问 `http://127.0.0.1:8080/users?id=5`。

# 五、higplat / 订阅与轮询

### test14

- 目的：展示如何通过 `higplat` 协议轮询 `WATCHDOG` 标签并在值发生变化时输出。
- 逻辑：`readb` 读取标签值，每 200ms 轮询一次，发现变化时打印。
- 依赖：`common_include/higplat.h`（`connectgplat/readb/disconnectgplat`）。

### test15

- 目的：展示 `higplat` 的发布订阅模型：一个线程订阅 `WATCHDOG` 并监听事件，另一个线程周期写入心跳值（`writeb`）。
- 特点：事件驱动 + 超时 `waitpostdata`（本例用 200ms 轮询超时）并在 `WATCHDOG` 值变化时打印。
- 依赖：`common_include/higplat.h`（`connectgplat/subscribe/waitpostdata/writeb`）。
