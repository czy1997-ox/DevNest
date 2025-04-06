#pragma once

#include "uv.h"

#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_set>

// 前向声明
class ThreadCommunicator;
using ThreadCommunicatorPtr = std::shared_ptr<ThreadCommunicator>;

// 跨线程通信抽象接口
class ThreadCommunicator {
public:
    virtual ~ThreadCommunicator() = default;
    virtual void post(std::function<void()> task) = 0;
    virtual void initialize(uv_loop_t* loop) = 0;
    virtual void close() = 0;
};

// libuv实现的跨线程通信器
class UvAsyncCommunicator : public ThreadCommunicator {
public:
    UvAsyncCommunicator() : async_handle_(nullptr), initialized_(false) {}

    ~UvAsyncCommunicator() { close(); }

    void initialize(uv_loop_t* loop) override {
        if (initialized_)
            return;

        async_handle_ = new uv_async_t;
        async_handle_->data = this;
        uv_async_init(loop, async_handle_, on_async);
        initialized_ = true;
    }

    void post(std::function<void()> task) override {
        if (!initialized_) {
            throw std::runtime_error("Communicator not initialized");
        }

        {
            std::lock_guard<std::mutex> lock(task_mutex_);
            task_queue_.push(std::move(task));
        }
        uv_async_send(async_handle_);
    }

    void close() override {
        if (!initialized_)
            return;

        if (async_handle_ && !uv_is_closing(reinterpret_cast<uv_handle_t*>(async_handle_))) {
            uv_close(reinterpret_cast<uv_handle_t*>(async_handle_),
                     [](uv_handle_t* handle) { delete reinterpret_cast<uv_async_t*>(handle); });
            async_handle_ = nullptr;
        }

        initialized_ = false;
    }

private:
    static void on_async(uv_async_t* handle) {
        auto* self = static_cast<UvAsyncCommunicator*>(handle->data);
        self->process_tasks();
    }

    void process_tasks() {
        std::queue<std::function<void()>> tasks;

        {
            std::lock_guard<std::mutex> lock(task_mutex_);
            tasks.swap(task_queue_);
        }

        while (!tasks.empty()) {
            auto task = std::move(tasks.front());
            tasks.pop();

            try {
                task();
            } catch (const std::exception& e) {
                fprintf(stderr, "Exception in task: %s\n", e.what());
            } catch (...) {
                fprintf(stderr, "Unknown exception in task\n");
            }
        }
    }

    uv_async_t* async_handle_;
    bool initialized_;
    std::queue<std::function<void()>> task_queue_;
    std::mutex task_mutex_;
};

class EventLoop {
public:
    EventLoop(std::shared_ptr<ThreadCommunicator> communicator = std::make_shared<UvAsyncCommunicator>())
        : loop_(nullptr), communicator_(communicator) {
        loop_ = new uv_loop_t;
        uv_loop_init(loop_);

        // 初始化通信器
        communicator_->initialize(loop_);
    }

    ~EventLoop() {
        // 关闭所有句柄
        close_all_handles();

        // 关闭通信器
        communicator_->close();

        // 运行事件循环直到所有handle都关闭
        uv_run(loop_, UV_RUN_DEFAULT);

        // 清理事件循环
        int result = uv_loop_close(loop_);
        if (result != 0) {
            fprintf(stderr, "Error closing loop: %s\n", uv_strerror(result));
        }

        delete loop_;
    }

    // 使用通信器实现post功能
    void post(std::function<void()> task) { communicator_->post(std::move(task)); }

    uv_loop_t* get() const { return loop_; }
    void run() { uv_run(loop_, UV_RUN_DEFAULT); }
    void stop() { uv_stop(loop_); }

    void register_handle(uv_handle_t* handle) {
        std::lock_guard<std::mutex> lock(handles_mutex_);
        handles_.insert(handle);
    }

    void unregister_handle(uv_handle_t* handle) {
        std::lock_guard<std::mutex> lock(handles_mutex_);
        handles_.erase(handle);
    }

private:
    void close_all_handles() {
        std::lock_guard<std::mutex> lock(handles_mutex_);
        for (auto* handle : handles_) {
            if (!uv_is_closing(handle)) {
                uv_close(handle, [](uv_handle_t* handle) {
                    (void)handle; // 忽略未使用的参数
                });
            }
        }
    }

    uv_loop_t* loop_;
    std::shared_ptr<ThreadCommunicator> communicator_;
    std::unordered_set<uv_handle_t*> handles_;
    std::mutex handles_mutex_;
};
