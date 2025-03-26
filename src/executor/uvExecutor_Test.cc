
#include <iostream>
#include <utility>
#include <uvHandle.hpp>

int main() {
    Loop loop;
    TimerHandle timer(loop);

    timer.start(1000, 1000, [](uv_timer_t* handle) {
        static int count = 0;
        std::cout << "Timer triggered: " << ++count << std::endl;
        if (count >= 5) {
            uv_timer_stop(handle); // 停止定时器
        }
    });

    // 运行事件循环，直到没有活动句柄
    loop.run();

    return 0;
}