#ifndef CONFIG_H
#define CONFIG_H

// Struct to hold server configuration settings
typedef struct {
    int port;
    char document_root[256];
} ServerConfig;

// Loads server configuration from a file, falling back to defaults if necessary
ServerConfig load_config(const char *filepath);

#endif