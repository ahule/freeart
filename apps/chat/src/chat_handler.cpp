#include "chat_handler.h"
#include <iostream>
#include <cstring>
#include <vector>

chat_handler::chat_handler(std::string password) : password_(std::move(password)) {}

void chat_handler::on_connect(freeart::server_core<fcp_session>& server, freeart::connection<fcp_session>& conn) {
    (void)server;
    std::cout << "[+] connection accepted fd=" << conn.fd() << '\n';
}

int chat_handler::on_data(freeart::server_core<fcp_session>& server, freeart::connection<fcp_session>& conn, const uint8_t* data, size_t len) {
    (void)data;
    (void)len;

    fcp_session& session = conn.context();
    char* payload        = nullptr;

    int consumed = fcp_parse(conn.buf(), conn.bytes_received(), session, payload);

    if (consumed == -1) return -1;
    if (consumed == 0)  return 0;

    if (!is_authenticated(conn.fd())) {
        if (session.header.type == LOGIN && payload != nullptr && std::string(payload) == password_) {
            int ok = 0;
            conn.send(&ok, sizeof(ok));
            {
                std::lock_guard<std::mutex> lk(auth_mutex_);
                authenticated_.insert(conn.fd());
            }
            size_t ulen = std::min(strlen(session.header.sender), sizeof(session.username) - 1);
            memcpy(session.username, session.header.sender, ulen);
            std::cout << "[login] fd=" << conn.fd() << " authenticated\n";
        } else {
            int fail = -1;
            conn.send(&fail, sizeof(fail));
            delete[] payload;
            return -1;
        }
    } else {
        std::string sender(session.username);
        if (sender.empty()) sender = "anon";
        std::cout << '[' << sender << ':' << conn.fd() << "]: " << payload << '\n';
        broadcast_message(server, conn.fd(), sender, payload);
    }

    delete[] payload;
    return consumed;
}

void chat_handler::on_disconnect(freeart::server_core<fcp_session>& server, freeart::connection<fcp_session>& conn) {
    (void)server;
    {
        std::lock_guard<std::mutex> lk(auth_mutex_);
        authenticated_.erase(conn.fd());
    }
    std::cout << "[-] fd=" << conn.fd() << " disconnected\n";
}

void chat_handler::broadcast_message(freeart::server_core<fcp_session>& server, int sender_fd, const std::string& sender_name, const std::string& msg) {
    size_t total = sizeof(packethdr) + msg.size();
    std::vector<uint8_t> pkt(total);
    packethdr hdr{};
    hdr.type   = MESSAGE;
    hdr.length = static_cast<int>(msg.size());
    memcpy(hdr.magic,   "FCP", 3);
    memcpy(hdr.version, "1.0", 3);
    size_t slen = std::min(sender_name.size(), sizeof(hdr.sender) - 1);
    memcpy(hdr.sender, sender_name.c_str(), slen);
    hdr.checksum = fcp_checksum(msg.c_str(), msg.size());
    memcpy(pkt.data(), &hdr, sizeof(hdr));
    memcpy(pkt.data() + sizeof(hdr), msg.c_str(), msg.size());

    server.broadcast(pkt.data(), pkt.size(), sender_fd);
}

bool chat_handler::is_authenticated(int fd) {
    std::lock_guard<std::mutex> lk(auth_mutex_);
    return authenticated_.count(fd) > 0;
}
