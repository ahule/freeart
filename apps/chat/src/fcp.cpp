#include "fcp.h"
#include <freeart/utils.h>
#include <cstring>
#include <cerrno>
#include <vector>
#include <sys/socket.h>
#include <algorithm>

unsigned int fcp_checksum(const void* data, size_t length) {
    const uint16_t* ptr = static_cast<const uint16_t*>(data);
    uint32_t sum = 0;
    while (length > 1) {
        sum    += *ptr++;
        length -= 2;
    }
    if (length > 0)
        sum += *reinterpret_cast<const uint8_t*>(ptr);
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);
    return static_cast<uint16_t>(~sum);
}

bool fcp_send(int sockfd, int type, const std::string& payload, const std::string& sender) {
    packethdr hdr{};
    hdr.type   = type;
    hdr.length = static_cast<int>(payload.size());
    memcpy(hdr.magic,   "FCP", 3);
    memcpy(hdr.version, "1.0", 3);

    if (!sender.empty()) {
        size_t slen = std::min(sender.size(), sizeof(hdr.sender) - 1);
        memcpy(hdr.sender, sender.c_str(), slen);
    }

    hdr.checksum = hdr.length > 0 ? fcp_checksum(payload.c_str(), hdr.length) : 0;

    size_t total = sizeof(hdr) + static_cast<size_t>(hdr.length);
    std::vector<uint8_t> buf(total);
    memcpy(buf.data(), &hdr, sizeof(hdr));
    if (hdr.length > 0)
        memcpy(buf.data() + sizeof(hdr), payload.c_str(), hdr.length);

    const uint8_t* ptr  = buf.data();
    size_t remaining = total;
    while (remaining > 0) {
        ssize_t n = send(sockfd, ptr, remaining, MSG_NOSIGNAL);
        if (n <= 0) return false;
        ptr       += n;
        remaining -= static_cast<size_t>(n);
    }
    return true;
}

char* fcp_recv(int sockfd, packethdr& out_header) {
    if (!freeart::recv_all(sockfd, &out_header, sizeof(out_header)))
        return nullptr;
    if (out_header.magic[0] != 'F' || out_header.magic[1] != 'C' || out_header.magic[2] != 'P')
        return nullptr;
    if (out_header.length <= 0 || out_header.length > MAX_PAYLOAD)
        return nullptr;

    char* buffer = new char[out_header.length + 1];
    if (freeart::recv_all(sockfd, buffer, out_header.length)) {
        buffer[out_header.length] = '\0';
        if (out_header.checksum == fcp_checksum(buffer, out_header.length))
            return buffer;
    }
    delete[] buffer;
    return nullptr;
}

int fcp_parse(const uint8_t* buf, size_t len, fcp_session& session, char*& out_payload) {
    out_payload = nullptr;

    if (!session.header_ready) {
        if (len < sizeof(packethdr)) return 0;
        memcpy(&session.header, buf, sizeof(packethdr));
        if (session.header.magic[0] != 'F' ||
            session.header.magic[1] != 'C' ||
            session.header.magic[2] != 'P' ||
            session.header.length < 0 ||
            session.header.length > MAX_PAYLOAD)
            return -1;
        session.header_ready = true;
    }

    size_t total_needed = sizeof(packethdr) + static_cast<size_t>(session.header.length);
    if (len < total_needed) return 0;

    if (session.header.length == 0) {
        out_payload    = new char[1];
        out_payload[0] = '\0';
        session.header_ready = false;
        return static_cast<int>(total_needed);
    }

    const char* body = reinterpret_cast<const char*>(buf + sizeof(packethdr));
    out_payload = new char[session.header.length + 1];
    memcpy(out_payload, body, session.header.length);
    out_payload[session.header.length] = '\0';
    session.header_ready = false;

    if (session.header.checksum != fcp_checksum(out_payload, session.header.length)) {
        delete[] out_payload;
        out_payload = nullptr;
        return -1;
    }
    return static_cast<int>(total_needed);
}
