#ifndef FREEART_FCP_H
#define FREEART_FCP_H

#include <string>
#include <mutex>

#pragma pack(push, 1)
struct packethdr { // FCP : Freeart Chat Protocol
    int type;   // 1: Login, 2: Message, 3: Exit
    int length;
    char magic[3];
    char version[3]; // "1.0"
    char sender[16];
    unsigned int checksum;
};
#pragma pack(pop)

enum TYPE {
    LOGIN = 1,
    MESSAGE = 2,
    EXIT = 3,
};

constexpr int MAX_PACKET_SIZE = 1024;

struct client_session {
    int fd;
    std::mutex mutex;
    char* buf;
    size_t bytes_received;
    packethdr header;
    bool header_ready = false;
    client_session() {
        buf = new char[MAX_PACKET_SIZE];
        bytes_received = 0;
    }
};

void set_nonblocking(int fd);
unsigned int checksum(const void* data, size_t length);
bool recv_all(int socket, void* buffer, size_t length);
void send_packet(int sockfd, int packet_type, const std::string& message, const std::string& sender = "");
char* recv_packet(int sockfd, packethdr& header);
int recv_packet_nonblocking(client_session& session, char*& out_buffer);

#endif //FREEART_FCP_H