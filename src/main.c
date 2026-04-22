#include <stdio.h>
#include <stdlib.h>
#include "../include/server.h"
#include "../include/config.h"

// Entry point for the Vento web server.
// Parses command line arguments to set the listening port.
int main(int argc, char *argv[]) {
    ServerConfig config = load_config("vento.conf");

    if (argc > 1) {
        config.port = atoi(argv[1]);
    }

    printf("Starting Vento on port %d...\n", config.port);
    start_server(config);

    return 0;
}