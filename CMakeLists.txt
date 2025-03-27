# 文件: CMakeLists.txt
cmake_minimum_required(VERSION 3.14)
project(YourProject)

# set (FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "禁用远程仓库更新" FORCE )
# 强制所有 FetchContent 下载和构建到项目根目录的 _deps 文件夹
# set(FETCHCONTENT_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/_deps" CACHE PATH "" FORCE)

# 添加 CMake 模块路径
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# 包含 libuv 配置
# include(cmake/GetLibUv.cmake)

# uv Test程序配置
add_executable(uv_test example/uvtest.cpp)
target_include_directories(uv_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/executor
        ${CMAKE_CURRENT_SOURCE_DIR}/_deps/include
)
target_link_libraries(uv_test 
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/_deps/lib/libuv.so)
set_target_properties(uv_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/output/bin"
)

# Execute Test程序配置
add_executable(uvExecutor_Test 
    src/executor/uvExecutor_Test.cc
    src/executor/uvHandle.hpp
    src/executor/uvExecutor.hpp)
# 添加头文件搜索路径
target_include_directories(uvExecutor_Test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/executor
        ${CMAKE_CURRENT_SOURCE_DIR}/_deps/include
)
target_link_libraries(uvExecutor_Test 
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/_deps/lib/libuv.so)
set_target_properties(uvExecutor_Test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/output/bin"
)