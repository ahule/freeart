#include "fcp.h"
#include <freeart/utils.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

static std::atomic<bool> g_connected{false};
static const char* SERVER_IP   = "127.0.0.1";
static const int   SERVER_PORT = 8888;

static void recv_loop(int sockfd) {
    while (g_connected) {
        packethdr hdr{};
        char* payload = fcp_recv(sockfd, hdr);

        if (payload == nullptr || hdr.type == EXIT) {
            std::cout << "\n[disconnected]\n";
            g_connected = false;
            delete[] payload;
            break;
        }

        std::string sender(hdr.sender[0] ? hdr.sender : "anon");
        std::cout << "\r[" << sender << "]: " << payload << "\nclient: " << std::flush;
        delete[] payload;
    }
}

int main(int argc, char* argv[]) {
    const char* ip   = argc > 1 ? argv[1] : SERVER_IP;
    int         port = argc > 2 ? std::stoi(argv[2]) : SERVER_PORT;

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) { std::cerr << "socket failed\n"; return 1; }

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(static_cast<uint16_t>(port));
    inet_pton(AF_INET, ip, &addr.sin_addr);

    if (connect(sockfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == -1) {
        std::cerr << "connect failed\n";
        close(sockfd);
        return 1;
    }
    std::cout << "connected to " << ip << ':' << port << '\n';

    while (true) {
        std::string pwd;
        std::cout << "password: ";
        std::getline(std::cin, pwd);
        if (pwd == "exit") { close(sockfd); return 0; }

        fcp_send(sockfd, LOGIN, pwd);

        int status = 0;
        if (!freeart::recv_all(sockfd, &status, sizeof(status))) {
            std::cerr << "connection error\n";
            close(sockfd);
            return 1;
        }
        if (status == -1) {
            std::cout << "wrong password, try again\n";
            continue;
        }
        std::cout << "logged in\n";
        g_connected = true;
        break;
    }

    std::thread(recv_loop, sockfd).detach();

    while (g_connected) {
        std::string msg;
        std::cout << "client: " << std::flush;
        if (!std::getline(std::cin, msg)) break;
        if (msg.empty()) continue;
        if (msg == "exit") break;
        fcp_send(sockfd, MESSAGE, msg);
    }

    close(sockfd);
    return 0;
}
