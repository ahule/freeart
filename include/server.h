#ifndef SERVER_H
#define SERVER_H

#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <string>
#include "fcp.h"

struct code_task {
    int clientfd;
    int epollfd;
};

class server {
private:
    int sockfd = -1;
    const int MAX_EVENTS = 64;
    const int THREAD_POOL_SIZE = 5;
    const char* SERVER_PASSWORD = "password";

    std::vector<int> active_clients;
    std::mutex clients_mutex;

    std::queue<code_task> task_queue;
    std::mutex queue_mutex;
    std::condition_variable pool_cv;
    bool server_running = true;

    std::unordered_map<int, client_session> client_sessions;
    std::mutex sessions_mutex;

    void worker_thread_loop(int thread_id);
    void broadcast_to_all(int sender_fd, const std::string& sender_name, const std::string& message);
    bool is_authenticated(int client_fd);
    bool check_sockfd(int socket);
    void admin_panel();
    std::vector<std::string> strcut(const std::string& str, char delimiter);

    template<typename container>
    void print_container(const container& c, char delimiter, bool numbering) {
        if (numbering) {
            int j = 1;
            for (const auto& i : c) {
                std::cout << j << ". " << i << delimiter;
                j++;
            }
        } else {
            for (const auto& i : c) {
                std::cout << i << delimiter;
            }
        }
    }

public:
    server();
    ~server();
    void server_connection();
    void server_main();

    server(const server&) = delete;
    server& operator=(const server&) = delete;
};

#endif // SERVER_H