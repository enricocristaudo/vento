#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include "../include/server.h"
#include "../include/http.h"
#include "../include/utils.h"
#include "../include/logger.h"
#include "../include/config.h"

#define BUFFER_SIZE 4096

volatile sig_atomic_t server_running = 1;

void handle_signal(int sig) {
    (void)sig;
    server_running = 0;
}

typedef struct {
    int client_fd;
    char client_ip[INET_ADDRSTRLEN];
    char document_root[256];
} ClientInfo;

// Thread handler to process individual HTTP connections
void *handle_client(void *arg) {
    ClientInfo *info = (ClientInfo *)arg;
    int client_fd = info->client_fd;
    char client_ip[INET_ADDRSTRLEN];
    char document_root[256];
    strcpy(client_ip, info->client_ip);
    strcpy(document_root, info->document_root);
    free(info);

    char buffer[BUFFER_SIZE] = {0};
    size_t total_read = 0;
    int headers_found = 0;

    while (total_read < BUFFER_SIZE - 1) {
        ssize_t bytes_read = read(client_fd, buffer + total_read, BUFFER_SIZE - 1 - total_read);
        if (bytes_read <= 0) {
            break;
        }
        total_read += bytes_read;
        buffer[total_read] = '\0';

        if (strstr(buffer, "\r\n\r\n") != NULL) {
            headers_found = 1;
            break;
        }
    }

    if (headers_found) {
        HttpRequest req = parse_http_request(buffer);

        char *body_start = strstr(buffer, "\r\n\r\n") + 4;
        size_t headers_len = body_start - buffer;
        size_t body_read = total_read - headers_len;

        if (req.content_length > 0) {
            size_t to_read = req.content_length;
            if (to_read > sizeof(req.body) - 1) {
                to_read = sizeof(req.body) - 1;
            }

            while (body_read < to_read && total_read < BUFFER_SIZE - 1) {
                ssize_t bytes_read = read(client_fd, buffer + total_read, BUFFER_SIZE - 1 - total_read);
                if (bytes_read <= 0) {
                    break;
                }
                total_read += bytes_read;
                body_read += bytes_read;
                buffer[total_read] = '\0';
            }
            req = parse_http_request(buffer);
        }

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

            send_all(client_fd, forbidden, strlen(forbidden));
            status_code = 403;
        } else if (strcmp(req.method, "POST") == 0 && strcmp(decoded_uri, "/api/echo") == 0) {
            char response[4096];
            int response_len = snprintf(response, sizeof(response),
                                        "HTTP/1.1 200 OK\r\n"
                                        "Content-Type: text/plain\r\n"
                                        "Content-Length: %zu\r\n"
                                        "Connection: close\r\n\r\n"
                                        "%s", strlen(req.body), req.body);
            send_all(client_fd, response, response_len);
            status_code = 200;
        } else {
            char filepath[512];
            strcpy(filepath, document_root);
            strcat(filepath, decoded_uri);

            struct stat path_stat;
            int redirect = 0;
            if (stat(filepath, &path_stat) == 0 && S_ISDIR(path_stat.st_mode)) {
                if (filepath[strlen(filepath) - 1] != '/') {
                    redirect = 1;
                    char redirect_response[1024];
                    snprintf(redirect_response, sizeof(redirect_response),
                             "HTTP/1.1 301 Moved Permanently\r\n"
                             "Location: %s/\r\n"
                             "Content-Length: 0\r\n"
                             "Connection: close\r\n\r\n", req.path);
                    send_all(client_fd, redirect_response, strlen(redirect_response));
                    status_code = 301;
                } else {
                    strcat(filepath, "index.html");
                }
            }

            if (!redirect) {
                int response_length = 0;
                char *response = build_http_response(filepath, "200 OK", &response_length);

                if (response != NULL) {
                    send_all(client_fd, response, response_length);
                    free(response);
                    status_code = 200;
                } else {
                    char error_path[512];
                    snprintf(error_path, sizeof(error_path), "%s/404.html", document_root);
                    response = build_http_response(error_path, "404 Not Found", &response_length);

                    if (response != NULL) {
                        send_all(client_fd, response, response_length);
                        free(response);
                        status_code = 404;
                    } else {
                        const char *hard_fallback = "HTTP/1.1 404 Not Found\r\n"
                                                    "Content-Type: text/plain\r\n"
                                                    "Connection: close\r\n\r\n"
                                                    "404 - Resource not found. (Vento Hard Fallback)";

                        send_all(client_fd, hard_fallback, strlen(hard_fallback));
                        status_code = 404;
                    }
                }
            }
        }

        log_access(client_ip, req.method, decoded_uri, req.version, status_code);
    }

    close(client_fd);
    pthread_exit(NULL);
    return NULL;
}

// Initializes the socket, binds it to the specified port, and enters
// an infinite loop to accept and handle incoming HTTP connections.
void start_server(ServerConfig config) {
   struct sigaction sa;
   sa.sa_handler = handle_signal;
   sa.sa_flags = 0;
   sigemptyset(&sa.sa_mask);
   sigaction(SIGINT, &sa, NULL);
   sigaction(SIGTERM, &sa, NULL);

   int server_fd, client_fd;
   struct sockaddr_in address;
   int opt = 1;
   int addrlen = sizeof(address);

   if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
      perror("Socket creation failed");
      exit(EXIT_FAILURE);
   }

   if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
      perror("setsockopt failed");
      exit(EXIT_FAILURE);
   }

   address.sin_family = AF_INET;
   address.sin_addr.s_addr = INADDR_ANY;
   address.sin_port = htons(config.port);

   if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
      perror("Bind failed");
      exit(EXIT_FAILURE);
   }

   if (listen(server_fd, 10) < 0) {
      perror("Listen failed");
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

   while (server_running) {
      if ((client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
         if (!server_running) break;
         perror("Accept failed");
         continue;
      }

      ClientInfo *client_info = malloc(sizeof(ClientInfo));
      if (!client_info) {
         perror("Failed to allocate memory for new connection");
         close(client_fd);
         continue;
      }
      client_info->client_fd = client_fd;
      inet_ntop(AF_INET, &(address.sin_addr), client_info->client_ip, INET_ADDRSTRLEN);
      strncpy(client_info->document_root, config.document_root, sizeof(client_info->document_root) - 1);
      client_info->document_root[sizeof(client_info->document_root) - 1] = '\0';

      pthread_t thread_id;
      if (pthread_create(&thread_id, NULL, handle_client, (void *)client_info) < 0) {
         perror("Could not create thread");
         free(client_info);
         close(client_fd);
         continue;
      }

      pthread_detach(thread_id);
   }

   printf("\nShutting down Vento...\n");
   close(server_fd);
}