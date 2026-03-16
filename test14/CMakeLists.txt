project(test14)

# 查找源文件
file(GLOB SOURCES "src/*.cpp")

# 创建可执行文件
add_executable(${PROJECT_NAME} ${SOURCES})

# 添加头文件包含路径
target_include_directories(${PROJECT_NAME}
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/include
	PUBLIC
		${COMMON_INCLUDE_DIR}
)

# 链接库
target_link_libraries(${PROJECT_NAME}
	PRIVATE
		Threads::Threads
		higplat
)
