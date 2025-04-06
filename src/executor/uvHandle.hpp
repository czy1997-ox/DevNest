#pragma once
#include "uv.h"

#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

// 前向声明EventLoop类
class EventLoop;

// 所有句柄的基类
class HandleBase {
public:
    HandleBase(EventLoop& loop, uv_handle_t* handle) 
    : loop_(loop), handle_(handle) { 
        loop_.register_handle(handle_); 
    }

    virtual ~HandleBase() {
        loop_.unregister_handle(handle_);
    }

    // 获取事件循环对象
    EventLoop& get_loop() const { return loop_; }

protected:
    EventLoop& loop_;
    uv_handle_t* handle_;
};

// 定时器句柄
class TimerHandle : public HandleBase {
public:
    using Callback = std::function<void()>;

    explicit TimerHandle(EventLoop& loop) : HandleBase(loop, reinterpret_cast<uv_handle_t*>(new uv_timer_t)) {
        uv_timer_init(loop.get(), get_as_timer());
        get_as_timer()->data = this;
    }

    ~TimerHandle() override {
        // 确保当handle关闭时能正确释放内存
        if (!uv_is_closing(handle_)) {
            uv_close(handle_, [](uv_handle_t* handle) { 
                delete reinterpret_cast<uv_timer_t*>(handle); });
        }
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
        if (self->callback_)
            self->callback_();
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

    ~SignalHandle() override {
        // 确保当handle关闭时能正确释放内存
        if (!uv_is_closing(handle_)) {
            uv_close(handle_, [](uv_handle_t* handle) { 
                delete reinterpret_cast<uv_signal_t*>(handle); });
        }
    }

    uv_signal_t* get_as_signal() const { return reinterpret_cast<uv_signal_t*>(handle_); }

    // 启动信号监听
    void start(int signum, Callback callback) {
        callback_ = std::move(callback);
        uv_signal_start(get_as_signal(), on_signal, signum);
    }

    void stop() { uv_signal_stop(get_as_signal()); }

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

// TCP 连接表示
struct TcpConnection {
    std::vector<char> buffer;
    uv_tcp_t tcp;

    explicit TcpConnection(size_t buffer_size = 65536) : buffer(buffer_size) {}
};

// TCP 基础句柄类
class TcpHandle : public HandleBase {
public:
    // 读取数据回调
    using DataCallback = std::function<void(const char* data, ssize_t len)>;
    // 连接关闭回调
    using CloseCallback = std::function<void()>;
    // 错误回调
    using ErrorCallback = std::function<void(int status)>;

    explicit TcpHandle(EventLoop& loop) : HandleBase(loop, reinterpret_cast<uv_handle_t*>(new uv_tcp_t)) {
        uv_tcp_init(loop.get(), get_as_tcp());
        get_as_tcp()->data = this;
    }

    ~TcpHandle() override {
        // 确保当handle关闭时能正确释放内存
        if (!uv_is_closing(handle_)) {
            uv_close(handle_, [](uv_handle_t* handle) { delete reinterpret_cast<uv_tcp_t*>(handle); });
        }
    }

    uv_tcp_t* get_as_tcp() const { return reinterpret_cast<uv_tcp_t*>(handle_); }

    // 设置错误回调
    void set_error_callback(ErrorCallback callback) { error_callback_ = std::move(callback); }

protected:
    DataCallback data_callback_;
    CloseCallback close_callback_;
    ErrorCallback error_callback_;

    // 处理错误的辅助方法
    void handle_error(int status) {
        if (status < 0 && error_callback_) {
            error_callback_(status);
        }
    }

    // 处理读取完成回调
    static void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {

        auto self = static_cast<TcpHandle*>(stream->data);
        if (nread < 0) {
            // EOF或错误
            if (nread == UV_EOF) {
                if (self->close_callback_) {
                    self->close_callback_();
                }
            } else {
                self->handle_error(nread);
            }
            return;
        }

        if (nread > 0 && self->data_callback_) {
            self->data_callback_(buf->base, nread);
        }
    }

    // 分配缓冲区回调
    static void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
        (void)handle; // 忽略未使用的参数
        static char buffer[65536];
        *buf = uv_buf_init(buffer, static_cast<unsigned int>(suggested_size));
    }

    // 写入回调
    static void on_write(uv_write_t* req, int status) {
        auto self = static_cast<TcpHandle*>(req->handle->data);
        self->handle_error(status);
        delete req;
    }
};

// TCP 服务器句柄
class TcpServerHandle : public TcpHandle {
public:
    // 新连接回调
    using ConnectionCallback = std::function<void(std::shared_ptr<TcpConnection>)>;

    explicit TcpServerHandle(EventLoop& loop) : TcpHandle(loop) {}

    // 绑定地址并开始监听
    bool bind_and_listen(const std::string& ip, int port, int backlog = 128) {
        struct sockaddr_in addr;
        int result = uv_ip4_addr(ip.c_str(), port, &addr);
        if (result != 0) {
            handle_error(result);
            return false;
        }

        result = uv_tcp_bind(get_as_tcp(), reinterpret_cast<const sockaddr*>(&addr), 0);
        if (result != 0) {
            handle_error(result);
            return false;
        }

        result = uv_listen(reinterpret_cast<uv_stream_t*>(get_as_tcp()), backlog, on_new_connection);
        if (result != 0) {
            handle_error(result);
            return false;
        }

        return true;
    }

    // 设置连接回调
    void set_connection_callback(ConnectionCallback callback) { connection_callback_ = std::move(callback); }

    // 设置数据回调
    void set_data_callback(DataCallback callback) { data_callback_ = std::move(callback); }

    // 设置关闭回调
    void set_close_callback(CloseCallback callback) { close_callback_ = std::move(callback); }

private:
    ConnectionCallback connection_callback_;

    // 新连接回调
    static void on_new_connection(uv_stream_t* server, int status) {

        auto self = static_cast<TcpServerHandle*>(server->data);
        if (status < 0) {
            self->handle_error(status);
            return;
        }

        auto client_connection = std::make_shared<TcpConnection>();
        int result = uv_tcp_init(self->loop_.get(), &client_connection->tcp);
        if (result != 0) {
            self->handle_error(result);
            return;
        }

        client_connection->tcp.data = self;

        result = uv_accept(server, reinterpret_cast<uv_stream_t*>(&client_connection->tcp));
        if (result != 0) {
            self->handle_error(result);
            return;
        }

        if (self->connection_callback_) {
            self->connection_callback_(client_connection);
        }

        result = uv_read_start(reinterpret_cast<uv_stream_t*>(&client_connection->tcp), TcpHandle::alloc_buffer,
                               TcpHandle::on_read);
        if (result != 0) {
            self->handle_error(result);
        }
    }
};

// TCP 客户端句柄
class TcpClientHandle : public TcpHandle {
public:
    // 连接回调
    using ConnectCallback = std::function<void(bool success)>;

    explicit TcpClientHandle(EventLoop& loop) 
    : TcpHandle(loop) {}

    // 连接到服务器
    void connect(const std::string& ip, int port, ConnectCallback callback) {
        connect_callback_ = std::move(callback);

        struct sockaddr_in addr;
        int result = uv_ip4_addr(ip.c_str(), port, &addr);
        if (result != 0) {
            handle_error(result);
            return;
        }

        auto connect_req = new uv_connect_t();
        connect_req->data = this;

        result = uv_tcp_connect(connect_req, get_as_tcp(), reinterpret_cast<const sockaddr*>(&addr), on_connect);
        if (result != 0) {
            handle_error(result);
            delete connect_req;
        }
    }

    // 设置数据回调
    void set_data_callback(DataCallback callback) { data_callback_ = std::move(callback); }

    // 设置关闭回调
    void set_close_callback(CloseCallback callback) { close_callback_ = std::move(callback); }

    // 发送数据
    bool send(const char* data, size_t len) {
        auto write_req = new uv_write_t();

        uv_buf_t buf = uv_buf_init(const_cast<char*>(data), static_cast<unsigned int>(len));

        int result = uv_write(write_req, reinterpret_cast<uv_stream_t*>(get_as_tcp()), &buf, 1, on_write);
        if (result != 0) {
            handle_error(result);
            delete write_req;
            return false;
        }

        return true;
    }

    // 开始读取数据
    bool start_read() {
        int result =
            uv_read_start(reinterpret_cast<uv_stream_t*>(get_as_tcp()), TcpHandle::alloc_buffer, TcpHandle::on_read);
        if (result != 0) {
            handle_error(result);
            return false;
        }

        return true;
    }

    // 停止读取数据
    void stop_read() { uv_read_stop(reinterpret_cast<uv_stream_t*>(get_as_tcp())); }

private:
    ConnectCallback connect_callback_;

    // 连接回调
    static void on_connect(uv_connect_t* req, int status) {
        auto self = static_cast<TcpClientHandle*>(req->handle->data);

        if (self->connect_callback_) {
            self->connect_callback_(status == 0);
        }

        if (status == 0) {
            self->start_read();
        } else {
            self->handle_error(status);
        }

        delete req;
    }
};

using TimerHandlePtr = std::shared_ptr<TimerHandle>;
using SignalHandlePtr = std::shared_ptr<SignalHandle>;
using TcpServerHandlePtr = std::shared_ptr<TcpServerHandle>;
using TcpClientHandlePtr = std::shared_ptr<TcpClientHandle>;

// UDP 基础句柄类
class UdpHandle : public HandleBase {
public:
    // 接收数据回调
    using DataCallback = std::function<void(const char* data, ssize_t len, const struct sockaddr* addr)>;
    // 错误回调
    using ErrorCallback = std::function<void(int status)>;

    explicit UdpHandle(EventLoop& loop) 
    : HandleBase(loop, reinterpret_cast<uv_handle_t*>(new uv_udp_t)) {
        uv_udp_init(loop.get(), get_as_udp());
        get_as_udp()->data = this;
    }

    ~UdpHandle() override {
        // 确保当handle关闭时能正确释放内存
        if (!uv_is_closing(handle_)) {
            uv_close(handle_, [](uv_handle_t* handle) { delete reinterpret_cast<uv_udp_t*>(handle); });
        }
    }

    uv_udp_t* get_as_udp() const { return reinterpret_cast<uv_udp_t*>(handle_); }

    // 设置错误回调
    void set_error_callback(ErrorCallback callback) { error_callback_ = std::move(callback); }

protected:
    DataCallback data_callback_;
    ErrorCallback error_callback_;

    // 处理错误的辅助方法
    void handle_error(int status) {
        if (status < 0 && error_callback_) {
            error_callback_(status);
        }
    }

    // 分配缓冲区回调
    static void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
        (void)handle; // 忽略未使用的参数
        static char buffer[65536];
        *buf = uv_buf_init(buffer, static_cast<unsigned int>(suggested_size));
    }

    // 接收数据回调
    static void on_recv(uv_udp_t* handle,
                        ssize_t nread, 
                        const uv_buf_t* buf, 
                        const struct sockaddr* addr, 
                        unsigned flags) {
        (void)flags; // 忽略未使用的参数
        auto self = static_cast<UdpHandle*>(handle->data);

        if (nread < 0) {
            self->handle_error(nread);
            return;
        }

        if (nread > 0 && self->data_callback_) {
            self->data_callback_(buf->base, nread, addr);
        }
    }

    // 发送回调
    static void on_send(uv_udp_send_t* req, int status) {
        auto self = static_cast<UdpHandle*>(req->handle->data);
        self->handle_error(status);
        delete req;
    }
};

// UDP 服务器句柄
class UdpServerHandle : public UdpHandle {
public:
    explicit UdpServerHandle(EventLoop& loop) : UdpHandle(loop) {}

    // 绑定地址
    bool bind(const std::string& ip, int port, unsigned int flags = 0) {
        struct sockaddr_in addr;
        int result = uv_ip4_addr(ip.c_str(), port, &addr);
        if (result != 0) {
            handle_error(result);
            return false;
        }

        result = uv_udp_bind(get_as_udp(), reinterpret_cast<const sockaddr*>(&addr), flags);
        if (result != 0) {
            handle_error(result);
            return false;
        }

        return true;
    }

    // 开始接收数据
    bool start_recv(DataCallback callback) {
        data_callback_ = std::move(callback);

        int result = uv_udp_recv_start(get_as_udp(), alloc_buffer, on_recv);
        if (result != 0) {
            handle_error(result);
            return false;
        }

        return true;
    }

    // 停止接收数据
    void stop_recv() { uv_udp_recv_stop(get_as_udp()); }

    // 向特定地址发送数据
    bool send(const char* data, size_t len, const struct sockaddr* addr) {
        auto send_req = new uv_udp_send_t();

        uv_buf_t buf = uv_buf_init(const_cast<char*>(data), static_cast<unsigned int>(len));

        int result = uv_udp_send(send_req, get_as_udp(), &buf, 1, addr, on_send);
        if (result != 0) {
            handle_error(result);
            delete send_req;
            return false;
        }

        return true;
    }

    // 向特定IP和端口发送数据
    bool send_to(const char* data, size_t len, const std::string& ip, int port) {
        struct sockaddr_in addr;
        int result = uv_ip4_addr(ip.c_str(), port, &addr);
        if (result != 0) {
            handle_error(result);
            return false;
        }

        return send(data, len, reinterpret_cast<const sockaddr*>(&addr));
    }
};

// UDP 客户端句柄
class UdpClientHandle : public UdpHandle {
public:
    explicit UdpClientHandle(EventLoop& loop) 
    : UdpHandle(loop), is_connected_(false) {}

    // 连接到特定地址（注意：UDP是无连接的，这里只是保存目标地址）
    bool connect(const std::string& ip, int port) {
        int result = uv_ip4_addr(ip.c_str(), port, &remote_addr_);
        if (result != 0) {
            handle_error(result);
            return false;
        }

        is_connected_ = true;
        return true;
    }

    // 开始接收数据
    bool start_recv(DataCallback callback) {
        data_callback_ = std::move(callback);

        int result = uv_udp_recv_start(get_as_udp(), alloc_buffer, on_recv);
        if (result != 0) {
            handle_error(result);
            return false;
        }

        return true;
    }

    // 停止接收数据
    void stop_recv() { uv_udp_recv_stop(get_as_udp()); }

    // 发送数据到连接的地址
    bool send(const char* data, size_t len) {
        if (!is_connected_) {
            return false;
        }

        auto send_req = new uv_udp_send_t();

        uv_buf_t buf = uv_buf_init(const_cast<char*>(data), static_cast<unsigned int>(len));

        int result =
            uv_udp_send(send_req, get_as_udp(), &buf, 1, reinterpret_cast<const sockaddr*>(&remote_addr_), on_send);
        
        if (result != 0) {
            handle_error(result);
            delete send_req;
            return false;
        }

        return true;
    }

    // 向特定地址发送数据
    bool send_to(const char* data, size_t len, const std::string& ip, int port) {
        struct sockaddr_in addr;
        int result = uv_ip4_addr(ip.c_str(), port, &addr);
        if (result != 0) {
            handle_error(result);
            return false;
        }

        auto send_req = new uv_udp_send_t();

        uv_buf_t buf = uv_buf_init(const_cast<char*>(data), static_cast<unsigned int>(len));

        result = uv_udp_send(send_req, get_as_udp(), &buf, 1, reinterpret_cast<const sockaddr*>(&addr), on_send);
        if (result != 0) {
            handle_error(result);
            delete send_req;
            return false;
        }

        return true;
    }

private:
    struct sockaddr_in remote_addr_;
    bool is_connected_;
};

using UdpServerHandlePtr = std::shared_ptr<UdpServerHandle>;
using UdpClientHandlePtr = std::shared_ptr<UdpClientHandle>;