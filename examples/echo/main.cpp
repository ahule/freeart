#include <freeart/server_core.h>
#include <freeart/ihandler.h>
#include <csignal>

struct empty_ctx {};

class echo_handler : public freeart::ihandler<empty_ctx> {
public:
    void on_connect(freeart::server_core<empty_ctx>&, freeart::connection<empty_ctx>&) override {}

    int on_data(freeart::server_core<empty_ctx>&, freeart::connection<empty_ctx>& conn, const uint8_t* data, size_t len) override {
        conn.send(data, len);
        return static_cast<int>(len);
    }

    void on_disconnect(freeart::server_core<empty_ctx>&, freeart::connection<empty_ctx>&) override {}
};

int main() {
    signal(SIGPIPE, SIG_IGN);

    freeart::server_config cfg;
    cfg.port = 9000;

    freeart::server_core<empty_ctx> server(cfg, std::make_unique<echo_handler>());
    server.run();
    return 0;
}
