#include <chrono>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include "uvExecutor.hpp"
#include "uvHandle.hpp"

// TCP服务器示例函数
void tcp_server_example() {
    std::cout << "启动TCP服务器..." << std::endl;

    EventLoop loop;
    auto server = std::make_shared<TcpServerHandle>(loop);
    // 存储所有活动连接
    auto connections = std::make_shared<std::vector<std::shared_ptr<TcpConnection>>>();

    if (!server->bind_and_listen("0.0.0.0", 8080)) {
        std::cout << "服务器绑定失败" << std::endl;
        return;
    }

    // 设置连接回调
    server->set_connection_callback([connections](std::shared_ptr<TcpConnection> conn) {
        std::cout << "新的TCP客户端连接" << std::endl;
        connections->push_back(conn);  // 保存连接

        // 获取客户端地址信息
        struct sockaddr_storage addr;
        int len = sizeof(addr);
        uv_tcp_getpeername(&conn->tcp, reinterpret_cast<struct sockaddr*>(&addr), &len);

        char ip[INET6_ADDRSTRLEN];
        int port = 0;

        if (addr.ss_family == AF_INET) {
            auto* ipv4 = reinterpret_cast<struct sockaddr_in*>(&addr);
            uv_inet_ntop(AF_INET, &ipv4->sin_addr, ip, sizeof(ip));
            port = ntohs(ipv4->sin_port);
            std::cout << "客户端IP: " << ip << ":" << port << std::endl;
        }
    });

    // 设置数据回调 - 实时打印收到的数据
    server->set_data_callback([](const char* data, ssize_t len) {
        std::string message(data, len);
        std::cout << "收到客户端消息: " << message << std::endl;

        // 显示收到的字节数
        std::cout << "收到 " << len << " 字节数据" << std::endl;
    });

    // 设置错误回调
    server->set_error_callback([](int status) { std::cout << "服务器错误: " << uv_strerror(status) << std::endl; });

    std::cout << "TCP服务器监听在 0.0.0.0:8080" << std::endl;

    SignalHandlePtr signal = std::make_shared<SignalHandle>(loop);
    signal->start(SIGINT, [&loop, connections, server](int)
                  {
        std::cout << "正在关闭服务器..." << std::endl;
        
        // 首先关闭所有客户端连接
        for(auto& conn : *connections) {
            conn->close([](int status) {
                if (status < 0) {
                    std::cout << "关闭连接错误: " << uv_strerror(status) << std::endl;
                }
            });
        }
        
        // 等待一小段时间确保连接关闭
        loop.post([connections, &loop]() {
            std::cout << "清理 " << connections->size() << " 个连接..." << std::endl;
            connections->clear();  // 清理所有连接
            loop.stop();
        }); });
    loop.run();
}

// TCP客户端示例函数 - 从命令行读取数据
void tcp_client_example() {
    std::cout << "启动TCP客户端..." << std::endl;

    EventLoop loop;
    auto client = std::make_shared<TcpClientHandle>(loop);

    // 命令行输入处理
    struct InputHandler {
        std::shared_ptr<TcpClientHandle> client;
        std::thread input_thread;
        bool running {true};

        explicit InputHandler(std::shared_ptr<TcpClientHandle> c) : client(c) {}

        void start() {
            input_thread = std::thread([this]() {
                std::string input;
                std::cout << "请输入要发送的消息 (输入'quit'退出):" << std::endl;

                while (running) {
                    std::getline(std::cin, input);

                    if (input == "quit") {
                        running = false;
                        break;
                    }

                    if (!input.empty()) {
                        // 使用lambda捕获input的副本，确保线程安全
                        client->get_loop().post([client = client, input_copy = input]() {
                            client->send(input_copy.c_str(), input_copy.size());
                            std::cout << "已发送: " << input_copy << std::endl;
                        });
                    }
                }

                // 退出程序
                client->get_loop().post([]() {
                    // 通知libuv退出事件循环
                    uv_stop(uv_default_loop());
                });
            });
        }

        ~InputHandler() {
            running = false;
            if (input_thread.joinable()) {
                input_thread.join();
            }
        }
    };

    // 设置连接回调
    client->connect("127.0.0.1", 8080, [&client](bool success) {
        if (success) {
            std::cout << "成功连接到服务器" << std::endl;
        } else {
            std::cout << "连接服务器失败" << std::endl;
        }
    });

    // 设置数据回调
    client->set_data_callback([](const char* data, ssize_t len) {
        std::string message(data, len);
        std::cout << "收到服务器响应: " << message << std::endl;
    });

    // 设置错误回调
    client->set_error_callback([](int status) { std::cout << "客户端错误: " << uv_strerror(status) << std::endl; });

    // 启动输入处理线程
    InputHandler input_handler(client);
    input_handler.start();

    // 运行事件循环
    loop.run();
}

// UDP 服务器示例
void udp_server_example() {
    EventLoop loop;

    auto server = std::make_shared<UdpServerHandle>(loop);

    // 设置错误回调
    server->set_error_callback([](int status) { std::cout << "UDP错误: " << uv_strerror(status) << std::endl; });

    // 绑定地址
    if (server->bind("0.0.0.0", 8081)) {
        std::cout << "UDP服务器启动在0.0.0.0:8081" << std::endl;

        // 开始接收数据
        server->start_recv([server](const char* data, ssize_t len, const struct sockaddr* addr) {
            std::string message(data, len);
            std::cout << "收到UDP消息: " << message << std::endl;

            // 获取发送者的地址
            struct sockaddr_in* client_addr = (struct sockaddr_in*)addr;
            char ip[17] = {0};
            uv_ip4_name(client_addr, ip, 16);

            // 响应客户端
            std::string response = "UDP服务器已收到消息: " + message;
            server->send(response.c_str(), response.length(), addr);
        });

        loop.run();
    } else {
        std::cout << "UDP服务器启动失败" << std::endl;
    }
}

// UDP 客户端示例
void udp_client_example() {
    EventLoop loop;

    auto client = std::make_shared<UdpClientHandle>(loop);

    // 设置错误回调
    client->set_error_callback([](int status) { std::cout << "UDP错误: " << uv_strerror(status) << std::endl; });

    // 连接到服务器（实际上只是保存地址）
    if (client->connect("127.0.0.1", 8081)) {
        // 开始接收响应
        client->start_recv([](const char* data, ssize_t len, const struct sockaddr* addr) {
            (void)addr;
            std::string message(data, len);
            std::cout << "收到UDP服务器响应: " << message << std::endl;
        });

        // 发送消息
        const char* message = "Hello, UDP Server!";
        client->send(message, strlen(message));

        // 运行一段时间
        std::thread([&loop]() {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            loop.stop();
        }).detach();

        loop.run();
    } else {
        std::cout << "UDP客户端初始化失败" << std::endl;
    }
}

int main() {
    std::cout << "请选择测试示例:" << std::endl;
    std::cout << "1. TCP 服务器" << std::endl;
    std::cout << "2. TCP 客户端" << std::endl;
    std::cout << "3. UDP 服务器" << std::endl;
    std::cout << "4. UDP 客户端" << std::endl;

    int choice;
    std::cin >> choice;

    switch (choice) {
    case 1:
        tcp_server_example();
        break;
    case 2:
        tcp_client_example();
        break;
    case 3:
        udp_server_example();
        break;
    case 4:
        udp_client_example();
        break;
    default:
        std::cout << "无效选择" << std::endl;
        break;
    }

    return 0;
}