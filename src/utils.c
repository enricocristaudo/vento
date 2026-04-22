#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include "../include/utils.h"

// Loads an entire file into a dynamically allocated memory buffer.
// Returns the buffer pointer, or NULL if the file cannot be read.
char* read_file_to_buffer(const char *filepath, long *file_size) {

    FILE *file = fopen(filepath, "rb");
    if (!file) {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    *file_size = ftell(file);

    fseek(file, 0, SEEK_SET);

    char *buffer = malloc(*file_size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    fread(buffer, 1, *file_size, file);
    buffer[*file_size] = '\0';

    fclose(file);
    return buffer;
}

// Determines the appropriate MIME type based on the file extension.
// Defaults to "text/plain" if the extension is unknown.
const char* get_mime_type(const char *filepath) {
    const char *dot = strrchr(filepath, '.');

    if (!dot) return "text/plain";

    if (strcmp(dot, ".html") == 0) return "text/html";
    if (strcmp(dot, ".css") == 0)  return "text/css";
    if (strcmp(dot, ".js") == 0)   return "application/javascript";
    if (strcmp(dot, ".json") == 0) return "application/json";
    if (strcmp(dot, ".png") == 0)  return "image/png";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0) return "image/jpeg";

    return "text/plain";
}

int is_safe_uri(const char *uri) {
    if (strstr(uri, "..") != NULL) {
        return 0;
    }

    return 1;
}

void url_decode(const char *src, char *dest) {
    char a, b;
    while (*src) {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit(a) && isxdigit(b))) {

            if (a >= 'a') a -= 'a' - 'A';
            if (a >= 'A') a -= ('A' - 10);
            else a -= '0';

            if (b >= 'a') b -= 'a' - 'A';
            if (b >= 'A') b -= ('A' - 10);
            else b -= '0';

            *dest++ = 16 * a + b;
            src += 3;
        } else if (*src == '+') {
            *dest++ = ' ';
            src++;
        } else {
            *dest++ = *src++;
        }
    }
    *dest = '\0';
}

// Sends all data to a socket, handling partial writes.
int send_all(int socket, const char *buffer, size_t length) {
    size_t bytes_sent = 0;
    while (bytes_sent < length) {
        ssize_t n = write(socket, buffer + bytes_sent, length - bytes_sent);
        if (n <= 0) {
            return -1;
        }
        bytes_sent += n;
    }
    return 0;
}