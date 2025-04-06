#include <iostream>
#include <utility>
#include "uvExecutor.hpp"
#include "uvHandle.hpp"

int main() {
    EventLoop loop;
    TimerHandle timer(loop);

    timer.start(1000, 1000, [&timer]() {
        static int count = 0;
        std::cout << "Timer triggered: " << ++count << std::endl;
        if (count >= 5) {
            timer.stop(); // 停止定时器
        }
    });

    SignalHandlePtr signal = std::make_shared<SignalHandle>(loop);
    signal->start(SIGINT, [&loop](int) {
        // std::cout << "Received SIGINT, stopping..." << std::endl;
        loop.stop();
    });
    // 运行事件循环，直到没有活动句柄
    loop.run();

    return 0;
}