#pragma once
#include "ihandler.h"
#include "connection.h"
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <atomic>

namespace freeart {

struct server_config {
    uint16_t    port         = 8888;
    std::string host         = "0.0.0.0";
    int         backlog      = 512;
    int         thread_count = 0;
    int         max_events   = 256;
};

struct work_task {
    int client_fd;
    int epoll_fd;
};

template<typename T>
class server_core {
public:
    server_core(server_config cfg, std::unique_ptr<ihandler<T>> handler);
    ~server_core();

    server_core(const server_core&)            = delete;
    server_core& operator=(const server_core&) = delete;

    void   run();
    void   stop();
    void   broadcast(const void* data, size_t len, int exclude_fd = -1);
    size_t connection_count() const;

    std::shared_ptr<connection<T>> get_connection(int fd);

private:
    void setup_socket();
    void event_loop();
    void worker_loop();
    void accept_connection(int epoll_fd);
    void close_connection(int client_fd, int epoll_fd);

    server_config               cfg_;
    std::unique_ptr<ihandler<T>> handler_;
    int                         sockfd_ = -1;
    std::atomic<bool>           running_{false};

    std::unordered_map<int, std::shared_ptr<connection<T>>> connections_;
    mutable std::mutex                                       connections_mutex_;

    std::queue<work_task>    task_queue_;
    std::mutex               queue_mutex_;
    std::condition_variable  queue_cv_;
    std::vector<std::thread> workers_;
};

} // namespace freeart

#include "server_core.tpp"
