#pragma once
#include <string>
#include <cstdint>

#pragma pack(push, 1)
struct packethdr {
    int          type;
    int          length;
    char         magic[3];
    char         version[3];
    char         sender[16];
    unsigned int checksum;
};
#pragma pack(pop)

enum packet_type {
    LOGIN   = 1,
    MESSAGE = 2,
    EXIT    = 3,
};

static constexpr int MAX_PAYLOAD = 65500;

unsigned int fcp_checksum(const void* data, size_t length);
bool         fcp_send(int sockfd, int type, const std::string& payload, const std::string& sender = "");
char*        fcp_recv(int sockfd, packethdr& out_header);

struct fcp_session {
    packethdr header{};
    bool      header_ready  = false;
    bool      authenticated = false;
    char      username[16]  = {};
};

int fcp_parse(const uint8_t* buf, size_t len, fcp_session& session, char*& out_payload);
