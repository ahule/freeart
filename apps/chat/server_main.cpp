#include <freeart/server_core.h>
#include "chat_handler.h"
#include "fcp.h"
#include <csignal>
#include <iostream>
#include <sstream>
#include <thread>
#include <string>

static freeart::server_core<fcp_session>* g_server = nullptr;

static void admin_panel() {
    std::string line;
    std::cout << "cmd: " << std::flush;
    while (std::getline(std::cin, line)) {
        if (line == "quit" || line == "exit") {
            if (g_server) g_server->stop();
            break;
        }
        if (line == "count") {
            std::cout << "connections: " << (g_server ? g_server->connection_count() : 0) << '\n';
        }
        std::cout << "cmd: " << std::flush;
    }
}

int main(int argc, char* argv[]) {
    signal(SIGPIPE, SIG_IGN);

    freeart::server_config cfg;
    cfg.port     = 8888;
    cfg.backlog  = 512;

    std::string password = "password";
    if (argc > 1) password = argv[1];
    if (argc > 2) cfg.port = static_cast<uint16_t>(std::stoi(argv[2]));

    auto handler = std::make_unique<chat_handler>(password);

    freeart::server_core<fcp_session> server(cfg, std::move(handler));
    g_server = &server;

    std::thread admin_thread(admin_panel);
    admin_thread.detach();

    server.run();
    return 0;
}
