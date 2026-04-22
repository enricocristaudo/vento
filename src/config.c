#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/config.h"

ServerConfig load_config(const char *filepath) {
    ServerConfig config = {
        .port = 8080,
        .document_root = "www"
    };

    FILE *file = fopen(filepath, "r");
    if (!file) {
        return config;
    }

    char line[512];
    while (fgets(line, sizeof(line), file)) {
        char key[256] = {0};
        char value[256] = {0};
        
        if (sscanf(line, "%255[^=]=%255[^\n]", key, value) == 2) {
            if (strcmp(key, "PORT") == 0) {
                config.port = atoi(value);
            } else if (strcmp(key, "DOCUMENT_ROOT") == 0) {
                strncpy(config.document_root, value, sizeof(config.document_root) - 1);
                config.document_root[sizeof(config.document_root) - 1] = '\0';
            }
        }
    }

    fclose(file);
    return config;
}