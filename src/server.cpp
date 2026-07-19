#include "server.h"
#include <iostream>
#include <cstring>
#include <thread>
#include <algorithm>
#include <sstream>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>

using namespace std;

server::server() {
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
}

server::~server() {
    server_running = false;
    pool_cv.notify_all();
    if (sockfd != -1) close(sockfd);
}

void server::broadcast_to_all(int sender_fd, const string& sender_name, const string& message) {
    lock_guard<mutex> lock(clients_mutex);
    for (int client_fd : active_clients) {
        if (client_fd != sender_fd) {
            send_packet(client_fd, MESSAGE, message, sender_name);
        }
    }
}

bool server::is_authenticated(int client_fd) {
    lock_guard<mutex> lock(clients_mutex);
    return find(active_clients.begin(), active_clients.end(), client_fd) != active_clients.end();
}

bool server::check_sockfd(int socket) {
    lock_guard<mutex> lock(clients_mutex);
    return find(active_clients.begin(), active_clients.end(), socket) != active_clients.end();
}

vector<string> server::strcut(const string& str, char delimiter) {
    vector<string> result;
    stringstream ss(str);
    string token;
    while (getline(ss, token, delimiter)) {
        if (!token.empty()) {
            result.push_back(token);
        }
    }
    return result;
}

void server::worker_thread_loop(int thread_id) {
    (void)thread_id;
    int fail = -1;
    int succ = 0;

    while (server_running) {
        code_task task;
        {
            unique_lock<mutex> lock(queue_mutex);
            pool_cv.wait(lock, [this] { return !task_queue.empty() || !server_running; });

            if (!server_running && task_queue.empty()) return;

            task = task_queue.front();
            task_queue.pop();
        }

        char* buffer = nullptr;
        bool should_close = false;
        int result = 0;
        packethdr current_header;

        std::shared_ptr<client_session> session = nullptr;
        {
            lock_guard<mutex> session_lock(sessions_mutex);
            if (client_sessions.find(task.clientfd) != client_sessions.end()) {
                session = client_sessions[task.clientfd];
            }
        }
        if (session != nullptr) {
            lock_guard<mutex> client_lock(session->mutex);

            session->fd = task.clientfd;
            result = recv_packet_nonblocking(*session, buffer);

            if (result == 1 && buffer != nullptr) {
                current_header = session->header;

                // session->headerbuf.clear();
                // session->bodybuf.clear();
                // delete[] session->buf;
                // session->buf = nullptr;

                session->header_ready = false;
            }
            else if (result == -1) {
                should_close = true;
            }
        }

        if (result == 1 && buffer != nullptr && !should_close) {
            if (!is_authenticated(task.clientfd)) {
                if (current_header.type == 1 && strcmp(buffer, SERVER_PASSWORD) == 0) {
                    cout << "[Login] Client (" << task.clientfd << ") authenticated successfully!" << endl;
                    send(task.clientfd, &succ, sizeof(succ), 0);
                    {
                        lock_guard<mutex> lock(clients_mutex);
                        active_clients.push_back(task.clientfd);
                    }
                } else {
                    cout << "[Login] Client (" << task.clientfd << ") failed password check. Disconnecting..." << endl;
                    send(task.clientfd, &fail, sizeof(fail), 0);
                    should_close = true;
                }
            }
            else {
                string sender(current_header.sender);
                if (sender.empty()) sender = "Anonymous";

                cout << "[" << sender << ":" << task.clientfd << "]: " << buffer << endl;
                broadcast_to_all(task.clientfd, sender, buffer);
            }

            delete[] buffer;
        }

        if (should_close) {
            {
                lock_guard<mutex> lock(clients_mutex);
                active_clients.erase(remove(active_clients.begin(), active_clients.end(), task.clientfd), active_clients.end());
            }
            {
                lock_guard<mutex> lock(sessions_mutex);
                client_sessions.erase(task.clientfd);
            }
            epoll_ctl(task.epollfd, EPOLL_CTL_DEL, task.clientfd, nullptr);
            close(task.clientfd);

            // send exit message to client
            send_packet(task.clientfd, EXIT, "EXIT", "SERVER");

            cout << "[Server] Client [" << task.clientfd << "] disconnected/cleared." << endl;
            cout << "Command: " << flush;
        }
        else {
            struct epoll_event ev;
            ev.events = EPOLLIN | EPOLLONESHOT;
            ev.data.fd = task.clientfd;
            epoll_ctl(task.epollfd, EPOLL_CTL_MOD, task.clientfd, &ev);
        }
    }
}

void server::admin_panel() {
    string command;
    cout << "Command: " << flush;
    while (server_running) {
        if (!getline(cin, command)) break;
        vector<string> cmds = strcut(command, ' ');
        if (cmds.empty()) {
            cout << "Command: " << flush;
            continue;
        }

        if (cmds[0] == "disconnect") {
            if (cmds.size() < 2) {
                cout << "[Server Error]: Missing socket ID. Usage: disconnect <socket_id>" << endl;
                cout << "Command: " << flush;
                continue;
            }
            int client_socket = stoi(cmds[1]);
            if (!check_sockfd(client_socket)) {
                cout << "[Server Error]: Socket ID Not Found." << endl;
                cout << "Command: " << flush;
                continue;
            }

            {
                lock_guard<mutex> lock(clients_mutex);
                active_clients.erase(remove(active_clients.begin(), active_clients.end(), client_socket), active_clients.end());
            }
            {
                lock_guard<mutex> lock(sessions_mutex);
                client_sessions.erase(client_socket);
            }
            close(client_socket);
            cout << "[Server]: disconnecting client on socket (" << client_socket << ")" << endl;
        }
        else if (cmds[0] == "list") {
            lock_guard<mutex> lock(clients_mutex);
            if (active_clients.empty()) {
                cout << "[Server]: No active clients." << endl;
            } else {
                print_container(active_clients, '\n', true);
            }
        }
        cout << "Command: " << flush;
    }
}

void server::server_connection() {
    if (sockfd != -1) {
        cout << "Create Socket Success." << endl;
    } else {
        cout << "Socket Creation Failed." << endl;
        return;
    }

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = INADDR_ANY;
    saddr.sin_port = htons(8888);

    if (bind(sockfd, (struct sockaddr*)&saddr, sizeof(saddr)) == 0) {
        cout << "Bind Success." << endl;
    } else {
        cout << "Bind Failed." << endl;
        return;
    }

    if (listen(sockfd, 10) == 0) {
        cout << "Listening..." << endl;
    } else {
        cout << "Listening Failed." << endl;
        return;
    }
}

void server::server_main() {
    thread(&server::admin_panel, this).detach();

    vector<thread> thread_pool;
    for (int i = 0; i < THREAD_POOL_SIZE; ++i) {
        thread_pool.emplace_back(&server::worker_thread_loop, this, i);
    }

    int epoll_fd = epoll_create1(0);
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = sockfd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockfd, &ev);

    struct epoll_event events[MAX_EVENTS];

    while (server_running) {
        int ready_fds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

        for (int i = 0; i < ready_fds; ++i) {
            int active_fd = events[i].data.fd;

            if (active_fd == sockfd) {
                int client_socket = accept(sockfd, nullptr, nullptr);
                if (client_socket == -1) continue;
                set_nonblocking(client_socket);

                struct epoll_event client_ev;
                client_ev.events = EPOLLIN | EPOLLONESHOT;
                client_ev.data.fd = client_socket;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_socket, &client_ev);

                {
                    lock_guard<mutex> lock(sessions_mutex);
                    client_sessions[client_socket] = std::make_shared<client_session>();
                    client_sessions[client_socket]->fd = client_socket;
                }

                cout << "[Epoll] A new connection has been accepted ("<< client_socket << ")..." << endl;
                cout << "Command: " << flush;
            }
            else {
                {
                    lock_guard<mutex> lock(queue_mutex);
                    task_queue.push({active_fd, epoll_fd});
                }
                pool_cv.notify_one();
            }
        }
    }

    for (auto& th : thread_pool) {
        if (th.joinable()) th.join();
    }
}