#include "fcp.h"
#include <cstring>
#include <cerrno>
#include <cstdlib>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <algorithm>

using namespace std;

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags != -1) {
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
}

unsigned int checksum(const void* data, size_t length) {
    const uint16_t* ptr = static_cast<const uint16_t*>(data);
    uint32_t sum = 0;

    while (length > 1) {
        sum += *ptr++;
        length -= 2;
    }
    if (length > 0) {
        sum += *reinterpret_cast<const uint8_t*>(ptr);
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return static_cast<uint16_t>(~sum);
}

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

void send_packet(int sockfd, int packet_type, const string& message, const string& sender) {
    packethdr header{};

    header.type = packet_type;
    header.length = message.length();
    memcpy(header.magic, "FCP", 3);
    memcpy(header.version, "1.0", 3);
    if (!sender.empty()) {
        size_t len = std::min(sender.length(), sizeof(header.sender) - 1);
        memcpy(header.sender, sender.c_str(), len);
    }

    if (header.length > 0) {
        header.checksum = checksum(message.c_str(), header.length);
    } else {
        header.checksum = 0;
    }

    size_t total_size = sizeof(header) + header.length;
    char* sendbuf = new char[total_size];
    memcpy(sendbuf, &header, sizeof(header));
    if (header.length > 0) {
        memcpy(sendbuf + sizeof(header), message.c_str(), header.length);
    }

    send(sockfd, sendbuf, total_size, 0);
    delete[] sendbuf;
}

char* recv_packet(int sockfd, packethdr& header) {
    if (!recv_all(sockfd, &header, sizeof(header))) {
        return nullptr;
    }
    if (header.magic[0] != 'F' || header.magic[1] != 'C' || header.magic[2] != 'P' || header.length < 0) {
        return nullptr;
    }
    if (header.length > 0) {
        char* buffer = new char[header.length + 1];
        if (recv_all(sockfd, buffer, header.length)) {
            buffer[header.length] = '\0';
            if (header.checksum == checksum(buffer, header.length)) {
                return buffer;
            }
            delete[] buffer;
        }
    }
    return nullptr;
}

int recv_packet_nonblocking(client_session& session, char*& out_buffer) {
    out_buffer = nullptr;

    if (!session.header_ready) {
        size_t current_size = session.headerbuf.size();
        size_t bytes_needed = sizeof(packethdr) - current_size;

        vector<char> temp_buf(bytes_needed);
        int bytes_recv = recv(session.fd, temp_buf.data(), bytes_needed, 0);

        if (bytes_recv > 0) {
            session.headerbuf.insert(session.headerbuf.end(), temp_buf.begin(), temp_buf.begin() + bytes_recv);

            if (session.headerbuf.size() == sizeof(packethdr)) {
                memcpy(&session.header, session.headerbuf.data(), sizeof(packethdr));

                if (session.header.magic[0] != 'F' || session.header.magic[1] != 'C' || session.header.magic[2] != 'P' || session.header.length < 0) {
                    return -1;
                }
                session.header_ready = true;
            }
        }
        else if (bytes_recv == 0) {
            return -1;
        }
        else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return 0;
            }
            return -1;
        }
    }

    if (session.header_ready) {
        if (session.header.length == 0) {
            out_buffer = new char[1];
            out_buffer[0] = '\0';
            return 1;
        }

        size_t current_body_size = session.bodybuf.size();
        size_t body_bytes_needed = static_cast<size_t>(session.header.length) - current_body_size;

        vector<char> temp_body(body_bytes_needed);
        int bytes_recv = recv(session.fd, temp_body.data(), body_bytes_needed, 0);

        if (bytes_recv > 0) {
            session.bodybuf.insert(session.bodybuf.end(), temp_body.begin(), temp_body.begin() + bytes_recv);
        }
        else if (bytes_recv < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return 0;
        }
        else {
            return -1;
        }

        if (session.bodybuf.size() == static_cast<size_t>(session.header.length)) {
            out_buffer = new char[session.header.length + 1];
            memcpy(out_buffer, session.bodybuf.data(), session.header.length);
            out_buffer[session.header.length] = '\0';

            if (session.header.checksum == checksum(out_buffer, session.header.length)) {
                return 1;
            } else {
                delete[] out_buffer;
                out_buffer = nullptr;
                return -1;
            }
        }
    }

    return 0;
}

fcp_memory_pool::fcp_memory_pool(size_t chunk_size, size_t chunksnum) {
    if (chunk_size >= sizeof(node*)) {
        this->chunk_size = chunk_size;
    } else {
        this->chunk_size = sizeof(node*);
    }

    pool_size = this->chunk_size * chunksnum;
    pool_start = malloc(pool_size);
    free_list = static_cast<node*>(pool_start);

    node* current = free_list;
    for (size_t i = 1; i < chunksnum; ++i) {
        current->next = reinterpret_cast<node*>(static_cast<char*>(pool_start) + (i * this->chunk_size));
        current = current->next;
    }
    current->next = nullptr;
}

fcp_memory_pool::~fcp_memory_pool() {
    free(pool_start);
}

void* fcp_memory_pool::allocate() {
    if (!free_list) return nullptr;
    node* current = free_list;
    free_list = free_list->next;
    return current;
}

void fcp_memory_pool::deallocate(void* ptr) {
    if (!ptr) return;
    node* current = static_cast<node*>(ptr);
    current->next = free_list;
    free_list = current;
}