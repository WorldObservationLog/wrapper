/*
 * main.cpp — main.c 中 socket 处理器的 C++ 异常屏障
 * handle() 会调用可能抛出 SVError 的 C++ 函数 (如 getPersistentKey),
 * 这里用 extern "C" 包装并捕获异常, 避免 C++ 异常穿过 C 边界导致崩溃。
 * 每个新 socket 服务 (decrypt/m3u8/key/account) 都经 *_cpp 分发。
 */
#include <cstdio>
#include <cstring>
#include <exception>
#include <functional>
#include <unistd.h>

extern "C" void handle(int fd);

extern "C" uint8_t handle_cpp(int fd) {
    try {
        handle(fd);
        return 1;
    } catch (const std::exception &e) {
        fprintf(stderr, "[!] catched an exception: %s\n", e.what());
        return 0;
    }
}

extern "C" void handle_key_request(int fd);

/* C++ exception barrier for the HTTP key service (getPersistentKey can throw SVError). */
extern "C" uint8_t handle_key_request_cpp(int fd) {
    try {
        handle_key_request(fd);
        return 1;
    } catch (const std::exception &e) {
        fprintf(stderr, "[!] key request exception: %s\n", e.what());
        const char *resp = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: application/json\r\nContent-Length: 28\r\nConnection: close\r\n\r\n{\"error\":\"key retrieval failed\"}";
        write(fd, resp, strlen(resp));
        return 0;
    }
}

static void endLeaseCb(int const &c) {
    fprintf(stderr, "[.] end lease code %d\n", c);
    exit(1);
}

static void pbErrCb(void *) {
    fprintf(stderr, "[.] playback error\n");
    exit(1);
}

extern "C" std::function<void (int const&)> endLeaseCallback(endLeaseCb);
extern "C" std::function<void (void *)> pbErrCallback(pbErrCb);
