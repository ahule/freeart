#ifndef FREEART_FCP_H
#define FREEART_FCP_H


using namespace std;

#pragma pack(push, 1)
struct packethdr { // FCP : Freeart Chat Protocol
    int type;   // 1: Login, 2: Message, 3: Exit
    int length;
    char magic[3];
    char version[3]; // "1.01"
    char sender[16];
    unsigned int checksum = 0;
};
#pragma pack(pop)

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

void send_packet(int sockfd, int packet_type, const string& message) {
    packethdr header;
    memset(&header, 0, sizeof(header));

    header.type = packet_type;
    header.length = strlen(message.c_str());
    strncpy(header.magic, "FCP", 3);
    strncpy(header.version, "1.0", 3);

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
        cout << "\n[Connection] Server disconnected." << endl;
        close(sockfd);
        return nullptr;
    }

    if (header.magic[0] != 'F' || header.magic[1] != 'C' || header.magic[2] != 'P') {
        return nullptr;
    }

    if (header.length > 0) {
        char* buffer = new char[header.length + 1];

        if (recv_all(sockfd, buffer, header.length)) {
            buffer[header.length] = '\0';

            if (header.checksum == checksum(buffer, header.length)) {
                return buffer;
            } else {
                cout << "\n[Security Error] Packet corrupted! Checksum mismatch." << endl;
                delete[] buffer;
                return nullptr;
            }
        } else {
            delete[] buffer;
            return nullptr;
        }
    }
    return nullptr;
}



#endif //FREEART_FCP_H
