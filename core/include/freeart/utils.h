#pragma once
#include <cstddef>

namespace freeart {

void set_nonblocking(int fd);
bool recv_all(int socket, void* buffer, size_t length);

}
