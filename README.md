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

## Todo List

### Phase 1: Core Process Management (Priority: HIGHEST) 🔴
**Foundation for a process manager - must be done first**

- [ ] Display process list with PID, PPID, user, command
- [ ] Show process CPU usage percentage
- [ ] Display process memory usage (RSS, VMS)
- [ ] Show process state (running, sleeping, zombie, etc.)
- [ ] Implement basic ncurses UI layout (panels/windows for process list)
- [ ] Add keyboard navigation (arrow keys, page up/down)
- [ ] Implement process killing/termination functionality (SIGTERM, SIGKILL)
- [ ] Add process sorting options (by CPU, memory, PID, name)
- [ ] Display process start time and runtime
- [ ] Show process priority and nice value

### Phase 2: System Overview (Priority: HIGH) 🟠
**Essential system information for monitoring**

#### CPU Information
- [ ] Display CPU model name and architecture
- [ ] Show CPU cores (physical and logical)
- [ ] Show CPU usage percentage (overall and per-core)
- [ ] Show CPU load average (1min, 5min, 15min)
- [ ] Display current CPU frequency

#### Memory Information
- [ ] Display total system memory (RAM)
- [ ] Show used memory and percentage
- [ ] Display available/free memory
- [ ] Show cached memory
- [ ] Display buffer memory
- [ ] Show swap space (total, used, free)
- [ ] Display memory usage per process

### Phase 3: Enhanced Process Details (Priority: MEDIUM) 🟡
**Advanced process information and management**

- [ ] Implement process filtering and searching
- [ ] Display process parent-child relationships (tree view)
- [ ] Show process threads count
- [ ] Display process command line arguments
- [ ] Show process network connections
- [ ] Display process open files
- [ ] Show process I/O statistics (read/write bytes)
- [ ] Display process file descriptors count
- [ ] Show process shared memory information

### Phase 4: Advanced System Information (Priority: MEDIUM-LOW) 🟢
**Additional system monitoring features**

#### Advanced CPU Information
- [ ] Display CPU temperature (if available)
- [ ] Display CPU cache information (L1, L2, L3)
- [ ] Show CPU flags/features

#### Advanced Memory Information
- [ ] Show shared memory information
- [ ] Display memory pressure/stress indicators

#### Disk Information
- [ ] List all mounted filesystems
- [ ] Display disk usage per filesystem (total, used, free, percentage)
- [ ] Show disk I/O statistics (read/write rates)
- [ ] Display disk I/O operations per second (IOPS)
- [ ] List block devices and their sizes
- [ ] Show disk mount points and filesystem types
- [ ] Display inode usage per filesystem
- [ ] Show disk temperature (if available)
- [ ] Display disk health status (SMART data if available)

#### Network Information
- [ ] List all network interfaces
- [ ] Display network interface status (up/down)
- [ ] Show network interface IP addresses (IPv4 and IPv6)
- [ ] Display network interface MAC addresses
- [ ] Show network traffic statistics (bytes sent/received)
- [ ] Display network packet statistics (packets sent/received, errors, drops)
- [ ] Show network connection states (ESTABLISHED, LISTEN, etc.)
- [ ] Display active network connections with process information
- [ ] Show network interface speed and duplex mode
- [ ] Display routing table information
- [ ] Show DNS configuration

### Phase 5: Advanced Process Features (Priority: LOW) 🔵
**Nice-to-have advanced process details**

- [ ] Show process environment variables
- [ ] Show process resource limits
- [ ] Display process signal handlers
- [ ] Display process context switches

## Author

Written by @frankischilling
