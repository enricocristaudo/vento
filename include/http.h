#ifndef HTTP_H
#define HTTP_H

// Struct to hold the parsed HTTP request data
typedef struct {
    char method[16];
    char path[256];
    char query[256];
    char version[16];
    char body[2048];
    int content_length;
} HttpRequest;

// Parses the raw request string from the socket into a structured format.
HttpRequest parse_http_request(const char *raw_request);

// Builds the HTTP response (headers + body) based on the requested file.
// Returns a dynamically allocated string (remember to free it!).
// Updated: Now accepts an HTTP status code (e.g., "200 OK", "404 Not Found")
char* build_http_response(const char *file_path, const char *status_code, int *response_length);

#endif