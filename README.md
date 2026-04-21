# ~ vento ~

![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)
![Language: C](https://img.shields.io/badge/Language-C-blue.svg)
![Platform: Linux | macOS](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS-lightgrey.svg)
![Build: Passing](https://img.shields.io/badge/Build-Passing-brightgreen.svg)

**Vento** is a lightweight, blazing-fast, and multithreaded HTTP web server written from scratch in pure C. It utilizes raw POSIX sockets to serve static files with minimal overhead, making it an excellent educational resource for understanding how the web works under the hood.

## Features

- **Pure C Implementation:** No external dependencies, just standard C and POSIX libraries.
- **Multithreaded:** Handles concurrent client requests efficiently by spawning independent threads via `<pthread.h>`.
- **Static File Serving:** Delivers HTML, CSS, JavaScript, and images straight from the `www/` directory.
- **MIME Type Recognition:** Automatically detects and assigns the correct `Content-Type` headers for common file extensions.
- **Security:** Built-in protection against Directory Traversal (Path Traversal) attacks to ensure files outside the web root cannot be accessed.
- **Standard Routing:** Automatically resolves `/index.html` when a directory is requested and gracefully handles trailing slashes with HTTP `301 Moved Permanently` redirects.
- **Custom Error Pages:** Supports standard error handling, including `404 Not Found` pages.

## Getting Started

### Prerequisites

To build and run Vento, you need a UNIX-like environment (Linux, macOS, or WSL on Windows) with `gcc` and `make` installed.

### Building the Server

Clone the repository and run `make` in the root directory:

```bash
git clone https://github.com/enrico/vento.git
cd vento
make
```

This will compile the source code and place the executable in the `bin/` directory.

### Running Vento

You can start the server by executing the compiled binary. By default, Vento listens on port `8080`.

```bash
./bin/vento
```

To specify a custom port, pass it as an argument:

```bash
./bin/vento 3000
```

Once running, simply open your browser and navigate to `http://localhost:8080` (or your custom port). The server will start serving files located in the `www/` folder.

## Directory Structure

- `src/`: Contains all the C source code for the server, HTTP parsing, and utilities.
- `include/`: Header files defining the core structures and functions.
- `www/`: The default directory containing the static files (HTML, CSS, JS, images) served to clients.
- `bin/`: The output directory for the compiled executable.
- `obj/`: The directory where intermediate object files are stored during compilation.

## Cleaning Up

To remove the compiled binaries and intermediate object files, run:

```bash
make clean
```

## Contributing

Contributions, issues, and feature requests are welcome! Feel free to check the [issues page](https://github.com/enrico/vento/issues).

## License

This project is open-source and available under the [MIT License](LICENSE).