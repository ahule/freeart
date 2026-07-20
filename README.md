<div align="center">

# FreeArt

**A minimal, high-performance server kernel for Linux — written in C++20**

*Build any networked application on top of a solid, protocol-agnostic foundation.*

![C++20](https://img.shields.io/badge/C%2B%2B-20-blue?style=flat-square&logo=cplusplus)
![Linux](https://img.shields.io/badge/Linux-only-informational?style=flat-square&logo=linux)
![License](https://img.shields.io/badge/License-GPLv2-green?style=flat-square)
![Model](https://img.shields.io/badge/I%2FO-epoll%20%2B%20thread%20pool-orange?style=flat-square)

</div>

---

## The Kernel Idea

Most networked software reimplements the same infrastructure over and over — the event loop, the thread pool, the connection lifecycle, the I/O buffer management. FreeArt exists to eliminate that repetition.

The concept borrows from the Linux kernel philosophy: **the kernel does not decide what your application does.** It provides a stable, efficient substrate that any application can be built upon. FreeArt is that substrate for TCP servers. You bring the protocol. FreeArt handles everything beneath it.

A chat server, an HTTP server, a game server, a custom binary protocol daemon — all of these are just handlers sitting on top of the same core.

---

## Philosophy

| Principle | What it means |
|-----------|---------------|
| **Protocol Agnostic** | The core has no concept of HTTP, WebSocket, or any specific protocol. You define how bytes are interpreted. |
| **Zero Overhead Abstraction** | Connection state is templated. No virtual dispatch on the hot path, no heap allocation per message. |
| **Concurrent by Design** | epoll multiplexes connections. A thread pool processes events. Your handler is called once per readable event. |
| **Build Anything** | Implement three methods and you have a production-ready TCP server. The infrastructure is already there. |

---

## Core Concept

FreeArt is structured around a single interface: `ihandler<T>`.
You implement this interface to define what your server does with a connection and its data. The core manages the rest.

**How the layers work:**

```
┌─────────────────────────────────┐
│  Your Handler (ihandler<T>)     │  ← you write this
├─────────────────────────────────┤
│  connection<T>                  │  ← per-connection state + buffer
├─────────────────────────────────┤
│  Thread Pool                    │  ← worker threads process events
├─────────────────────────────────┤
│  epoll Event Loop               │  ← watches all file descriptors
├─────────────────────────────────┤
│  TCP Socket (bind/listen/accept)│  ← kernel
└─────────────────────────────────┘
```

**The entire API surface:**

```cpp
template<typename T>
class ihandler {
public:
    virtual void on_connect   (server_core<T>& server, connection<T>& conn) = 0;
    virtual int  on_data      (server_core<T>& server, connection<T>& conn,
                               const uint8_t* data, size_t len) = 0;
    virtual void on_disconnect(server_core<T>& server, connection<T>& conn) = 0;
};

// on_data: return bytes consumed, 0 for more data needed, -1 to close connection
```

---

## Getting Started

Define your context struct, implement the handler, instantiate the server. The example below is a complete, working TCP echo server:

```cpp
#include <freeart/server_core.h>

struct my_context {};   // your per-connection state goes here

class echo_handler : public freeart::ihandler<my_context> {
public:
    void on_connect   (freeart::server_core<my_context>&,
                       freeart::connection<my_context>&) override {}

    int  on_data      (freeart::server_core<my_context>&,
                       freeart::connection<my_context>& conn,
                       const uint8_t* data, size_t len) override {
        conn.send(data, len);
        return static_cast<int>(len);
    }

    void on_disconnect(freeart::server_core<my_context>&,
                       freeart::connection<my_context>&) override {}
};

int main() {
    freeart::server_config cfg;
    cfg.port = 9000;

    freeart::server_core<my_context> server(cfg, std::make_unique<echo_handler>());
    server.run();
}
```

---

## Requirements

- Linux (epoll is Linux-specific)
- C++20 or later
- CMake 3.20+
- pthreads

---

## Author

**Ahmad Sadaqa** — [github.com/ahule](https://github.com/ahule)

Systems programmer and the original author of FreeArt. Interested in low-level networking, performance engineering, and building tools that remove friction from server development.

---

## License

FreeArt is free software: you can redistribute it and/or modify it under the terms of the **GNU General Public License v2.0** as published by the Free Software Foundation, either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the [LICENSE](LICENSE) file for full details.