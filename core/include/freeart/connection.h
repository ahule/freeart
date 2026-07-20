#pragma once
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <sys/socket.h>

namespace freeart {

template<typename T>
class connection {
public:
    static constexpr size_t BUF_SIZE = 65536;

    explicit connection(int fd) : fd_(fd), bytes_received_(0) {}

    connection(const connection&)            = delete;
    connection& operator=(const connection&) = delete;

    bool send(const void* data, size_t len) {
        std::lock_guard<std::mutex> lk(send_mutex_);
        const uint8_t* ptr = static_cast<const uint8_t*>(data);
        size_t remaining   = len;
        while (remaining > 0) {
            ssize_t n = ::send(fd_, ptr, remaining, MSG_NOSIGNAL);
            if (n <= 0) return false;
            ptr       += n;
            remaining -= static_cast<size_t>(n);
        }
        return true;
    }

    int    fd()             const { return fd_; }
    size_t& bytes_received()      { return bytes_received_; }
    uint8_t* buf()                { return buf_; }

    T&       context()       { return context_; }
    const T& context() const { return context_; }

private:
    int         fd_;
    std::mutex  send_mutex_;
    T           context_{};
    uint8_t     buf_[BUF_SIZE];
    size_t      bytes_received_;
};

} // namespace freeart
