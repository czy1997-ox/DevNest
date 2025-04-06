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

set(CMAKE_BUILD_TYPE "Debug")

# Debug模式配置
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # 启用位置无关代码（PIC）
    # set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL "Enable PIC for ASan" FORCE)

    # 设置C/C++通用调试参数
    set(COMMON_DEBUG_FLAGS "-g3 -O0 -Wall -Wextra")

    # 应用至C编译器
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_DEBUG_FLAGS}" 
        CACHE STRING "Debug C flags" FORCE)

    # 应用至C++编译器
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_DEBUG_FLAGS}" 
        CACHE STRING "Debug C++ flags" FORCE)

    # 设置链接器参数（同时作用于C/C++目标）
    # set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address" 
    #     CACHE STRING "Debug linker flags" FORCE)

    # 标记高级变量
    mark_as_advanced(
        # CMAKE_POSITION_INDEPENDENT_CODE
        CMAKE_C_FLAGS
        CMAKE_CXX_FLAGS
        # CMAKE_EXE_LINKER_FLAGS
    )
endif()

add_subdirectory(src)
add_subdirectory(example)