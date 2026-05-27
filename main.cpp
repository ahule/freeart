#include <iostream>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <cstring>
#include <unistd.h>
#include <thread>
#include <vector>
#include <sstream>
#include <mutex>

using namespace std;


class server {
private:
    struct packethdr { // FCP : Freeart Chat Protocol
        int type;   // 1: Login, 2: Message, 3: Exit
        int length;
        char magic[3];
        char version[3]; // "1.01"
        char sender[16];
    };
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    const char* SERVER_PASSWORD = "password";
    int fail = -1;
    int succ = 0;

    vector<int> active_clients;
    mutex clients_mutex;

    bool recv_all(int socket, void* buffer, size_t length) {
        char* ptr = static_cast<char*>(buffer);
        while (length > 0) {
            int bytes = recv(socket, ptr, length, 0);
            if (bytes <= 0) { return false; }
            ptr += bytes;
            length -= bytes;
        }
        return true;
    }

    bool check_sockfd(int socket) {
        for (auto i : active_clients) {
            if (i == socket) {
                return true;
            }
        }
        return false;
    }

    template<typename container>
    void print_container(const container& c, char delimiter, bool numbering) {
        if (numbering) {
            int j = 1;
            for (const auto& i : c) {
                cout << j << ". " << i << delimiter;
                j++;
            }
        }else {
            for (const auto& i : c) {
                cout << i << delimiter;
            }
        }
    }

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

            if (!recv_all(client_socket, &header, sizeof(header))) return;

            if (strncmp(header.magic, "FCP", 3) != 0) {
                close(client_socket);
                return;
            }

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
                        lock_guard<mutex> lock(clients_mutex);
                        active_clients.push_back(client_socket);
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

            if (strncmp(header.magic, "FCP", 3) != 0) {
                break;
            }

            if (header.length > 0) {
                char* buffer = new char[header.length + 1];
                int rmsg = recv(client_socket, buffer, header.length, 0);

                if (rmsg > 0) {
                    buffer[rmsg] = '\0';
                    cout << "[" << client_socket << "]: " << buffer << endl;
                    cout << "Command: " << flush;
                }
                delete[] buffer;
            }
        }

        {
            lock_guard<mutex> lock(clients_mutex);
            for (auto it = active_clients.begin(); it != active_clients.end(); ++it) {
                if (*it == client_socket) {
                    active_clients.erase(it);
                    break;
                }
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
                    cout << "[Server Error]: Socket ID Not Found. Usage: disconnect <socket_id>" << endl;
                    continue;
                };
                close(client_socket);
                cout << "[Server]: disconnecting client on socket (" << client_socket <<")" << endl;
            }else if (cmds[0] == "list") {
                if (active_clients.empty()) {
                    cout << "[Server]: No active clients." << endl;
                }else {
                    print_container(active_clients, '\n', true);
                }
            }
        }
    }
public:
    server() {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
    }
    void server_connection() {
        if (sockfd != -1) {
            cout << "Create Socket Success." << endl;
        }else {
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

        if (bind(sockfd, (struct sockaddr*) &saddr, sizeof(saddr)) == 0) {
            cout << "Bind Success." << endl;
        }else {
            cout << "Bind Failed." << endl;
            return;
        }

        if (listen(sockfd, 10) == 0) {
            cout << "Listening..." << endl;
        }else {
            cout << "Listening Failed." << endl;
            return;
        }
    }

    void server_main() {
        struct sockaddr_in client_saddr;
        socklen_t len = sizeof(client_saddr);

        thread(&server::admin_panel, this).detach();
        cout << "Command: ";
        while (true) {
            int client_socket = accept(sockfd, (struct sockaddr*)&client_saddr, &len);
            if (client_socket != -1) {
                cout << "New Client Connected! Socket FD: " << client_socket << endl;
                thread(&server::handle_client, this, client_socket).detach();
            }
        }
    }

    ~server() {
        if (sockfd != -1) {
            close(sockfd);
        }
    }

server(const server&) = delete;
server& operator=(const server&) = delete;
};

int main() {
    server s;
    s.server_connection();
    s.server_main();
    return 0;
}