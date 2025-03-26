#pragma once
#include "uv.h"

#include <functional>
#include <mutex>
#include <iostream>
#include <uvExecutor.hpp>

// 所有句柄的基类
class HandleBase {
public:
    HandleBase(Loop& loop, uv_handle_t* handle) 
        : loop_(loop), handle_(handle) {
        loop_.register_handle(handle_);
    }

    virtual ~HandleBase() {
        loop_.unregister_handle(handle_);
        if (!uv_is_closing(handle_)) {
            uv_close(handle_, [](uv_handle_t* handle) {
                // 释放内存的回调
                delete reinterpret_cast<uv_handle_t*>(handle);
            });
        }
    }

protected:
    Loop& loop_;
    uv_handle_t* handle_;
};

// 定时器句柄
class TimerHandle : public HandleBase {
public:
    explicit TimerHandle(Loop& loop)
        : HandleBase(loop, reinterpret_cast<uv_handle_t*>(new uv_timer_t)) {
        uv_timer_init(loop.get(), get_as_timer());
    }

    uv_timer_t* get_as_timer() const {
        return reinterpret_cast<uv_timer_t*>(handle_);
    }

    void start(uint64_t timeout, uint64_t repeat, uv_timer_cb callback) {
        uv_timer_start(get_as_timer(), callback, timeout, repeat);
    }

    void stop() {
        uv_timer_stop(get_as_timer());
    }
};