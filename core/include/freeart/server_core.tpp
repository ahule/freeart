#pragma once
#include "server_core.h"
#include "utils.h"
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

namespace freeart {

template<typename T>
server_core<T>::server_core(server_config cfg, std::unique_ptr<ihandler<T>> handler)
    : cfg_(std::move(cfg)), handler_(std::move(handler)) {}

template<typename T>
server_core<T>::~server_core() {
    stop();
}

template<typename T>
void server_core<T>::setup_socket() {
    sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd_ == -1) return;

    int opt = 1;
    setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(cfg_.port);
    inet_pton(AF_INET, cfg_.host.c_str(), &addr.sin_addr);

    if (bind(sockfd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == -1) {
        close(sockfd_);
        sockfd_ = -1;
        return;
    }

    listen(sockfd_, cfg_.backlog);
}

template<typename T>
void server_core<T>::run() {
    setup_socket();
    if (sockfd_ == -1) return;

    running_ = true;

    int n = cfg_.thread_count > 0
        ? cfg_.thread_count
        : static_cast<int>(std::thread::hardware_concurrency());
    if (n == 0) n = 4;

    for (int i = 0; i < n; ++i)
        workers_.emplace_back(&server_core<T>::worker_loop, this);

    event_loop();

    for (auto& w : workers_)
        if (w.joinable()) w.join();
}

template<typename T>
void server_core<T>::stop() {
    running_ = false;
    queue_cv_.notify_all();
    if (sockfd_ != -1) {
        close(sockfd_);
        sockfd_ = -1;
    }
}

template<typename T>
void server_core<T>::event_loop() {
    int epoll_fd = epoll_create1(0);

    struct epoll_event ev{};
    ev.events  = EPOLLIN;
    ev.data.fd = sockfd_;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockfd_, &ev);

    std::vector<struct epoll_event> events(cfg_.max_events);

    while (running_) {
        int n = epoll_wait(epoll_fd, events.data(), cfg_.max_events, 500);
        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;
            if (fd == sockfd_) {
                accept_connection(epoll_fd);
            } else {
                {
                    std::lock_guard<std::mutex> lk(queue_mutex_);
                    task_queue_.push({fd, epoll_fd});
                }
                queue_cv_.notify_one();
            }
        }
    }

    close(epoll_fd);
}

template<typename T>
void server_core<T>::accept_connection(int epoll_fd) {
    int client_fd = accept(sockfd_, nullptr, nullptr);
    if (client_fd == -1) return;

    set_nonblocking(client_fd);

    auto conn = std::make_shared<connection<T>>(client_fd);
    {
        std::lock_guard<std::mutex> lk(connections_mutex_);
        connections_[client_fd] = conn;
    }

    struct epoll_event ev{};
    ev.events  = EPOLLIN | EPOLLONESHOT;
    ev.data.fd = client_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);

    handler_->on_connect(*this, *conn);
}

template<typename T>
void server_core<T>::close_connection(int client_fd, int epoll_fd) {
    auto conn = get_connection(client_fd);
    if (conn) handler_->on_disconnect(*this, *conn);

    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
    close(client_fd);

    std::lock_guard<std::mutex> lk(connections_mutex_);
    connections_.erase(client_fd);
}

template<typename T>
void server_core<T>::worker_loop() {
    while (running_) {
        work_task task{};
        {
            std::unique_lock<std::mutex> lk(queue_mutex_);
            queue_cv_.wait(lk, [this] {
                return !task_queue_.empty() || !running_;
            });
            if (!running_ && task_queue_.empty()) return;
            task = task_queue_.front();
            task_queue_.pop();
        }

        auto conn = get_connection(task.client_fd);
        if (!conn) continue;

        uint8_t* buf     = conn->buf();
        size_t& received = conn->bytes_received();
        size_t  space    = connection<T>::BUF_SIZE - received;

        if (space == 0) {
            close_connection(task.client_fd, task.epoll_fd);
            continue;
        }

        ssize_t n = recv(task.client_fd, buf + received, space, 0);

        if (n > 0) {
            received += static_cast<size_t>(n);
            int result = handler_->on_data(*this, *conn, buf, received);

            if (result == -1) {
                close_connection(task.client_fd, task.epoll_fd);
                continue;
            }

            if (result > 0 && static_cast<size_t>(result) <= received) {
                size_t consumed = static_cast<size_t>(result);
                received -= consumed;
                if (received > 0)
                    std::memmove(buf, buf + consumed, received);
            }

            struct epoll_event ev{};
            ev.events  = EPOLLIN | EPOLLONESHOT;
            ev.data.fd = task.client_fd;
            epoll_ctl(task.epoll_fd, EPOLL_CTL_MOD, task.client_fd, &ev);

        } else if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
            close_connection(task.client_fd, task.epoll_fd);
        } else {
            struct epoll_event ev{};
            ev.events  = EPOLLIN | EPOLLONESHOT;
            ev.data.fd = task.client_fd;
            epoll_ctl(task.epoll_fd, EPOLL_CTL_MOD, task.client_fd, &ev);
        }
    }
}

template<typename T>
void server_core<T>::broadcast(const void* data, size_t len, int exclude_fd) {
    std::lock_guard<std::mutex> lk(connections_mutex_);
    for (auto& [fd, conn] : connections_) {
        if (fd != exclude_fd)
            conn->send(data, len);
    }
}

template<typename T>
size_t server_core<T>::connection_count() const {
    std::lock_guard<std::mutex> lk(connections_mutex_);
    return connections_.size();
}

template<typename T>
std::shared_ptr<connection<T>> server_core<T>::get_connection(int fd) {
    std::lock_guard<std::mutex> lk(connections_mutex_);
    auto it = connections_.find(fd);
    if (it != connections_.end()) return it->second;
    return nullptr;
}

} // namespace freeart
