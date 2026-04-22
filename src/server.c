#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "../include/server.h"
#include "../include/http.h"
#include "../include/utils.h"
#include "../include/logger.h"

#define BUFFER_SIZE 4096

typedef struct {
    int client_fd;
    char client_ip[INET_ADDRSTRLEN];
} ClientInfo;

// Thread handler to process individual HTTP connections
void *handle_client(void *arg) {
    ClientInfo *info = (ClientInfo *)arg;
    int client_fd = info->client_fd;
    char client_ip[INET_ADDRSTRLEN];
    strcpy(client_ip, info->client_ip);
    free(info);

    char buffer[BUFFER_SIZE] = {0};
    ssize_t bytes_read = read(client_fd, buffer, BUFFER_SIZE);

    if (bytes_read > 0) {
        HttpRequest req = parse_http_request(buffer);
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

            write(client_fd, forbidden, strlen(forbidden));
            status_code = 403;
        } else if (strcmp(req.method, "POST") == 0 && strcmp(decoded_uri, "/api/echo") == 0) {
            char response[4096];
            int response_len = snprintf(response, sizeof(response),
                                        "HTTP/1.1 200 OK\r\n"
                                        "Content-Type: text/plain\r\n"
                                        "Content-Length: %zu\r\n"
                                        "Connection: close\r\n\r\n"
                                        "%s", strlen(req.body), req.body);
            write(client_fd, response, response_len);
            status_code = 200;
        } else {
            char filepath[512] = "www";
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
                    write(client_fd, redirect_response, strlen(redirect_response));
                    status_code = 301;
                } else {
                    strcat(filepath, "index.html");
                }
            }

            if (!redirect) {
                int response_length = 0;
                char *response = build_http_response(filepath, "200 OK", &response_length);

                if (response != NULL) {
                    write(client_fd, response, response_length);
                    free(response);
                    status_code = 200;
                } else {
                    response = build_http_response("www/404.html", "404 Not Found", &response_length);

                    if (response != NULL) {
                        write(client_fd, response, response_length);
                        free(response);
                        status_code = 404;
                    } else {
                        const char *hard_fallback = "HTTP/1.1 404 Not Found\r\n"
                                                    "Content-Type: text/plain\r\n"
                                                    "Connection: close\r\n\r\n"
                                                    "404 - Resource not found. (Vento Hard Fallback)";

                        write(client_fd, hard_fallback, strlen(hard_fallback));
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
void start_server(int port) {
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
   address.sin_port = htons(port);

   if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
      perror("Bind failed");
      exit(EXIT_FAILURE);
   }

   if (listen(server_fd, 10) < 0) {
      perror("Listen failed");
      exit(EXIT_FAILURE);
   }

   printf("Vento is blowing at [http://localhost:%d]\n\n", port);

   while (1) {
      if ((client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
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

      pthread_t thread_id;
      if (pthread_create(&thread_id, NULL, handle_client, (void *)client_info) < 0) {
         perror("Could not create thread");
         free(client_info);
         close(client_fd);
         continue;
      }

      // Detach the thread so that its resources are automatically released
      // back to the system without the need for another thread to join with it.
      pthread_detach(thread_id);
   }
}