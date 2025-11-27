# cuPID

A simple process manager for Linux written in C.

## Description

cuPID is a lightweight process manager that provides an ncurses-based interface for monitoring and managing processes on Linux systems.

## Features

- ncurses-based terminal UI
- Configuration file support via cupidconf
- Automatic configuration file setup in `~/.config/cuPID/config.conf`

## Requirements

- Linux operating system
- C compiler (gcc or clang)
- ncurses library
- POSIX-compliant environment

## Dependencies

- **ncurses**: Terminal UI library
- **cupidconf**: Configuration parser library (included in `lib/cupidconf/`)

## Building

1. Clone the repository:
   ```bash
   git clone <repository-url>
   cd cuPID
   ```

2. Build the project:
   ```bash
   make
   ```

3. The executable will be created in `bin/cuPID`

## Installation

To install system-wide:
```bash
make install
```

This will copy the binary to `/usr/local/bin/cuPID`.

To uninstall:
```bash
make uninstall
```

## Usage

Run the program:
```bash
./bin/cuPID
```

## Configuration

cuPID automatically creates a configuration file at `~/.config/cuPID/config.conf` on first run. You can edit this file to customize cuPID's behavior.

The configuration file uses the cupidconf format:
```
key = value
```

Values starting with `~` or `~/` are automatically expanded to your home directory.

## Building from Source

The Makefile supports the following targets:

- `make` or `make all` - Build the project
- `make clean` - Remove build artifacts
- `make install` - Install to `/usr/local/bin/`
- `make uninstall` - Remove from `/usr/local/bin/`

## License

See [LICENSE](LICENSE) for details.

## Author

Written by @frankischilling

