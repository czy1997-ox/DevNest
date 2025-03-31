#pragma once

#include "uv.h"
#include "uvHandle.hpp"

#include <functional>
#include <memory>
#include <unordered_set>
class EventLoop {
public:
    EventLoop() {
        uv_loop_init(&loop_);
    }

    ~EventLoop() {
        // 关闭所有未关闭的句柄
        close_all_handles();
        // 运行一次循环以处理关闭回调
        uv_run(&loop_, UV_RUN_ONCE);
        uv_loop_close(&loop_);
    }

    uv_loop_t* get() { return &loop_; }

    int run(uv_run_mode mode = UV_RUN_DEFAULT) {
        return uv_run(&loop_, mode);
    }

    void stop() {
        uv_stop(&loop_); // 内部调用 libuv 的 uv_stop
    }

    void register_handle(uv_handle_t* handle) {
        handles_.insert(handle);
    }

    void unregister_handle(uv_handle_t* handle) {
        handles_.erase(handle);
    }

private:
    uv_loop_t loop_;
    std::unordered_set<uv_handle_t*> handles_;

    void close_all_handles() {
        for (auto handle : handles_) {
            if (!uv_is_closing(handle)) {
                uv_close(handle, [](uv_handle_t* ) {});
            }
        }
    }
};
