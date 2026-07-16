#ifndef FREEART_FCP_H
#define FREEART_FCP_H

#include <iostream>
#include <vector>
#include <string>
#include <cstdint>

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

struct client_session {
    int fd;
    std::vector<char> headerbuf;
    std::vector<char> bodybuf;
    packethdr header;
    bool header_ready = false;
};

void set_nonblocking(int fd);
unsigned int checksum(const void* data, size_t length);
bool recv_all(int socket, void* buffer, size_t length);
void send_packet(int sockfd, int packet_type, const std::string& message, const std::string& sender = "");
char* recv_packet(int sockfd, packethdr& header);
int recv_packet_nonblocking(client_session& session, char*& out_buffer);

class fcp_memory_pool {
private:
    struct node {
        node* next;
    };

    size_t pool_size;
    size_t chunk_size;
    node* free_list;
    void* pool_start;

public:
    fcp_memory_pool(size_t chunk_size, size_t chunksnum);
    ~fcp_memory_pool();

    void* allocate();
    void deallocate(void* ptr);
};

#endif //FREEART_FCP_H