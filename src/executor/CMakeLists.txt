# Execute Test程序配置
add_executable(uvExecutor_Test 
    ${CMAKE_SOURCE_DIR}/src/executor/uvExecutor_Test.cc
    ${CMAKE_SOURCE_DIR}/src/executor/uvHandle.hpp
    ${CMAKE_SOURCE_DIR}/src/executor/uvExecutor.hpp)
# 添加头文件搜索路径
target_include_directories(uvExecutor_Test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/executor
        ${CMAKE_SOURCE_DIR}/_deps/include
)
target_link_libraries(uvExecutor_Test 
    PRIVATE 
        ${CMAKE_SOURCE_DIR}/_deps/lib/libuv.so)
set_target_properties(uvExecutor_Test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/output/bin"
)