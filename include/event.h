#ifndef EVENT_H
#define EVENT_H

enum EventType {
    EVENT_READ,
    EVENT_WRITE,
    EVENT_ERROR
};

struct VentoEvent {
    int fd;
    enum EventType type;
};

// Initializes the multiplexer and returns its file descriptor.
int event_init(void);

// Registers a new socket in the multiplexer.
int event_add(int efd, int fd, int is_server);

// Modifies a socket's state in the multiplexer.
int event_mod(int efd, int fd, int state);

// Blocks and returns triggered events.
int event_wait(int efd, struct VentoEvent *events, int max_events);

#endif