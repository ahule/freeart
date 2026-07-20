#pragma once
#include <freeart/ihandler.h>
#include <freeart/server_core.h>
#include "fcp.h"
#include <unordered_set>
#include <mutex>
#include <string>

class chat_handler : public freeart::ihandler<fcp_session> {
public:
    explicit chat_handler(std::string password);

    void on_connect   (freeart::server_core<fcp_session>& server, freeart::connection<fcp_session>& conn) override;
    int  on_data      (freeart::server_core<fcp_session>& server, freeart::connection<fcp_session>& conn, const uint8_t* data, size_t len) override;
    void on_disconnect(freeart::server_core<fcp_session>& server, freeart::connection<fcp_session>& conn) override;

private:
    void broadcast_message(freeart::server_core<fcp_session>& server, int sender_fd, const std::string& sender_name, const std::string& msg);
    bool is_authenticated(int fd);

    std::string          password_;
    std::unordered_set<int> authenticated_;
    std::mutex           auth_mutex_;
};
