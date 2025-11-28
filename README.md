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

### Phase 0: Configuration System (Priority: HIGHEST) 🔴
**Foundation for configurable behavior - must be done early**

#### Core Configuration Infrastructure
- [ ] Create config structure/struct to hold all config values
- [ ] Implement config loading and parsing using cupidconf_get()
- [ ] Add default values for all configuration options
- [ ] Implement config validation (check valid values, ranges, etc.)
- [ ] Add error handling for missing/invalid config values
- [ ] Create helper functions to read config values with defaults
- [ ] Add config file path resolution (handle ~ expansion via cupidconf)

#### Basic Configuration Options
- [ ] `refresh_rate` - Update interval in milliseconds (default: 1000)
- [ ] `default_sort` - Default sort column (cpu, memory, pid, name) (default: cpu)
- [ ] `sort_reverse` - Default sort order (true/false) (default: false)
- [ ] `show_header` - Show column headers (true/false) (default: true)
- [ ] `color_enabled` - Enable color output (true/false) (default: true)
- [ ] `max_processes` - Maximum number of processes to display (default: 0 = unlimited)

#### UI Configuration Options
- [ ] `ui_layout` - Layout style (compact, detailed, minimal) (default: detailed)
- [ ] `show_cpu_panel` - Show CPU information panel (true/false) (default: true)
- [ ] `show_memory_panel` - Show memory information panel (true/false) (default: true)
- [ ] `panel_height` - Height of system info panels (default: 3)
- [ ] `process_list_height` - Height of process list window (default: auto)

#### Process Display Configuration
- [ ] `columns` - Comma-separated list of columns to display (default: pid,user,cpu,mem,command)
- [ ] `default_filter` - Default process filter pattern (default: empty = all)
- [ ] `show_threads` - Show thread count per process (true/false) (default: false)
- [ ] `tree_view_default` - Default tree view state (expanded/collapsed/flat) (default: flat)
- [ ] `highlight_selected` - Highlight selected process (true/false) (default: true)

#### System Monitoring Configuration
- [ ] `cpu_show_per_core` - Show per-core CPU usage (true/false) (default: false)
- [ ] `memory_units` - Memory display units (KB, MB, GB, auto) (default: auto)
- [ ] `show_swap` - Show swap space information (true/false) (default: true)
- [ ] `disk_enabled` - Enable disk monitoring (true/false) (default: false)
- [ ] `network_enabled` - Enable network monitoring (true/false) (default: false)

#### Advanced Configuration Features
- [ ] Implement config file hot reload (watch file for changes, reload on SIGHUP)
- [ ] Add config command-line override (--config option)
- [ ] Add config validation on startup with helpful error messages
- [ ] Create default config file template with comments
- [ ] Document all configuration options in README
- [ ] Add config option for log file path (if logging is added)
- [ ] Add config option for key bindings customization

### Phase 1: Core Process Management (Priority: HIGHEST) 🔴
**Foundation for a process manager - must be done first**

- [ ] Display process list with PID, PPID, user, command
- [ ] Show process CPU usage percentage
- [ ] Display process memory usage (RSS, VMS)
- [ ] Show process state (running, sleeping, zombie, etc.)
- [ ] Implement basic ncurses UI layout (panels/windows for process list)
- [ ] Add keyboard navigation (arrow keys, page up/down)
- [ ] Implement process killing/termination functionality (SIGTERM, SIGKILL)
- [ ] Add process sorting options (by CPU, memory, PID, name) - use config for default
- [ ] Display process start time and runtime
- [ ] Show process priority and nice value
- [ ] Integrate configuration system - read and apply config values
- [ ] Use config for refresh rate, default sort, column display

### Phase 2: System Overview (Priority: HIGH) 🟠
**Essential system information for monitoring**

#### CPU Information
- [ ] Display CPU model name and architecture
- [ ] Show CPU cores (physical and logical)
- [ ] Show CPU usage percentage (overall and per-core) - respect cpu_show_per_core config
- [ ] Show CPU load average (1min, 5min, 15min)
- [ ] Display current CPU frequency
- [ ] Use config to enable/disable CPU panel display

#### Memory Information
- [ ] Display total system memory (RAM)
- [ ] Show used memory and percentage
- [ ] Display available/free memory
- [ ] Show cached memory
- [ ] Display buffer memory
- [ ] Show swap space (total, used, free) - respect show_swap config
- [ ] Display memory usage per process
- [ ] Use memory_units config for display formatting (KB, MB, GB)
- [ ] Use config to enable/disable memory panel display

### Phase 3: Enhanced Process Details (Priority: MEDIUM) 🟡
**Advanced process information and management**

- [ ] Implement process filtering and searching - use default_filter config
- [ ] Display process parent-child relationships (tree view) - respect tree_view_default config
- [ ] Show process threads count - respect show_threads config
- [ ] Display process command line arguments
- [ ] Show process network connections
- [ ] Display process open files
- [ ] Show process I/O statistics (read/write bytes)
- [ ] Display process file descriptors count
- [ ] Show process shared memory information
- [ ] Use columns config to show/hide specific columns
- [ ] Add config option for search case sensitivity
- [ ] Add config option for maximum search results

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
- [ ] Use disk_enabled config to enable/disable disk monitoring
- [ ] Add config option for disk display units (KB, MB, GB, TB)
- [ ] Add config option for which filesystems to display (filter list)
- [ ] Add config option for disk temperature units (Celsius/Fahrenheit)

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
- [ ] Use network_enabled config to enable/disable network monitoring
- [ ] Add config option for network speed units (B/s, KB/s, MB/s, GB/s)
- [ ] Add config option for which network interfaces to monitor (filter list)
- [ ] Add config option to show/hide loopback interfaces

### Phase 5: Advanced Process Features (Priority: LOW) 🔵
**Nice-to-have advanced process details**

- [ ] Show process environment variables
- [ ] Show process resource limits
- [ ] Display process signal handlers
- [ ] Display process context switches

## Author

Written by @frankischilling

