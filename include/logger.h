#ifndef LOGGER_H
#define LOGGER_H

// Logs a server access event to vento.log
void log_access(const char *client_ip, const char *method, const char *uri, const char *version, int status_code);

#endif