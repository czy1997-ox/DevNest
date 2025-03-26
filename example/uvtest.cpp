#include <uv.h>
#include <iostream>

int main() {
    uv_loop_t *loop = uv_default_loop();
    std::cout << "Libuv version: " << uv_version_string() << std::endl;
    uv_run(loop, UV_RUN_DEFAULT);
    uv_loop_close(loop);
    return 0;
}