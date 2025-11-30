#include "cpu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <ncurses.h>

static void draw_progress_bar(int y, int x, double percent, int width, bool use_color) {
    if (width < 1)
        return;
    
    // Clamp percent to 0-100
    if (percent < 0.0)
        percent = 0.0;
    if (percent > 100.0)
        percent = 100.0;
    
    int filled = (int)((percent / 100.0) * (width - 2)); // -2 for brackets
    if (filled > width - 2)
        filled = width - 2;
    
    // Draw bar with brackets
    mvaddch(y, x, '[');
    
    // Choose color based on usage
    if (use_color && has_colors()) {
        if (percent > 80.0)
            attron(COLOR_PAIR(2)); // yellow for high
        else if (percent > 50.0)
            attron(COLOR_PAIR(1)); // cyan for medium
    }
    
    // Draw filled portion
    for (int i = 0; i < filled; i++) {
        mvaddch(y, x + 1 + i, '#'); // solid block
    }
    
    // Draw empty portion
    for (int i = filled; i < width - 2; i++) {
        mvaddch(y, x + 1 + i, ' ');
    }
    
    if (use_color && has_colors())
        attroff(COLOR_PAIR(1) | COLOR_PAIR(2));
    
    mvaddch(y, x + width - 1, ']');
}

static int read_cpu_info_static(cpu_info_t *info) {
    if (!info)
        return -1;

    FILE *fp = fopen("/proc/cpuinfo", "r");
    if (!fp)
        return -1;

    int max_physical_id = -1;
    int max_core_id = -1;
    int logical_count = 0;
    bool found_model = false;
    // Track unique (physical_id, core_id) pairs to count physical cores
    bool seen_cores[256][256] = {false}; // Support up to 256 physical packages and 256 cores each
    int unique_physical_cores = 0;

    char line[512];
    int current_physical_id = -1;
    int current_core_id = -1;
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "model name", 10) == 0) {
            char *colon = strchr(line, ':');
            if (colon) {
                colon++;
                while (*colon == ' ' || *colon == '\t')
                    colon++;
                size_t len = strlen(colon);
                while (len > 0 && (colon[len - 1] == '\n' || colon[len - 1] == '\r'))
                    colon[--len] = '\0';
                if (len > 0 && len < sizeof(info->model_name)) {
                    strncpy(info->model_name, colon, sizeof(info->model_name) - 1);
                    info->model_name[sizeof(info->model_name) - 1] = '\0';
                    found_model = true;
                }
            }
        } else if (strncmp(line, "processor", 9) == 0) {
            logical_count++;
            // Reset for new processor entry
            current_physical_id = -1;
            current_core_id = -1;
        } else if (strncmp(line, "physical id", 11) == 0) {
            char *colon = strchr(line, ':');
            if (colon) {
                int pid = atoi(colon + 1);
                if (pid > max_physical_id)
                    max_physical_id = pid;
                current_physical_id = pid;
            }
        } else if (strncmp(line, "core id", 7) == 0) {
            char *colon = strchr(line, ':');
            if (colon) {
                int cid = atoi(colon + 1);
                if (cid > max_core_id)
                    max_core_id = cid;
                current_core_id = cid;
                // Track unique physical cores
                if (current_physical_id >= 0 && current_core_id >= 0) {
                    int pid = current_physical_id < 256 ? current_physical_id : 255;
                    int cid = current_core_id < 256 ? current_core_id : 255;
                    if (!seen_cores[pid][cid]) {
                        seen_cores[pid][cid] = true;
                        unique_physical_cores++;
                    }
                }
            }
        }
    }
    fclose(fp);

    info->logical_cores = logical_count > 0 ? logical_count : sysconf(_SC_NPROCESSORS_ONLN);
    if (unique_physical_cores > 0) {
        info->physical_cores = unique_physical_cores;
    } else if (max_physical_id >= 0 && max_core_id >= 0) {
        // Fallback: estimate based on max values (less accurate for hybrid CPUs)
        info->physical_cores = (max_physical_id + 1) * (max_core_id + 1);
    } else {
        // Fallback: assume hyperthreading if logical > 1
        info->physical_cores = (info->logical_cores + 1) / 2;
        if (info->physical_cores < 1)
            info->physical_cores = 1;
    }

    if (!found_model) {
        strncpy(info->model_name, "Unknown CPU", sizeof(info->model_name) - 1);
        info->model_name[sizeof(info->model_name) - 1] = '\0';
    }

    return 0;
}

static int read_per_core_usage(cpu_info_t *info) {
    if (!info || info->logical_cores <= 0)
        return -1;

    static unsigned long long *prev_totals = NULL;
    static unsigned long long *prev_idles = NULL;
    static bool have_prev = false;
    static int prev_core_count = 0;

    if (prev_core_count != info->logical_cores) {
        free(prev_totals);
        free(prev_idles);
        prev_totals = NULL;
        prev_idles = NULL;
        have_prev = false;
        prev_core_count = info->logical_cores;
    }

    if (!info->core_usage) {
        info->core_usage = calloc(info->logical_cores, sizeof(double));
        if (!info->core_usage)
            return -1;
    }

    if (!prev_totals) {
        prev_totals = calloc(info->logical_cores, sizeof(unsigned long long));
        prev_idles = calloc(info->logical_cores, sizeof(unsigned long long));
        if (!prev_totals || !prev_idles)
            return -1;
    }

    FILE *fp = fopen("/proc/stat", "r");
    if (!fp)
        return -1;

    char line[256];
    int core_idx = -1;
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "cpu", 3) != 0)
            continue;
        if (strncmp(line, "cpu ", 4) == 0)
            continue; // skip aggregate cpu line

        unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
        int parsed = sscanf(line, "cpu%d %llu %llu %llu %llu %llu %llu %llu %llu",
                            &core_idx, &user, &nice, &system, &idle,
                            &iowait, &irq, &softirq, &steal);
        if (parsed < 9 || core_idx < 0 || core_idx >= info->logical_cores)
            continue;

        unsigned long long idle_all = idle + iowait;
        unsigned long long non_idle = user + nice + system + irq + softirq + steal;
        unsigned long long total = idle_all + non_idle;

        if (have_prev && core_idx < info->logical_cores) {
            unsigned long long total_delta = total - prev_totals[core_idx];
            unsigned long long idle_delta = idle_all - prev_idles[core_idx];
            if (total_delta > 0) {
                double usage = (double)(total_delta - idle_delta) / (double)total_delta * 100.0;
                if (usage < 0.0)
                    usage = 0.0;
                if (usage > 100.0)
                    usage = 100.0;
                info->core_usage[core_idx] = usage;
            }
        }

        prev_totals[core_idx] = total;
        prev_idles[core_idx] = idle_all;
    }
    fclose(fp);

    have_prev = true;
    return 0;
}

static int read_cpu_temperatures(cpu_info_t *info) {
    if (!info || info->logical_cores <= 0)
        return -1;

    if (!info->core_temps) {
        info->core_temps = calloc(info->logical_cores, sizeof(double));
        if (!info->core_temps)
            return -1;
        // Initialize to invalid value
        for (int i = 0; i < info->logical_cores; i++)
            info->core_temps[i] = -999.0;
    }

    // Try to read from thermal zones
    DIR *thermal_dir = opendir("/sys/class/thermal");
    if (thermal_dir) {
        struct dirent *entry;
        int temp_idx = 0;
        while ((entry = readdir(thermal_dir)) != NULL) {
            if (strncmp(entry->d_name, "thermal_zone", 12) != 0)
                continue;

            char path[512];
            snprintf(path, sizeof(path), "/sys/class/thermal/%.200s/type", entry->d_name);
            FILE *fp = fopen(path, "r");
            if (fp) {
                char type[64];
                if (fgets(type, sizeof(type), fp)) {
                    // Check if it's a CPU thermal zone
                    if (strstr(type, "x86_pkg_temp") || strstr(type, "cpu") || strstr(type, "Tdie")) {
                        fclose(fp);
                        snprintf(path, sizeof(path), "/sys/class/thermal/%.200s/temp", entry->d_name);
                        fp = fopen(path, "r");
                        if (fp) {
                            long temp_millidegrees = 0;
                            if (fscanf(fp, "%ld", &temp_millidegrees) == 1) {
                                double temp_c = (double)temp_millidegrees / 1000.0;
                                // Apply to all cores if it's a package temp
                                if (strstr(type, "x86_pkg_temp") || strstr(type, "Tdie")) {
                                    for (int i = 0; i < info->logical_cores; i++)
                                        info->core_temps[i] = temp_c;
                                    break; // Found package temp, done
                                } else if (temp_idx < info->logical_cores) {
                                    info->core_temps[temp_idx++] = temp_c;
                                }
                            }
                            fclose(fp);
                        }
                    } else {
                        fclose(fp);
                    }
                } else {
                    fclose(fp);
                }
            }
        }
        closedir(thermal_dir);
    }

    // Try coretemp if thermal zones didn't work
    bool has_valid_temp = false;
    for (int i = 0; i < info->logical_cores; i++) {
        if (info->core_temps[i] > -500.0) {
            has_valid_temp = true;
            break;
        }
    }

    if (!has_valid_temp) {
        // Try /sys/devices/platform/coretemp.*/hwmon*/
        DIR *platform_dir = opendir("/sys/devices/platform");
        if (platform_dir) {
            struct dirent *entry;
            while ((entry = readdir(platform_dir)) != NULL) {
                if (strncmp(entry->d_name, "coretemp.", 9) != 0)
                    continue;

                char hwmon_path[512];
                snprintf(hwmon_path, sizeof(hwmon_path), "/sys/devices/platform/%.200s/hwmon", entry->d_name);
                DIR *hwmon_dir = opendir(hwmon_path);
                if (hwmon_dir) {
                    struct dirent *hwmon_entry;
                    while ((hwmon_entry = readdir(hwmon_dir)) != NULL) {
                        if (hwmon_entry->d_name[0] == '.')
                            continue;

                        for (int i = 0; i < info->logical_cores; i++) {
                            char temp_path[768];
                            snprintf(temp_path, sizeof(temp_path), "/sys/devices/platform/%.200s/hwmon/%.200s/temp%d_input",
                                    entry->d_name, hwmon_entry->d_name, i + 1);
                            FILE *fp = fopen(temp_path, "r");
                            if (fp) {
                                long temp_millidegrees = 0;
                                if (fscanf(fp, "%ld", &temp_millidegrees) == 1) {
                                    info->core_temps[i] = (double)temp_millidegrees / 1000.0;
                                }
                                fclose(fp);
                            }
                        }
                    }
                    closedir(hwmon_dir);
                    break;
                }
            }
            closedir(platform_dir);
        }
    }

    return 0;
}

static int read_load_average(cpu_info_t *info) {
    if (!info)
        return -1;

    FILE *fp = fopen("/proc/loadavg", "r");
    if (!fp)
        return -1;

    if (fscanf(fp, "%lf %lf %lf", &info->load_avg_1min, &info->load_avg_5min, &info->load_avg_15min) != 3) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}

static int read_cpu_frequencies(cpu_info_t *info) {
    if (!info || info->logical_cores <= 0)
        return -1;

    if (!info->core_freqs) {
        info->core_freqs = calloc(info->logical_cores, sizeof(double));
        if (!info->core_freqs)
            return -1;
    }

    for (int i = 0; i < info->logical_cores; i++) {
        char path[128];
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", i);
        FILE *fp = fopen(path, "r");
        if (fp) {
            long freq_khz = 0;
            if (fscanf(fp, "%ld", &freq_khz) == 1) {
                info->core_freqs[i] = (double)freq_khz / 1000.0; // Convert to MHz
            }
            fclose(fp);
        } else {
            // Try cpuinfo_cur_freq as fallback
            snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_cur_freq", i);
            fp = fopen(path, "r");
            if (fp) {
                long freq_khz = 0;
                if (fscanf(fp, "%ld", &freq_khz) == 1) {
                    info->core_freqs[i] = (double)freq_khz / 1000.0;
                }
                fclose(fp);
            }
        }
    }
    return 0;
}

void cpu_info_init(cpu_info_t *info) {
    if (!info)
        return;
    memset(info, 0, sizeof(cpu_info_t));
    info->physical_cores = 0;
    info->logical_cores = 0;
    info->core_usage = NULL;
    info->core_temps = NULL;
    info->core_freqs = NULL;
    info->load_avg_1min = 0.0;
    info->load_avg_5min = 0.0;
    info->load_avg_15min = 0.0;
}

void cpu_info_free(cpu_info_t *info) {
    if (!info)
        return;
    free(info->core_usage);
    free(info->core_temps);
    free(info->core_freqs);
    info->core_usage = NULL;
    info->core_temps = NULL;
    info->core_freqs = NULL;
}

int read_full_cpu_info(cpu_info_t *info) {
    if (!info)
        return -1;

    if (read_cpu_info_static(info) != 0)
        return -1;

    read_per_core_usage(info);
    read_cpu_temperatures(info);
    read_load_average(info);
    read_cpu_frequencies(info);

    return 0;
}

double read_cpu_usage_percent(void) {
    static unsigned long long prev_total = 0;
    static unsigned long long prev_idle = 0;
    static bool have_prev = false;

    FILE *fp = fopen("/proc/stat", "r");
    if (!fp)
        return -1.0;

    char line[256];
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return -1.0;
    }
    fclose(fp);

    // Format: cpu  user nice system idle iowait irq softirq steal guest guest_nice
    char cpu_label[5];
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
    int parsed = sscanf(line, "%4s %llu %llu %llu %llu %llu %llu %llu %llu",
                        cpu_label, &user, &nice, &system, &idle,
                        &iowait, &irq, &softirq, &steal);
    if (parsed < 5)
        return -1.0;

    unsigned long long idle_all = idle + iowait;
    unsigned long long non_idle = user + nice + system + irq + softirq + steal;
    unsigned long long total = idle_all + non_idle;

    if (!have_prev) {
        prev_total = total;
        prev_idle = idle_all;
        have_prev = true;
        return -1.0;
    }

    unsigned long long total_delta = total - prev_total;
    unsigned long long idle_delta = idle_all - prev_idle;
    prev_total = total;
    prev_idle = idle_all;

    if (total_delta == 0)
        return -1.0;

    double usage = (double)(total_delta - idle_delta) / (double)total_delta * 100.0;
    if (usage < 0.0)
        usage = 0.0;
    if (usage > 100.0)
        usage = 100.0;
    return usage;
}

void render_cpu_panel(const cupid_config *config, const cpu_info_t *cpu, int start_row, int cols) {
    if (!cpu || !config->show_cpu_panel)
        return;

    int y = start_row;
    int x = 2;

    if (has_colors())
        attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(y++, x, "CPU");
    if (has_colors())
        attroff(COLOR_PAIR(1) | A_BOLD);

    // CPU Model
    if (cpu->model_name[0]) {
        char model_display[80];
        strncpy(model_display, cpu->model_name, sizeof(model_display) - 1);
        model_display[sizeof(model_display) - 1] = '\0';
        // Truncate if too long
        if (strlen(model_display) > 75) {
            model_display[72] = '.';
            model_display[73] = '.';
            model_display[74] = '.';
            model_display[75] = '\0';
        }
        mvprintw(y++, x, "  Model: %s", model_display);
    }

    // Cores and Threads
    mvprintw(y++, x, "  Cores: %d physical, %d logical", cpu->physical_cores, cpu->logical_cores);

    // Load Average with bars (like btop)
    if (cpu->load_avg_1min > 0.0 || cpu->load_avg_5min > 0.0 || cpu->load_avg_15min > 0.0) {
        // Calculate load as percentage of logical cores
        double load_1_pct = (cpu->load_avg_1min / (double)cpu->logical_cores) * 100.0;
        double load_5_pct = (cpu->load_avg_5min / (double)cpu->logical_cores) * 100.0;
        double load_15_pct = (cpu->load_avg_15min / (double)cpu->logical_cores) * 100.0;
        if (load_1_pct > 100.0) load_1_pct = 100.0;
        if (load_5_pct > 100.0) load_5_pct = 100.0;
        if (load_15_pct > 100.0) load_15_pct = 100.0;
        
        int bar_x = x + 11; // "  Load 15m:" = 11 chars
        int bar_width = cols - bar_x - 20; // Leave room for text
        if (bar_width < 10) bar_width = 10;
        if (bar_width > 50) bar_width = 50;
        
        // 1min load
        mvprintw(y, x, "  Load 1m:");
        draw_progress_bar(y, bar_x, load_1_pct, bar_width, true);
        mvprintw(y, bar_x + bar_width + 2, "%.2f", cpu->load_avg_1min);
        y++;
        
        // 5min load
        mvprintw(y, x, "  Load 5m:");
        draw_progress_bar(y, bar_x, load_5_pct, bar_width, true);
        mvprintw(y, bar_x + bar_width + 2, "%.2f", cpu->load_avg_5min);
        y++;
        
        // 15min load
        mvprintw(y, x, "  Load 15m:");
        draw_progress_bar(y, bar_x, load_15_pct, bar_width, true);
        mvprintw(y, bar_x + bar_width + 2, "%.2f", cpu->load_avg_15min);
        y++;
    }

    // Per-core usage with bars (column-based layout: C0, C3 on row 1; C1, C4 on row 2; etc.)
    if (config->cpu_show_per_core && cpu->core_usage && cpu->logical_cores > 0) {
        // Add blank line after load average
        y++;
        // Align with load average bars - "  Load 15m:" = 11 chars, so bar starts at x + 11
        int load_label_width = 11; // "  Load 15m:"
        int bar_start_x = x + load_label_width;
        
        // Calculate grid layout: number of columns based on available width
        // Format: "C19 [####] 12.5% " = 4 (label) + 10 (bar) + 6 (percentage) + 1 (space) = 21 chars
        int label_width = 4;      // "C19 " (accommodates up to C99)
        int bar_width = 10;       // "[########]"
        int percent_width = 6;    // " 12.5%"
        int spacing = 1;          // Space between columns
        int core_width = label_width + bar_width + percent_width + spacing; // 21 total
        int num_columns = (cols - bar_start_x - 4) / core_width;
        if (num_columns < 1) num_columns = 1;
        if (num_columns > cpu->logical_cores) num_columns = cpu->logical_cores;
        if (num_columns > 8) num_columns = 8; // Max 8 columns for readability
        
        int num_rows = (cpu->logical_cores + num_columns - 1) / num_columns;
        
        // Display cores in column order: C0, C3, C6... then C1, C4, C7... then C2, C5, C8...
        for (int row = 0; row < num_rows; row++) {
            for (int col = 0; col < num_columns; col++) {
                int core_idx = col * num_rows + row;
                if (core_idx >= cpu->logical_cores)
                    break;
                
                // Calculate position: label starts before bar_start_x to align bars
                int label_x = bar_start_x - label_width;
                int display_x = label_x + col * core_width;
                double usage = cpu->core_usage[core_idx];
                if (usage < 0.0) usage = 0.0;
                if (usage > 100.0) usage = 100.0;
                
                // Label: C0, C1, etc. (fixed width, right-aligned number)
                if (core_idx < 10) {
                    mvprintw(y, display_x, "C%d  ", core_idx);
                } else {
                    mvprintw(y, display_x, "C%-2d ", core_idx);
                }
                int bar_x = display_x + label_width;
                
                // Bar (fixed width, aligned with load bars)
                draw_progress_bar(y, bar_x, usage, bar_width, true);
                int percent_x = bar_x + bar_width;
                
                // Percentage (with color coding, fixed width)
                if (has_colors()) {
                    if (usage > 80.0)
                        attron(COLOR_PAIR(2));
                    else if (usage > 50.0)
                        attron(COLOR_PAIR(1));
                }
                mvprintw(y, percent_x, "%5.1f%%", usage);
                if (has_colors())
                    attroff(COLOR_PAIR(1) | COLOR_PAIR(2));
            }
            y++;
        }
        
        // Add blank line between usage and temperatures
        y++;
        
        // Show temperatures in same column layout (if available)
        if (cpu->core_temps) {
            bool has_temps = false;
            for (int i = 0; i < cpu->logical_cores; i++) {
                if (cpu->core_temps[i] > -500.0) {
                    has_temps = true;
                    break;
                }
            }
            if (has_temps) {
                for (int row = 0; row < num_rows; row++) {
                    for (int col = 0; col < num_columns; col++) {
                        int core_idx = col * num_rows + row;
                        if (core_idx >= cpu->logical_cores)
                            break;
                        
                        // Calculate position: label starts before bar_start_x to align with usage labels
                        int label_x = bar_start_x - label_width;
                        int display_x = label_x + col * core_width;
                        // Align temperature with the core label (same position as C0, C1, etc.)
                        double temp = cpu->core_temps[core_idx];
                        if (temp > -500.0) {
                            // Format: "C1  64C" - align with label position, match usage format exactly
                            if (core_idx < 10) {
                                mvprintw(y, display_x, "C%d  %2.0fC", core_idx, temp);
                            } else {
                                mvprintw(y, display_x, "C%-2d %2.0fC", core_idx, temp);
                            }
                        } else {
                            // Format: "C1   N/A" - align with label position
                            if (core_idx < 10) {
                                mvprintw(y, display_x, "C%d   N/A", core_idx);
                            } else {
                                mvprintw(y, display_x, "C%-2d  N/A", core_idx);
                            }
                        }
                    }
                    y++;
                }
            }
        }
    } else if (cpu->core_usage && cpu->logical_cores > 0) {
        // Show average temp if available
        double avg_temp = -999.0;
        int temp_count = 0;
        if (cpu->core_temps) {
            for (int i = 0; i < cpu->logical_cores; i++) {
                if (cpu->core_temps[i] > -500.0) {
                    if (avg_temp < -500.0)
                        avg_temp = 0.0;
                    avg_temp += cpu->core_temps[i];
                    temp_count++;
                }
            }
            if (temp_count > 0)
                avg_temp /= temp_count;
        }

        if (avg_temp > -500.0) {
            mvprintw(y++, x, "  Temp: %.1fC", avg_temp);
        }

        // Show average/min/max frequency if available
        if (cpu->core_freqs) {
            double min_freq = 0.0, max_freq = 0.0, avg_freq = 0.0;
            int freq_count = 0;
            for (int i = 0; i < cpu->logical_cores; i++) {
                if (cpu->core_freqs[i] > 0.0) {
                    if (freq_count == 0) {
                        min_freq = max_freq = cpu->core_freqs[i];
                    } else {
                        if (cpu->core_freqs[i] < min_freq)
                            min_freq = cpu->core_freqs[i];
                        if (cpu->core_freqs[i] > max_freq)
                            max_freq = cpu->core_freqs[i];
                    }
                    avg_freq += cpu->core_freqs[i];
                    freq_count++;
                }
            }
            if (freq_count > 0) {
                avg_freq /= freq_count;
                if (min_freq == max_freq) {
                    mvprintw(y++, x, "  Freq: %.0f MHz", avg_freq);
                } else {
                    mvprintw(y++, x, "  Freq: %.0f-%.0f MHz (avg: %.0f)", min_freq, max_freq, avg_freq);
                }
            }
        }
    }
}

