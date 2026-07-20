#include "freeart/utils.h"
#include <fcntl.h>
#include <sys/socket.h>
#include <cerrno>

namespace freeart {

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags != -1)
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

bool recv_all(int socket, void* buffer, size_t length) {
    char* ptr = static_cast<char*>(buffer);
    while (length > 0) {
        ssize_t n = recv(socket, ptr, length, 0);
        if (n <= 0) return false;
        ptr    += n;
        length -= static_cast<size_t>(n);
    }
    return true;
}

} // namespace freeart
