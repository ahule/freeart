#include <iostream>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <cstring>
#include <unistd.h>
#include <thread>
#include <vector>
#include <sstream>

using namespace std;

struct packethdr { // FCP : Freeart Chat Protocol
    int type;   // 1: Login, 2: Message, 3: Exit
    int length;
};
const char* SERVER_PASSWORD = "password";
int fail = -1;
int succ = 0;

vector<string> strcut(const string& str, char delimiter) {
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

void handle_client(int client_socket) {
    bool login = false;

    while (!login) {
        packethdr header;

        int bytes = recv(client_socket, &header, sizeof(header), 0);
        if (bytes <= 0) return;

        if (header.type == 1 && header.length > 0) {
            char* loginbfr = new char[header.length + 1];
            int rmsg = recv(client_socket, loginbfr, header.length, 0);

            if (rmsg > 0) {
                loginbfr[rmsg] = '\0';

                if (strcmp(loginbfr, SERVER_PASSWORD) != 0) {
                    cout << "[Login] Client (" << client_socket << ") failed password check." << endl;
                    send(client_socket, &fail, sizeof(fail), 0);
                } else {
                    cout << "[Login] Client (" << client_socket << ") authenticated successfully!" << endl;
                    send(client_socket, &succ, sizeof(succ), 0);
                    login = true;
                }
            }

            delete[] loginbfr;
        } else {
            send(client_socket, &fail, sizeof(fail), 0);
        }
    }

    while (login) {
        packethdr header;
        int bytes = recv(client_socket, &header, sizeof(header), 0);
        if (bytes <= 0) {
            break;
        }

        if (header.length > 0) {
            char* buffer = new char[header.length + 1];
            int rmsg = recv(client_socket, buffer, header.length, 0);

            if (rmsg > 0) {
                buffer[rmsg] = '\0';
                cout << "[" << client_socket << "]: " << buffer << endl;
            }
            delete[] buffer;
        }
    }
    close(client_socket);
    cout << "Client [" << client_socket << "] disconnected cleanly." << endl;
}

void admin_panel() {
    string command;
    while (true) {

        getline(cin, command);
        vector<string> cmds = strcut(command, ' ');
        if (cmds.size() == 0) {
            return;
        }

        int client_socket = stoi(cmds[1]);

        if (cmds[0] == "disconnect") {
            close(client_socket);
            cout << "[Server]: disconnecting client on socket (" << client_socket <<")" << endl;
        }
    }
}

int main() {

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd != -1) {
        cout << "Create Socket Success." << endl;
    }else {
        cout << "Socket Creation Failed." << endl;
        return 1;
    }

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));

    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = INADDR_ANY;
    saddr.sin_port = htons(8888);

    if (bind(sockfd, (struct sockaddr*) &saddr, sizeof(saddr)) == 0) {
        cout << "Bind Success." << endl;
    }else {
        cout << "Bind Failed." << endl;
        return 1;
    }

    if (listen(sockfd, 10) == 0) {
        cout << "Listening..." << endl;
    }else {
        cout << "Listening Failed." << endl;
        return 1;
    }

    struct sockaddr_in client_saddr;
    socklen_t len = sizeof(client_saddr);

    thread(admin_panel).detach();
    cout << "Command: ";
    while (true) {
        int client_socket = accept(sockfd, (struct sockaddr*)&client_saddr, &len);
        if (client_socket != -1) {
            cout << "New Client Connected! Socket FD: " << client_socket << endl;
            thread(handle_client, client_socket).detach();
        }
    }

    close(sockfd);
    return 0;
}