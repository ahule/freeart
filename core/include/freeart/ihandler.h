#pragma once
#include <cstdint>
#include <cstddef>

namespace freeart {

template<typename T> class server_core;
template<typename T> class connection;

template<typename T>
class ihandler {
public:
    virtual ~ihandler() = default;
    virtual void on_connect(server_core<T>& server, connection<T>& conn) = 0;
    virtual int  on_data(server_core<T>& server, connection<T>& conn, const uint8_t* data, size_t len) = 0;
    virtual void on_disconnect(server_core<T>& server, connection<T>& conn) = 0;
};

} // namespace freeart
