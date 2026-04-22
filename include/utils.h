#ifndef UTILS_H
#define UTILS_H

// Reads a file from disk into a memory buffer.
// Returns the content or NULL if the file does not exist.
char* read_file_to_buffer(const char *filepath, long *file_size);

// Given a filename (e.g., "style.css"), returns the correct MIME type
// (e.g., "text/css") for the "Content-Type" HTTP header.
const char* get_mime_type(const char *filepath);

// Validates the URI to prevent Directory Traversal attacks.
// Returns 1 if the URI is safe, 0 if it contains malicious patterns.
int is_safe_uri(const char *uri);

// Decodes URL-encoded strings (e.g., "%2E" to ".", "%20" to " ").
void url_decode(const char *src, char *dest);

// Sends all data to a socket, handling partial writes.
int send_all(int socket, const char *buffer, size_t length);

#endif