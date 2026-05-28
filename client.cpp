#include <cstring>
#include <iostream>
#include <thread>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string>
#include "fcp.h"

using namespace std;

int login = false;
string SERVER_IP = "127.0.0.1";

void recv_chat(int sockfd) {
    while (true) {
        packethdr header;
        char* buffer = recv_packet(sockfd, header);
        if (buffer == nullptr) {
            continue;
        }
        
        cout << "\r[" << header.sender << "]: " << buffer << "\nclient: " << flush;
        delete[] buffer;
    }
}

int main() {
    // create socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd != -1) {
        cout << "Create Socket Success." << endl;
    }else {
        cout << "Socket Creation Failed." << endl;
        return 1;
    }

    // connect the client to server
    struct sockaddr_in sock_addr;
    memset(&sock_addr, 0, sizeof(sock_addr));

    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = inet_addr(SERVER_IP.c_str());
    sock_addr.sin_port = htons(8888);

    if (connect(sockfd, (struct sockaddr*) &sock_addr, sizeof(sock_addr)) != -1) {
        cout << "Connect Success." << endl;
    }else {
        cout << "Connect Failed." << endl;
        return 1;
    }

    // login
    while (true) {
        string msg = "";
        cout << "Login: ";
        getline(cin, msg);
        if (msg == "exit") {
            break;
        }

        send_packet(sockfd, 1, msg);

        int login_status = 0;
        int login_recv = recv(sockfd, &login_status, sizeof(login_status), 0);
        if (login_recv == sizeof(login_status)) {
            if (login_status == -1) {
                cout << "Login Failed. Try again." << endl;
            } else {
                cout << "Login Success!" << endl;
                login = true;
                break;
            }
        } else {
            cout << "Connection error during login." << endl;
            break;
        }
    }

    // send messages
    if (login) {
        thread(recv_chat, sockfd).detach();
    }
    while (login) {
        string msg = "";
        cout << "client: " << flush;
        getline(cin, msg);
        if (msg.empty()) continue;

        send_packet(sockfd, 2, msg);
    }

    close(sockfd);
    return 0;
}