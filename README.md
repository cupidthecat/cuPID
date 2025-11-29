# cuPID

A simple process manager for Linux written in C.

## Description

cuPID is a lightweight process manager that provides an ncurses-based interface for monitoring and managing processes on Linux systems.

## Features

- ncurses-based terminal UI
- Live process list with CPU and memory usage
- **Dual view modes**: Toggle between detailed CPU/Memory view and process-focused view
- **Comprehensive CPU monitoring**: 
  - Per-core CPU usage with visual progress bars
  - CPU temperatures for all cores
  - CPU frequencies
  - Load averages (1min, 5min, 15min) with visual bars
  - CPU model and core information
- **Detailed memory monitoring**:
  - Total, used, free, and available memory
  - Cached and buffer memory
  - Swap space information
  - Configurable memory display units (KB, MB, GB, auto)
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

- Use the live UI to monitor processes
- **Keyboard shortcuts**:
  - `q` or `Q` - Exit the program
  - `v` or `V` - Toggle between CPU/Memory view and Process view
  - Arrow Up/Down - Navigate through process list
  - Page Up/Page Down - Scroll through process list

## Configuration

cuPID automatically creates a configuration file at `~/.config/cuPID/config.conf` on first run. You can edit this file to customize cuPID's behavior.

The configuration file uses the cupidconf format:
```
key = value
```

Values starting with `~` or `~/` are automatically expanded to your home directory.

### Configuration Options

All options are optional; if you omit them, cuPID falls back to sensible defaults.

#### Core / Refresh

- **`refresh_rate`**  
  - **What it does**: Sets how often the UI refreshes the process list.  
  - **Type**: integer (milliseconds)  
  - **Default**: `1000` (1 second)  
  - **Example**: `refresh_rate = 500` for 2 updates per second.

- **`default_sort`**  
  - **What it does**: Chooses which column is used to sort processes.  
  - **Type**: string (`cpu`, `memory`, `mem`, `pid`, `name`, `command`)  
  - **Default**: `cpu` (highest CPU first).

- **`sort_reverse`**  
  - **What it does**: Reverses the sort order for the selected sort column.  
  - **Type**: boolean (`true/false`, `1/0`, `yes/no`, `on/off`)  
  - **Default**: `false` (descending for CPU/mem, ascending for PID/name).  
  - **Example**: `sort_reverse = true` to see lowest CPU usage first.

- **`max_processes`**  
  - **What it does**: Limits how many processes are shown after sorting.  
  - **Type**: integer (`0` = unlimited)  
  - **Default**: `-1`.  
  - **Example**: `max_processes = 50` to only show the top 50.

#### UI / Layout

- **`show_header`**  
  - **What it does**: Shows or hides the column header row and separator line.  
  - **Type**: boolean  
  - **Default**: `true`.

- **`color_enabled`**  
  - **What it does**: Enables ncurses color output (headers highlighted, etc.).  
  - **Type**: boolean  
  - **Default**: `true`.  
  - **Note**: If your terminal does not support color, cuPID will fall back safely.

- **`ui_layout`**  
  - **What it does**: Reserved for future layout styles (`compact`, `detailed`, `minimal`).  
  - **Type**: string  
  - **Default**: `detailed` (currently only style supported; the others are future-facing).

- **`show_cpu_panel`**, **`show_memory_panel`**  
  - **What they do**: Toggle the CPU and memory summary panels at the top of the UI.  
  - **Type**: boolean  
  - **Default**: `true` for both.  
  - **Note**: When disabled, the UI shows more space for the process list.

- **`panel_height`**, **`process_list_height`**  
  - **What they do**: Control how many rows are reserved for the system info panels and the process list.  
  - **Type**: integer  
  - **Defaults**: `panel_height = 3`, `process_list_height = -1` (auto fit).

#### Process Display

- **`columns`**  
  - **What it does**: Controls which columns are shown and in what order.  
  - **Type**: comma-separated list  
  - **Recognized columns**: `pid`, `ppid`, `user`, `state`, `cpu`, `mem`, `rss`, `vms`, `command`.  
  - **Default**: `pid,user,cpu,mem,command,threads`.  
  - **Example**:  
    - `columns = pid,ppid,user,cpu,mem,rss,command`

- **`command_max_width`**  
  - **What it does**: Caps how wide the `command` column can be, in characters, ensuring narrow columns (like `threads`) have space when they appear to the right.  
  - **Type**: integer  
  - **Values**:  
    - `0` = no cap; `command` fills all remaining width (may hide columns to the right if they exist)  
    - `-1` = auto cap; `command` uses as much width as possible **while still reserving enough room** for trailing narrow columns (e.g., `threads`)  
    - `> 0` = explicit maximum width in characters  
  - **Default**: `-1`.  
  - **Examples**:  
    - `command_max_width = 60` to limit commands to roughly 60 characters  
    - `command_max_width = -1` with `columns = user,pid,cpu,mem,command,threads` so that `threads` always remains visible and `command` uses the rest.

- **`default_filter`**  
  - **What it does**: Reserved for future process filtering; currently parsed but not applied.  
  - **Type**: string (pattern)  
  - **Default**: empty string (no filter).

- **`show_threads`**  
  - **What it does**: When enabled and when `threads` is included in `columns`, shows a `threads` column with the number of OS threads for each process (from `/proc/<pid>/status`).  
  - **Type**: boolean  
  - **Default**: `false`.  
  - **Note**: cuPID does **not** auto-add `threads` to `columns`; you control its presence and position via the `columns` list.

- **`tree_view_default`**  
  - **What it does**: Controls how the process list is presented (flat vs tree).  
  - **Type**: string (`flat`, `expanded`, `collapsed`)  
  - **Default**: `flat`.  
  - **Behavior**:  
    - `flat`: Simple list (no indentation).  
    - `expanded`: Show a tree; children are indented under parents in the `command` column.  
    - `collapsed`: Only show root processes (top-level parents).  

- **`cpu_group_mode`**  
  - **What it does**: Controls how CPU usage is interpreted in tree view.  
  - **Type**: string (`flat`, `aggregate`)  
  - **Default**: `flat`.  
  - **Behavior**:  
    - `flat`: Each row shows that PID's own CPU usage only (children are independent).  
    - `aggregate`: Parent rows show the sum of their own CPU plus all descendants' CPU. Children keep their own values.  
  - **Example**: `cpu_group_mode = aggregate` with `tree_view_default = expanded` to make parent processes show total tree CPU like btop's grouped view.  

- **`highlight_selected`**  
  - **What it does**: Controls whether the currently selected process row is visually highlighted.  
  - **Type**: boolean  
  - **Default**: `true`.  
  - **Behavior**:  
    - When `true`, the selected row is drawn with reverse video (and a color pair if colors are enabled).  
    - When `false`, selection still moves internally (for future actions like process killing), but no row is visually highlighted.

#### System Monitoring

- **`cpu_show_per_core`**  
  - **What it does**: Toggles detailed per-core CPU display in the CPU panel. When enabled, shows individual usage bars, temperatures, and frequencies for each CPU core in a compact grid layout.  
  - **Type**: boolean  
  - **Default**: `true`.  
  - **Behavior**:  
    - When `true`: Shows all cores (C0, C1, C2, etc.) with usage bars, temperatures, and frequencies in a column-based grid layout  
    - When `false`: Shows only overall CPU usage, load averages, and average temperature/frequency  
  - **Note**: The per-core display automatically adjusts to terminal width, showing multiple cores per row for compact viewing.

- **`memory_units`**  
  - **What it does**: Controls the units used for memory size columns (`rss`, `vms`) and the memory panel.  
  - **Type**: string (`kb`, `mb`, `gb`, `auto`, case-insensitive)  
  - **Default**: `auto`.  
  - **Behavior**:  
    - `kb`: show sizes in kilobytes (e.g., `10240K`)  
    - `mb`: show sizes in megabytes (e.g., `10.0M`)  
    - `gb`: show sizes in gigabytes (e.g., `0.01G`)  
    - `auto`: automatically choose K/M/G based on the size.

- **`memory_show_free`**, **`memory_show_available`**, **`memory_show_cached`**, **`memory_show_buffers`**  
  - **What they do**: Control which detailed fields are shown in the memory panel.  
  - **Type**: boolean (each)  
  - **Default**: `true` for all.  
  - **Behavior**:  
    - `memory_show_free`: Shows free memory (unused RAM)  
    - `memory_show_available`: Shows available memory (memory available for new processes)  
    - `memory_show_cached`: Shows cached memory (file system cache)  
    - `memory_show_buffers`: Shows buffer memory (block device buffers)  
  - **Example**: set `memory_show_cached = false` to hide the cached line if you prefer a shorter panel.

- **`show_swap`**  
  - **What it does**: Toggles the swap section in the memory panel. When enabled and swap is present, the memory panel shows swap total/used/free plus swap usage percentage.  
  - **Type**: boolean  
  - **Default**: `true`.

- **`disk_enabled`**, **`network_enabled`**  
  - **What they do**: Reserved for future disk and network panels; currently parsed but not used.  
  - **Type**: boolean  
  - **Defaults**: `false` for both.

## View Modes

cuPID supports two view modes that you can toggle with the `v` key:

### CPU/Memory View (Default)
- **Detailed CPU panel**: Shows CPU model, core counts, load averages with progress bars, and per-core usage/temperatures/frequencies (when `cpu_show_per_core = true`)
- **Detailed memory panel**: Shows total, used, free, available, cached, buffers, and swap information
- **Less process list space**: More vertical space is used for system information

### Process View
- **Minimal CPU info**: Shows only overall CPU usage percentage and load average (1 line)
- **Minimal memory info**: Shows only used/total memory with percentage (1 line)
- **More process list space**: Maximum vertical space for the process list

Press `v` or `V` at any time to switch between these views.

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
- [x] Create config structure/struct to hold all config values
- [x] Implement config loading and parsing using cupidconf_get()
- [x] Add default values for all configuration options
- [x] Implement config validation (check valid values, ranges, etc.)
- [x] Add error handling for missing/invalid config values
- [x] Create helper functions to read config values with defaults
- [x] Add config file path resolution (handle ~ expansion via cupidconf)

#### Basic Configuration Options
- [x] `refresh_rate` - Update interval in milliseconds (default: 1000)
- [x] `default_sort` - Default sort column (cpu, memory, pid, name) (default: cpu)
- [x] `sort_reverse` - Default sort order (true/false) (default: false)
- [x] `show_header` - Show column headers (true/false) (default: true)
- [x] `color_enabled` - Enable color output (true/false) (default: true)
- [x] `max_processes` - Maximum number of processes to display (default: 0 = unlimited)

#### UI Configuration Options
- [x] `ui_layout` - Layout style (compact, detailed, minimal) (default: detailed)
- [x] `show_cpu_panel` - Show CPU information panel (true/false) (default: true)
- [x] `show_memory_panel` - Show memory information panel (true/false) (default: true)
- [x] `panel_height` - Height of system info panels (default: 3)
- [x] `process_list_height` - Height of process list window (default: auto)

#### Process Display Configuration
- [x] `columns` - Comma-separated list of columns to display (default: pid,user,cpu,mem,command)
- [x] `default_filter` - Default process filter pattern (default: empty = all)
- [x] `show_threads` - Show thread count per process (true/false) (default: false)
- [x] `tree_view_default` - Default tree view state (expanded/collapsed/flat) (default: flat)
- [x] `highlight_selected` - Highlight selected process (true/false) (default: true)

#### System Monitoring Configuration
- [x] `cpu_show_per_core` - Show per-core CPU usage (true/false) (default: true)
- [x] `memory_units` - Memory display units (KB, MB, GB, auto) (default: auto)
- [x] `show_swap` - Show swap space information (true/false) (default: true)
- [x] `disk_enabled` - Enable disk monitoring (true/false) (default: false)
- [x] `network_enabled` - Enable network monitoring (true/false) (default: false)

#### Advanced Configuration Features
- [ ] Implement config file hot reload (watch file for changes, reload on SIGHUP)
- [ ] Add config command-line override (--config option)
- [ ] Add config validation on startup with helpful error messages
- [x] Create default config file template with comments
- [x] Document all configuration options in README
- [ ] Add config option for log file path (if logging is added)
- [ ] Add config option for key bindings customization

### Phase 1: Core Process Management (Priority: HIGHEST) 🔴
**Foundation for a process manager - must be done first**

- [x] Display process list with PID, PPID, user, command
- [x] Show process CPU usage percentage
- [x] Display process memory usage (RSS, VMS)
- [x] Show process state (running, sleeping, zombie, etc.)
- [x] Implement basic ncurses UI layout (panels/windows for process list)
- [ ] Add keyboard navigation (arrow keys, page up/down)
- [ ] Implement process killing/termination functionality (SIGTERM, SIGKILL)
- [x] Add process sorting options (by CPU, memory, PID, name) - use config for default
- [ ] Display process start time and runtime
- [ ] Show process priority and nice value
- [x] Integrate configuration system - read and apply config values
- [x] Use config for refresh rate, default sort, column display

### Phase 2: System Overview (Priority: HIGH) 🟠
**Essential system information for monitoring**

#### CPU Information
- [x] Display CPU model name and architecture
- [x] Show CPU cores (physical and logical)
- [x] Show CPU usage percentage (overall and per-core) - respect cpu_show_per_core config
- [x] Show CPU load average (1min, 5min, 15min) with visual progress bars
- [x] Display current CPU frequency (per-core)
- [x] Display CPU temperature (per-core, if available)
- [x] Use config to enable/disable CPU panel display
- [x] Compact grid layout for per-core display (column-based, multiple cores per row)
- [x] View toggle between detailed CPU/Memory view and process-focused view

#### Memory Information
- [x] Display total system memory (RAM)
- [x] Show used memory and percentage
- [x] Display available/free memory (configurable)
- [x] Show cached memory (configurable)
- [x] Display buffer memory (configurable)
- [x] Show swap space (total, used, free) - respect show_swap config
- [x] Display memory usage per process
- [x] Use memory_units config for display formatting (KB, MB, GB, auto)
- [x] Use config to enable/disable memory panel display
- [x] Configurable display of individual memory fields (free, available, cached, buffers)

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
- [x] Display CPU temperature (if available) - per-core temperatures shown in detailed view
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

