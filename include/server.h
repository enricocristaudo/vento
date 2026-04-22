#ifndef SERVER_H
#define SERVER_H

#include "config.h"

// Starts the server and listens on a specific port based on the provided configuration.
// Contains the infinite loop (while(server_running)) to accept incoming connections.
void start_server(ServerConfig config);

#endif