# 文件: cmake/libuv.cmake
# 功能: 通过 FetchContent 自动集成 libuv

cmake_minimum_required(VERSION 3.14)
project(YourProject)

# [1] 全局配置
set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/_deps/source" CACHE PATH "" FORCE)
set(FETCHCONTENT_QUIET OFF)

# [2] 添加统一构建选项
set(LIBUV_LIB_TYPE "SHARED" CACHE STRING "构建类型 [SHARED|STATIC|BOTH]")
set_property(CACHE LIBUV_LIB_TYPE PROPERTY STRINGS SHARED STATIC BOTH)

# [3] 根据选项设置变量
if(LIBUV_LIB_TYPE STREQUAL "SHARED")
    set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
    set(BUILD_STATIC_LIBS OFF CACHE BOOL "" FORCE)
elseif(LIBUV_LIB_TYPE STREQUAL "STATIC")
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
elseif(LIBUV_LIB_TYPE STREQUAL "BOTH")
    set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
    set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
endif()

# [4] 集成 libuv
include(FetchContent)
FetchContent_Declare(
    libuv
    GIT_REPOSITORY    https://github.com/libuv/libuv.git
    GIT_TAG           v1.41.1
    GIT_SHALLOW       TRUE
)

set(LIBUV_BUILD_TESTS OFF CACHE BOOL "Disable tests" FORCE)

FetchContent_MakeAvailable(libuv)

# 定义平台无关的别名
if(TARGET uv_a OR TARGET uv)
    message("In Alias")
    # 核心别名（自动选择静态/动态库）
    add_library(libuv::libuv INTERFACE IMPORTED)
    
    # 根据构建类型链接对应库
    if(LIBUV_LIB_TYPE STREQUAL "SHARED")
        target_link_libraries(libuv::libuv INTERFACE uv)
    elseif(LIBUV_LIB_TYPE STREQUAL "STATIC")
        target_link_libraries(libuv::libuv INTERFACE uv_a)
    else()
        # 双模式时优先链接共享库
        target_link_libraries(libuv::libuv INTERFACE uv uv_a)
    endif()

    # 包含头文件路径
    target_include_directories(libuv::libuv INTERFACE
        "${FETCHCONTENT_BASE_DIR}/libuv-src/include"
    )

endif()

# [5] 输出路径与命名规则
set(_LIBUV_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/_deps/lib")

set_target_properties(uv uv_a PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${_LIBUV_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${_LIBUV_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${_LIBUV_OUTPUT_DIR}"
    OUTPUT_NAME_DEBUG "uvd$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,STATIC_LIBRARY>:_static>"
    OUTPUT_NAME_RELEASE "uv$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,STATIC_LIBRARY>:_static>"
)

add_custom_target(clean_all
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_SOURCE_DIR}/_deps"
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_BINARY_DIR}"
    COMMENT "Cleaning all build artifacts"
)