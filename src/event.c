#include "../include/event.h"
#include <stddef.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/epoll.h>

// Initializes the multiplexer and returns its file descriptor.
int event_init(void) {
    return epoll_create1(0);
}

// Registers a new socket in the multiplexer.
int event_add(int efd, int fd, int is_server) {
    struct epoll_event ev;
    ev.events = EPOLLIN;
    if (!is_server) {
        // Additional flags can go here if needed
    }
    ev.data.fd = fd;
    return epoll_ctl(efd, EPOLL_CTL_ADD, fd, &ev);
}

// Modifies a socket's state in the multiplexer.
int event_mod(int efd, int fd, int state) {
    struct epoll_event ev;
    ev.events = (state == EVENT_WRITE) ? EPOLLOUT : EPOLLIN;
    ev.data.fd = fd;
    return epoll_ctl(efd, EPOLL_CTL_MOD, fd, &ev);
}

// Blocks and returns triggered events.
int event_wait(int efd, struct VentoEvent *events, int max_events) {
    struct epoll_event ep_events[max_events];
    int n = epoll_wait(efd, ep_events, max_events, -1);
    for (int i = 0; i < n; i++) {
        events[i].fd = ep_events[i].data.fd;
        if ((ep_events[i].events & EPOLLERR) || (ep_events[i].events & EPOLLHUP) || (ep_events[i].events & EPOLLRDHUP)) {
            events[i].type = EVENT_ERROR;
        } else if (ep_events[i].events & EPOLLOUT) {
            events[i].type = EVENT_WRITE;
        } else {
            events[i].type = EVENT_READ;
        }
    }
    return n;
}

#elif defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/event.h>
#include <stdlib.h>

// Initializes the multiplexer and returns its file descriptor.
int event_init(void) {
    return kqueue();
}

// Registers a new socket in the multiplexer.
int event_add(int efd, int fd, int is_server) {
    (void)is_server;
    struct kevent ev;
    EV_SET(&ev, fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    return kevent(efd, &ev, 1, NULL, 0, NULL);
}

// Modifies a socket's state in the multiplexer.
int event_mod(int efd, int fd, int state) {
    struct kevent ev[2];
    if (state == EVENT_WRITE) {
        EV_SET(&ev[0], fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
        EV_SET(&ev[1], fd, EVFILT_WRITE, EV_ADD, 0, 0, NULL);
        return kevent(efd, ev, 2, NULL, 0, NULL);
    } else {
        EV_SET(&ev[0], fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
        EV_SET(&ev[1], fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
        return kevent(efd, ev, 2, NULL, 0, NULL);
    }
}

// Blocks and returns triggered events.
int event_wait(int efd, struct VentoEvent *events, int max_events) {
    struct kevent *kq_events = malloc(sizeof(struct kevent) * max_events);
    if (!kq_events) return -1;
    
    int n = kevent(efd, NULL, 0, kq_events, max_events, NULL);
    if (n > 0) {
        for (int i = 0; i < n; i++) {
            events[i].fd = kq_events[i].ident;
            if ((kq_events[i].flags & EV_ERROR) || (kq_events[i].flags & EV_EOF)) {
                events[i].type = EVENT_ERROR;
            } else if (kq_events[i].filter == EVFILT_WRITE) {
                events[i].type = EVENT_WRITE;
            } else {
                events[i].type = EVENT_READ;
            }
        }
    }
    free(kq_events);
    return n;
}

#else
#error "Unsupported OS for Vento Event Loop"
#endif