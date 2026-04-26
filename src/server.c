#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include "../include/server.h"
#include "../include/http.h"
#include "../include/utils.h"
#include "../include/logger.h"
#include "../include/config.h"
#include "../include/event.h"

#define MAX_EVENTS 1024
#define MAX_FDS 8192

volatile sig_atomic_t server_running = 1;

struct ClientState clients[MAX_FDS];

// Signal handler to gracefully shut down the server
void handle_signal(int sig) {
    (void)sig;
    server_running = 0;
}

// Initializes the client state array
void init_clients() {
    for (int i = 0; i < MAX_FDS; i++) {
        clients[i].client_fd = -1;
        clients[i].write_buffer = NULL;
    }
}

// Cleans up client state and closes the socket
void free_client(int fd) {
    if (fd >= 0 && fd < MAX_FDS) {
        if (clients[fd].write_buffer) {
            free(clients[fd].write_buffer);
            clients[fd].write_buffer = NULL;
        }
        if (clients[fd].client_fd != -1) {
            close(fd);
            clients[fd].client_fd = -1;
        }
    }
}

// Starts the web server using asynchronous event handling
void start_server(ServerConfig config) {
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    int server_fd;
    struct sockaddr_in address;
    int opt = 1;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    set_nonblocking(server_fd);

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(config.port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 1024) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    init_clients();

    int efd = event_init();
    if (efd == -1) {
        perror("event_init failed");
        exit(EXIT_FAILURE);
    }

    if (event_add(efd, server_fd, 1) == -1) {
        perror("event_add server_fd failed");
        exit(EXIT_FAILURE);
    }

    printf("          ▄▄ ▄▄ ▄▄▄▄▄ ▄▄  ▄▄ ▄▄▄▄▄▄ ▄▄▄            \n"
           "▄▀▀▄  █   ██▄██ ██▄▄  ███▄██   ██  ██▀██   ▄▀▀▄  █ \n"
           "▀   ▀▀     ▀█▀  ██▄▄▄ ██ ▀██   ██  ▀███▀   ▀   ▀▀  \n\n"
           "[ Vento WS is Blowing ]\n"
           "-------------------------------\n"
           "Listening on : http://localhost:%d\n"
           "Document Root: %s\n"
           "Press Ctrl+C to shut down\n\n",
           config.port, config.document_root);

    struct VentoEvent ev_list[MAX_EVENTS];

    while (server_running) {
        int nev = event_wait(efd, ev_list, MAX_EVENTS);
        if (nev < 0) {
            if (errno == EINTR) continue;
            perror("event_wait failed");
            break;
        }

        for (int i = 0; i < nev; i++) {
            int fd = ev_list[i].fd;

            if (ev_list[i].type == EVENT_ERROR) {
                free_client(fd);
                continue;
            }

            if (fd == server_fd) {
                while (1) {
                    struct sockaddr_in client_addr;
                    socklen_t addrlen = sizeof(client_addr);
                    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addrlen);
                    if (client_fd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;
                        }
                        perror("accept failed");
                        break;
                    }

                    if (client_fd >= MAX_FDS) {
                        close(client_fd);
                        continue;
                    }

                    set_nonblocking(client_fd);

                    clients[client_fd].client_fd = client_fd;
                    clients[client_fd].bytes_read = 0;
                    clients[client_fd].bytes_to_write = 0;
                    clients[client_fd].bytes_written = 0;
                    if (clients[client_fd].write_buffer) {
                        free(clients[client_fd].write_buffer);
                        clients[client_fd].write_buffer = NULL;
                    }
                    clients[client_fd].status = STATE_READING;
                    inet_ntop(AF_INET, &(client_addr.sin_addr), clients[client_fd].client_ip, INET_ADDRSTRLEN);
                    strncpy(clients[client_fd].document_root, config.document_root, sizeof(clients[client_fd].document_root) - 1);
                    clients[client_fd].document_root[sizeof(clients[client_fd].document_root) - 1] = '\0';

                    event_add(efd, client_fd, 0);
                }
            } else if (ev_list[i].type == EVENT_READ) {
                struct ClientState *client = &clients[fd];
                if (client->client_fd == -1) continue;

                int bytes_to_read = sizeof(client->read_buffer) - 1 - client->bytes_read;
                if (bytes_to_read > 0) {
                    ssize_t bytes_read = read(fd, client->read_buffer + client->bytes_read, bytes_to_read);
                    if (bytes_read > 0) {
                        client->bytes_read += bytes_read;
                        client->read_buffer[client->bytes_read] = '\0';

                        if (strstr(client->read_buffer, "\r\n\r\n") != NULL) {
                            HttpRequest req = parse_http_request(client->read_buffer);
                            char *body_start = strstr(client->read_buffer, "\r\n\r\n") + 4;
                            size_t headers_len = body_start - client->read_buffer;
                            size_t body_read = client->bytes_read - headers_len;

                            int request_complete = 1;
                            if (req.content_length > 0) {
                                size_t to_read = req.content_length;
                                if (to_read > sizeof(req.body) - 1) {
                                    to_read = sizeof(req.body) - 1;
                                }
                                if (body_read < to_read) {
                                    request_complete = 0;
                                } else {
                                    req = parse_http_request(client->read_buffer);
                                }
                            }

                            if (request_complete) {
                                char decoded_uri[256];
                                url_decode(req.path, decoded_uri);
                                int status_code = 200;

                                printf("[%s] %s %s\n", req.method, decoded_uri, req.version);

                                if (!is_safe_uri(decoded_uri)) {
                                    printf("WARNING: Blocked potential path traversal attack: %s\n", decoded_uri);
                                    const char *forbidden = "HTTP/1.1 403 Forbidden\r\n"
                                                            "Content-Type: text/plain\r\n"
                                                            "Connection: close\r\n\r\n"
                                                            "403 - Forbidden.";
                                    client->write_buffer = strdup(forbidden);
                                    client->bytes_to_write = strlen(forbidden);
                                    status_code = 403;
                                } else if (strcmp(req.method, "POST") == 0 && strcmp(decoded_uri, "/api/echo") == 0) {
                                    char response[4096];
                                    int response_len = snprintf(response, sizeof(response),
                                                                "HTTP/1.1 200 OK\r\n"
                                                                "Content-Type: text/plain\r\n"
                                                                "Content-Length: %zu\r\n"
                                                                "Connection: close\r\n\r\n"
                                                                "%s", strlen(req.body), req.body);
                                    client->write_buffer = malloc(response_len + 1);
                                    memcpy(client->write_buffer, response, response_len);
                                    client->write_buffer[response_len] = '\0';
                                    client->bytes_to_write = response_len;
                                    status_code = 200;
                                } else {
                                    char filepath[512];
                                    strcpy(filepath, client->document_root);
                                    strcat(filepath, decoded_uri);

                                    struct stat path_stat;
                                    int redirect = 0;
                                    if (stat(filepath, &path_stat) == 0 && S_ISDIR(path_stat.st_mode)) {
                                        if (filepath[strlen(filepath) - 1] != '/') {
                                            redirect = 1;
                                            char redirect_response[1024];
                                            int resp_len = snprintf(redirect_response, sizeof(redirect_response),
                                                     "HTTP/1.1 301 Moved Permanently\r\n"
                                                     "Location: %s/\r\n"
                                                     "Content-Length: 0\r\n"
                                                     "Connection: close\r\n\r\n", req.path);
                                            client->write_buffer = malloc(resp_len + 1);
                                            memcpy(client->write_buffer, redirect_response, resp_len);
                                            client->write_buffer[resp_len] = '\0';
                                            client->bytes_to_write = resp_len;
                                            status_code = 301;
                                        } else {
                                            strcat(filepath, "index.html");
                                        }
                                    }

                                    if (!redirect) {
                                        int response_length = 0;
                                        char *response = build_http_response(filepath, "200 OK", &response_length);

                                        if (response != NULL) {
                                            client->write_buffer = response;
                                            client->bytes_to_write = response_length;
                                            status_code = 200;
                                        } else {
                                            char error_path[512];
                                            snprintf(error_path, sizeof(error_path), "%s/404.html", client->document_root);
                                            response = build_http_response(error_path, "404 Not Found", &response_length);

                                            if (response != NULL) {
                                                client->write_buffer = response;
                                                client->bytes_to_write = response_length;
                                                status_code = 404;
                                            } else {
                                                const char *hard_fallback = "HTTP/1.1 404 Not Found\r\n"
                                                                            "Content-Type: text/plain\r\n"
                                                                            "Connection: close\r\n\r\n"
                                                                            "404 - Resource not found. (Vento Hard Fallback)";
                                                client->write_buffer = strdup(hard_fallback);
                                                client->bytes_to_write = strlen(hard_fallback);
                                                status_code = 404;
                                            }
                                        }
                                    }
                                }

                                log_access(client->client_ip, req.method, decoded_uri, req.version, status_code);

                                client->status = STATE_WRITING;
                                event_mod(efd, fd, EVENT_WRITE);
                            }
                        }
                    } else if (bytes_read == 0) {
                        free_client(fd);
                    } else {
                        if (errno != EAGAIN && errno != EWOULDBLOCK) {
                            free_client(fd);
                        }
                    }
                } else {
                    free_client(fd);
                }
            } else if (ev_list[i].type == EVENT_WRITE) {
                struct ClientState *client = &clients[fd];
                if (client->client_fd == -1) continue;

                if (client->status == STATE_WRITING && client->write_buffer != NULL) {
                    size_t remaining = client->bytes_to_write - client->bytes_written;
                    ssize_t bytes_written = write(fd, client->write_buffer + client->bytes_written, remaining);
                    if (bytes_written > 0) {
                        client->bytes_written += bytes_written;
                        if (client->bytes_written == client->bytes_to_write) {
                            free_client(fd);
                        }
                    } else if (bytes_written < 0) {
                        if (errno != EAGAIN && errno != EWOULDBLOCK) {
                            free_client(fd);
                        }
                    }
                }
            }
        }
    }

    printf("\nShutting down Vento...\n");
    close(server_fd);
    close(efd);

    for (int i = 0; i < MAX_FDS; i++) {
        free_client(i);
    }
}