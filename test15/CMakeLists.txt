project(test15)

# 查找源文件
file(GLOB SOURCES "src/*.cpp")

# 创建可执行文件 
add_executable(${PROJECT_NAME} ${SOURCES})

# 添加头文件包含路径
target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include # 本地头文件路径
    PUBLIC
        ${COMMON_INCLUDE_DIR} # 公共头文件路径 
)

# 链接 第三方库
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Threads::Threads
        libpqxx::pqxx            # PostgreSQL C++库
        yaml-cpp::yaml-cpp       # yaml-cpp库
        spdlog::spdlog           # spdlog库
        redis++::redis++         # redis++库
        nlohmann_json::nlohmann_json # nlohmann_json库
        SOCI::soci_postgresql       
        SOCI::soci_core         # SOCI库
        cpr::cpr                   # cpr库
        httplib::httplib
        higplat                   # 本地higplat库
)
