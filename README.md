# Vento

![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)
![Language: C](https://img.shields.io/badge/Language-C-blue.svg)
![Platform: Linux | macOS](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS-lightgrey.svg)
![Build: Passing](https://img.shields.io/badge/Build-Passing-brightgreen.svg)

**Vento** is a high-performance, asynchronous, zero-copy HTTP server written entirely in pure C. Designed for massive concurrency and minimal resource overhead, Vento leverages modern POSIX APIs and kernel-level optimizations to deliver both static assets and dynamic responses with exceptional efficiency.

> **Disclaimer:** Vento is an educational high-performance project intended to demonstrate systems programming concepts, kernel-level I/O optimization, and non-blocking architecture. While fully functional, it is designed for learning and research purposes rather than immediate production deployment.

## Technical Architecture

Vento achieves its performance characteristics by abandoning traditional thread-per-connection models in favor of an event-driven, non-blocking architecture.

- **Event-Driven Concurrency:** Employs kernel-level I/O event notification mechanisms—`kqueue` on macOS/FreeBSD and `epoll` on Linux—allowing a single thread to manage thousands of concurrent connections efficiently.
- **Zero-Copy I/O:** Utilizes the `sendfile()` system call to bypass the user-space buffer when serving static files. Data is streamed directly from the file descriptor to the socket descriptor within the kernel space, resulting in near-zero CPU and RAM overhead.
- **Security-First Design:** Features robust built-in protections against common application-layer attacks. It includes automated state management to mitigate Slowloris attacks and implements an L7 HTTP Flood rate limiting engine via an internal hash table.
- **Protocol Efficiency:** Native support for HTTP/1.1 Keep-Alive connections, reducing TCP handshake overhead by multiplexing multiple requests over persistent sockets.

## Performance

In localized benchmarking environments, Vento consistently achieves throughput in excess of **~20,000 Requests Per Second (RPS)** on a local loopback interface, validating the efficiency of its zero-copy and asynchronous event-driven design.

## Project Structure

The codebase is strictly modular, separating core engine logic from network and HTTP specifics:

- `src/event.c` - OS-specific abstractions for event multiplexing (`epoll` / `kqueue`).
- `src/io.c` - Zero-copy file transmission routines wrapping the `sendfile()` API.
- `src/ratelimit.c` - L7 traffic management, request tracking, and IP-based rate limiting logic.
- `src/http.c` / `src/server.c` - HTTP/1.1 protocol parsing, state machine execution, and request lifecycle management (including Keep-Alive).
- `src/main.c` - Daemon initialization, socket binding, and event loop entry.
- `include/` - Core domain structures, macros, and function prototypes.

## Getting Started

### Prerequisites

Compilation requires a POSIX-compliant environment (Linux, macOS, or WSL) equipped with `gcc` and `make`.

### Build Instructions

Vento uses a standard `Makefile` for streamlined builds. Clone the repository and execute `make` to compile the source tree:

```bash
git clone https://github.com/nnevskij/vento
cd vento
make
```

The compiled binary will be generated in the `bin/` directory.

### Configuration

Vento operates using a dynamically loaded `vento.conf` file located in the working directory. If omitted, defaults are applied (Port: `8080`, Document Root: `"www"`).

Example `vento.conf`:
```ini
PORT=3000
DOCUMENT_ROOT=www
```

### Running the Server

Start the daemon by executing the compiled binary:

```bash
./bin/vento
```

To override the port without modifying the configuration file:

```bash
./bin/vento 8081
```

Graceful shutdown is supported; sending `SIGINT` (Ctrl+C) or `SIGTERM` will cleanly release descriptors, close connections, and terminate the process.

## License

This software is released under the [MIT License](LICENSE).
