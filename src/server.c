#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <pthread.h>
#include "../include/server.h"
#include "../include/http.h"
#include "../include/utils.h"

#define BUFFER_SIZE 4096

// Thread handler to process individual HTTP connections
void *handle_client(void *arg) {
    int client_fd = *((int *)arg);
    free(arg);

    char buffer[BUFFER_SIZE] = {0};
    ssize_t bytes_read = read(client_fd, buffer, BUFFER_SIZE);

    if (bytes_read > 0) {
        HttpRequest req = parse_http_request(buffer);
        char decoded_uri[256];
        url_decode(req.uri, decoded_uri);

        printf("[%s] %s %s\n", req.method, decoded_uri, req.version);

        if (!is_safe_uri(decoded_uri)) {
            printf("WARNING: Blocked potential path traversal attack: %s\n", decoded_uri);

            const char *forbidden = "HTTP/1.1 403 Forbidden\r\n"
                                    "Content-Type: text/plain\r\n"
                                    "Connection: close\r\n\r\n"
                                    "403 - Forbidden.";

            write(client_fd, forbidden, strlen(forbidden));
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
                             "Connection: close\r\n\r\n", req.uri);
                    write(client_fd, redirect_response, strlen(redirect_response));
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
                } else {
                    response = build_http_response("www/404.html", "404 Not Found", &response_length);

                    if (response != NULL) {
                        write(client_fd, response, response_length);
                        free(response);
                    } else {
                        const char *hard_fallback = "HTTP/1.1 404 Not Found\r\n"
                                                    "Content-Type: text/plain\r\n"
                                                    "Connection: close\r\n\r\n"
                                                    "404 - Resource not found.";

                        write(client_fd, hard_fallback, strlen(hard_fallback));
                    }
                }
            }
        }
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

      int *new_sock = malloc(sizeof(int));
      if (!new_sock) {
         perror("Failed to allocate memory for new connection");
         close(client_fd);
         continue;
      }
      *new_sock = client_fd;

      pthread_t thread_id;
      if (pthread_create(&thread_id, NULL, handle_client, (void *)new_sock) < 0) {
         perror("Could not create thread");
         free(new_sock);
         close(client_fd);
         continue;
      }

      pthread_detach(thread_id);
   }
}