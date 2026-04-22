#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/http.h"
#include "../include/utils.h"

// Parses the first line of a raw HTTP request into a structured HttpRequest.
// Extracts method, path, query parameters, version, and the payload body.
HttpRequest parse_http_request(const char *raw_request) {
   HttpRequest req = {0};
   char full_uri[512] = {0};
   
   sscanf(raw_request, "%15s %511s %15s", req.method, full_uri, req.version);
   
   char *query_start = strchr(full_uri, '?');
   if (query_start) {
      *query_start = '\0';
      strncpy(req.path, full_uri, sizeof(req.path) - 1);
      strncpy(req.query, query_start + 1, sizeof(req.query) - 1);
   } else {
      strncpy(req.path, full_uri, sizeof(req.path) - 1);
   }

   const char *body_start = strstr(raw_request, "\r\n\r\n");
   if (body_start) {
      strncpy(req.body, body_start + 4, sizeof(req.body) - 1);
   }

   return req;
}

// Constructs the full HTTP response including headers and the requested file content.
// Returns a dynamically allocated string containing the entire response.
char* build_http_response(const char *file_path, const char *status_code, int *response_length) {
   long file_size = 0;
   char *file_content = read_file_to_buffer(file_path, &file_size);

   if (!file_content) return NULL;

   const char *mime_type = get_mime_type(file_path);
   char header_buffer[1024];

   int header_len = snprintf(header_buffer, sizeof(header_buffer),
      "HTTP/1.1 %s\r\n"
      "Server: Vento/1.0\r\n"
      "Content-Type: %s\r\n"
      "Content-Length: %ld\r\n"
      "Connection: close\r\n\r\n",
      status_code, mime_type, file_size);

   *response_length = header_len + file_size;
   char *full_response = malloc(*response_length);

   if (!full_response) {
      free(file_content);
      return NULL;
   }

   memcpy(full_response, header_buffer, header_len);
   memcpy(full_response + header_len, file_content, file_size);
   free(file_content);

   return full_response;
}