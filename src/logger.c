#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include "../include/logger.h"

static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

void log_access(const char *client_ip, const char *method, const char *uri, const char *version, int status_code) {
    time_t now;
    struct tm *tm_info;
    char time_buffer[64];

    time(&now);
    tm_info = localtime(&now);

    // Format: [22/Apr/2026:12:34:03 +0200]
    strftime(time_buffer, sizeof(time_buffer), "[%d/%b/%Y:%H:%M:%S %z]", tm_info);

    pthread_mutex_lock(&log_mutex);

    FILE *log_file = fopen("vento.log", "a");
    if (log_file) {
        // Apache-style log format: IP - - [Date] "METHOD URI HTTP/1.1" STATUS_CODE
        fprintf(log_file, "%s - - %s \"%s %s %s\" %d\n", client_ip, time_buffer, method, uri, version, status_code);
        fclose(log_file);
    }

    pthread_mutex_unlock(&log_mutex);
}