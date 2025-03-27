#pragma once
#include "uv.h"

#include <functional>
#include <mutex>
#include <iostream>
#include <uvExecutor.hpp>

// 所有句柄的基类
class HandleBase {
public:
    HandleBase(EventLoop& loop, uv_handle_t* handle) 
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
    EventLoop& loop_;
    uv_handle_t* handle_;
};

// 定时器句柄
class TimerHandle : public HandleBase {
public:
    using Callback = std::function<void()>;

    explicit TimerHandle(EventLoop& loop)
        : HandleBase(loop, reinterpret_cast<uv_handle_t*>(new uv_timer_t)) {
        uv_timer_init(loop.get(), get_as_timer());
        get_as_timer()->data = this;
    }

    uv_timer_t* get_as_timer() const {
        return reinterpret_cast<uv_timer_t*>(handle_);
    }

    void start(uint64_t timeout, uint64_t repeat, Callback callback) {
        callback_ = std::move(callback);
        uv_timer_start(get_as_timer(), on_timer_trigger, timeout, repeat);
    }

    void stop() {
        uv_timer_stop(get_as_timer());
    }

private:
    static void on_timer_trigger(uv_timer_t* handle) {
        auto self = static_cast<TimerHandle*>(handle->data);
        if (self->callback_) self->callback_();
    }

    Callback callback_;
};

class SignalHandle : public HandleBase {
public:
    using Callback = std::function<void(int signum)>;

    explicit SignalHandle(EventLoop& loop)
        : HandleBase(loop, reinterpret_cast<uv_handle_t*>(new uv_signal_t)) {
        uv_signal_init(loop.get(), get_as_signal());
        get_as_signal()->data = this; // 保存实例指针
    }

    uv_signal_t* get_as_signal() const {
        return reinterpret_cast<uv_signal_t*>(handle_);
    }

    // 启动信号监听
    void start(int signum, Callback callback) {
        callback_ = std::move(callback);
        uv_signal_start(get_as_signal(), on_signal, signum);
    }

    void stop() {
        uv_signal_stop(get_as_signal());
    }

private:
    // 静态回调函数，通过 data 获取实例
    static void on_signal(uv_signal_t* handle, int signum) {
        auto self = static_cast<SignalHandle*>(handle->data);
        if (self->callback_) {
            self->callback_(signum);
        }
    }

    Callback callback_; // 用户自定义回调
};

using TimerHandlePtr = std::shared_ptr<TimerHandle>;
using SignalHandlePtr =  std::shared_ptr<SignalHandle>;