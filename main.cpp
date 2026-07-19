#include "server.h"
#include <csignal>

int main() {
    
    signal(SIGPIPE, SIG_IGN);

    server s;
    s.server_connection();
    s.server_main();

    return 0;
}
