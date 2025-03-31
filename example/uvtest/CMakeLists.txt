# uv Test程序配置
add_executable(uv_test ${CMAKE_CURRENT_SOURCE_DIR}/uvtest.cpp)
target_include_directories(uv_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/_deps/include
)
target_link_libraries(uv_test 
    PRIVATE
    ${CMAKE_SOURCE_DIR}/_deps/lib/libuv.so)
set_target_properties(uv_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/output/bin"
)