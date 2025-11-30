/*
cuPID is a simple process manager for Linux 
written by @frankischilling
11/27/2025 thanksgiving :) turkey day!
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <stdbool.h>
#include <ctype.h>
#include <strings.h>
#include <limits.h>
#include <dirent.h>
#include <unistd.h>
#include <ncurses.h>
#include "config.h"
#include "process.h"
#include "cpu.h"
#include "memory.h"

static double timespec_elapsed(struct timespec prev, struct timespec current) {
    double sec = (double)(current.tv_sec - prev.tv_sec);
    double nsec = (double)(current.tv_nsec - prev.tv_nsec) / 1e9;
    return sec + nsec;
}

static int get_column_width(const char *column, int remaining_space, int remaining_columns) {
    if (remaining_columns <= 1)
        return remaining_space;
    if (strcasecmp(column, "pid") == 0 || strcasecmp(column, "ppid") == 0)
        return 7;
    if (strcasecmp(column, "user") == 0)
        return 12;
    if (strcasecmp(column, "state") == 0)
        return 4;
    if (strcasecmp(column, "cpu") == 0)
        return 8;
    if (strcasecmp(column, "mem") == 0)
        return 8;
    if (strcasecmp(column, "rss") == 0 || strcasecmp(column, "vms") == 0)
        return 10;
    if (strcasecmp(column, "threads") == 0)
        return 8;
    return 12;
}
static void format_column_value(const cupid_config *config,
                                const process_info *info,
                                const char *column,
                                char *buffer,
                                size_t len) {
    if (!buffer || len == 0 || !info || !column)
        return;

    if (strcasecmp(column, "pid") == 0)
        snprintf(buffer, len, "%5d", info->pid);
    else if (strcasecmp(column, "ppid") == 0)
        snprintf(buffer, len, "%5d", info->ppid);
    else if (strcasecmp(column, "user") == 0)
        snprintf(buffer, len, "%s", info->user);
    else if (strcasecmp(column, "state") == 0)
        snprintf(buffer, len, "%c", info->state);
    else if (strcasecmp(column, "cpu") == 0)
        snprintf(buffer, len, "%5.1f%%", info->cpu_percent);
    else if (strcasecmp(column, "mem") == 0)
        snprintf(buffer, len, "%5.1f%%", info->mem_percent);
    else if (strcasecmp(column, "threads") == 0)
        snprintf(buffer, len, "%4d", info->threads);
    else if (strcasecmp(column, "rss") == 0)
        format_size_kb_units(info->rss_kb, config, buffer, len);
    else if (strcasecmp(column, "vms") == 0)
        format_size_kb_units(info->vms_kb, config, buffer, len);
    else if (strcasecmp(column, "command") == 0)
        snprintf(buffer, len, "%s", info->command);
    else
        snprintf(buffer, len, "-");
}

static void uppercase_copy(const char *src, char *dest, size_t len) {
    if (!src || !dest || len == 0)
        return;
    size_t i = 0;
    for (; src[i] && i < len - 1; ++i) {
        dest[i] = (char)toupper((unsigned char)src[i]);
    }
    dest[i] = '\0';
}

static void dfs_children(const process_list *list,
                         size_t idx,
                         int depth,
                         size_t *order,
                         int *depths,
                         size_t *out_count,
                         bool *visited) {
    if (!list || !order || !depths || !out_count || !visited)
        return;
    if (visited[idx])
        return;
    visited[idx] = true;
    order[*out_count] = idx;
    depths[*out_count] = depth;
    (*out_count)++;

    pid_t parent_pid = list->items[idx].pid;
    for (size_t i = 0; i < list->count; ++i) {
        if (!visited[i] && list->items[i].ppid == parent_pid) {
            dfs_children(list, i, depth + 1, order, depths, out_count, visited);
        }
    }
}

static void build_row_order(const cupid_config *config,
                            const process_list *list,
                            size_t *order,
                            int *depths,
                            size_t *out_count) {
    *out_count = 0;
    if (!config || !list || !order || !depths)
        return;

    size_t n = list->count;
    if (n == 0)
        return;

    if (config->tree_view_default == TREE_VIEW_FLAT) {
        for (size_t i = 0; i < n; ++i) {
            order[i] = i;
            depths[i] = 0;
        }
        *out_count = n;
        return;
    }

    bool *is_root = calloc(n, sizeof(bool));
    bool *visited = calloc(n, sizeof(bool));
    if (!is_root || !visited) {
        free(is_root);
        free(visited);
        for (size_t i = 0; i < n; ++i) {
            order[i] = i;
            depths[i] = 0;
        }
        *out_count = n;
        return;
    }

    for (size_t i = 0; i < n; ++i) {
        is_root[i] = true;
    }
    for (size_t i = 0; i < n; ++i) {
        pid_t ppid = list->items[i].ppid;
        for (size_t j = 0; j < n; ++j) {
            if (list->items[j].pid == ppid) {
                is_root[i] = false;
                break;
            }
        }
    }

    if (config->tree_view_default == TREE_VIEW_EXPANDED) {
        for (size_t i = 0; i < n; ++i) {
            if (is_root[i]) {
                dfs_children(list, i, 0, order, depths, out_count, visited);
            }
        }
        for (size_t i = 0; i < n; ++i) {
            if (!visited[i]) {
                dfs_children(list, i, 0, order, depths, out_count, visited);
            }
        }
    } else { // TREE_VIEW_COLLAPSED
        for (size_t i = 0; i < n; ++i) {
            if (is_root[i]) {
                order[*out_count] = i;
                depths[*out_count] = 0;
                (*out_count)++;
                visited[i] = true;
            }
        }
        for (size_t i = 0; i < n; ++i) {
            if (!visited[i]) {
                order[*out_count] = i;
                depths[*out_count] = 0;
                (*out_count)++;
            }
        }
    }

    free(is_root);
    free(visited);
}

static void render_process_table(const cupid_config *config,
                                 const process_list *list,
                                 int selected_row,
                                 int scroll_offset,
                                 int *visible_rows,
                                 int *total_rows,
                                 int table_start) {
    if (!config || !list) {
        if (visible_rows)
            *visible_rows = 0;
        if (total_rows)
            *total_rows = 0;
        return;
    }

    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    (void)cols;
    int usable_height = rows - table_start - 3;
    if (usable_height <= 1) {
        if (visible_rows)
            *visible_rows = 0;
        if (total_rows)
            *total_rows = 0;
        return;
    }

    char columns_buffer[sizeof(config->columns)];
    strncpy(columns_buffer, config->columns, sizeof(columns_buffer) - 1);
    columns_buffer[sizeof(columns_buffer) - 1] = '\0';

    char *tokens[16];
    int token_count = 0;
    char *saveptr = NULL;
    char *token = strtok_r(columns_buffer, ",", &saveptr);
    while (token && token_count < 16) {
        while (*token == ' ' || *token == '\t')
            token++;
        size_t len = strlen(token);
        while (len > 0 && (token[len - 1] == ' ' || token[len - 1] == '\t')) {
            token[len - 1] = '\0';
            len--;
        }
        if (*token != '\0')
            tokens[token_count++] = token;
        token = strtok_r(NULL, ",", &saveptr);
    }

    if (token_count == 0) {
        tokens[0] = "pid";
        tokens[1] = "user";
        tokens[2] = "cpu";
        tokens[3] = "mem";
        tokens[4] = "command";
        token_count = 5;
    }

    size_t n = list->count;
    size_t *order = NULL;
    int *depths = NULL;

    if (n > 0) {
        order = malloc(n * sizeof(size_t));
        depths = malloc(n * sizeof(int));
    }

    size_t out_count = 0;
    if (order && depths) {
        build_row_order(config, list, order, depths, &out_count);
    } else {
        for (size_t i = 0; i < n; ++i) {
            order[i] = i;
            depths[i] = 0;
        }
        out_count = n;
    }

    int threads_index = -1;
    for (int i = 0; i < token_count; ++i) {
        if (strcasecmp(tokens[i], "threads") == 0) {
            threads_index = i;
            break;
        }
    }
    int threads_width_fixed = 0;
    if (threads_index >= 0) {
        int fake_remaining_columns = token_count - threads_index;
        if (fake_remaining_columns <= 1)
            fake_remaining_columns = 2; // force non-"last column" width calculation
        threads_width_fixed = get_column_width("threads", cols - 4, fake_remaining_columns);
    }

    int header_row = table_start;
    int x = 2;
    if (config->show_header) {
        if (rows > table_start - 1)
            mvhline(table_start - 1, 1, ACS_HLINE, cols - 2);
        if (has_colors())
            attron(COLOR_PAIR(1) | A_BOLD);
        for (int i = 0; i < token_count; ++i) {
            int remaining_space = cols - x - 1;
            if (remaining_space <= 1)
                break;
            bool is_command_col = (strcasecmp(tokens[i], "command") == 0);
            bool is_threads_col = (strcasecmp(tokens[i], "threads") == 0);
            bool has_trailing_threads = (threads_index > i && threads_width_fixed > 0);

            int width;
            if (is_command_col && has_trailing_threads && config->command_max_width < 0) {
                width = remaining_space - threads_width_fixed;
                if (width < 4)
                    width = remaining_space - threads_width_fixed / 2;
                if (width < 4)
                    width = remaining_space;
            } else if (is_command_col && has_trailing_threads) {
                width = remaining_space;
            } else if (is_threads_col && threads_width_fixed > 0) {
                width = threads_width_fixed;
            } else if (is_command_col && (threads_index < 0 || threads_index <= i) &&
                       (token_count - i <= 1)) {
                width = remaining_space;
            } else if (is_threads_col && threads_width_fixed > 0) {
                width = threads_width_fixed;
            } else {
                width = get_column_width(tokens[i], remaining_space, token_count - i);
            }

            if (is_command_col && config->command_max_width > 0) {
                int maxw = config->command_max_width + 1; // leave at least 1 char padding
                if (width > maxw)
                    width = maxw;
            }

            if (width > remaining_space)
                width = remaining_space;
            if (width <= 1)
                break;
            char header[32];
            uppercase_copy(tokens[i], header, sizeof(header));
            mvprintw(header_row, x, "%-*s", width - 1, header);
            x += width;
        }
        if (has_colors())
            attroff(COLOR_PAIR(1) | A_BOLD);
    }

    int total = (int)out_count;
    if (total_rows)
        *total_rows = total;

    if (scroll_offset < 0)
        scroll_offset = 0;
    if (scroll_offset > total - 1)
        scroll_offset = total > 0 ? total - 1 : 0;

    size_t max_rows = 0;
    if (total > 0) {
        max_rows = (size_t)(total - scroll_offset);
        if ((int)max_rows > usable_height - 1)
            max_rows = (size_t)(usable_height - 1);
    }

    int first_row = table_start + (config->show_header ? 1 : 0);

    if (visible_rows)
        *visible_rows = (int)max_rows;

    if (max_rows == 0) {
        mvprintw(first_row, 2, "No processes to display.");
        goto cleanup;
    }

    for (size_t row = 0; row < max_rows; ++row) {
        int logical_row = scroll_offset + (int)row;
        size_t idx = order[logical_row];
        int depth = depths[logical_row];
        const process_info *info = &list->items[idx];
        int y = first_row + (int)row;
        x = 2;

        bool is_selected = (config->highlight_selected && logical_row == selected_row);
        if (is_selected) {
            if (has_colors())
                attron(COLOR_PAIR(3) | A_REVERSE);
            else
                attron(A_REVERSE);
        }
        for (int col = 0; col < token_count; ++col) {
            int remaining_space = cols - x - 1;
            if (remaining_space <= 1)
                break;

            bool is_command_col = (strcasecmp(tokens[col], "command") == 0);
            bool is_threads_col = (strcasecmp(tokens[col], "threads") == 0);
            bool is_last_col = (col == token_count - 1);
            bool has_trailing_threads = (threads_index > col && threads_width_fixed > 0);

            int width;
            if (is_command_col && has_trailing_threads && config->command_max_width < 0) {
                width = remaining_space - threads_width_fixed;
                if (width < 4)
                    width = remaining_space - threads_width_fixed / 2;
                if (width < 4)
                    width = remaining_space;
            } else if (is_command_col && has_trailing_threads) {
                width = remaining_space;
            } else if (is_command_col && (threads_index < 0 || threads_index <= col) && is_last_col) {
                width = remaining_space;
            } else if (is_threads_col && threads_width_fixed > 0) {
                width = threads_width_fixed;
            } else {
                width = get_column_width(tokens[col], remaining_space, token_count - col);
            }

            if (is_command_col && config->command_max_width > 0) {
                int maxw = config->command_max_width + 1;
                if (width > maxw)
                    width = maxw;
            }

            if (width > remaining_space)
                width = remaining_space;
            if (width <= 1)
                break;

            char value[256];
            format_column_value(config, info, tokens[col], value, sizeof(value));

            if (config->tree_view_default != TREE_VIEW_FLAT &&
                is_command_col &&
                depth > 0) {
                char indented[256];
                int indent_spaces = depth * 2;
                if (indent_spaces > width - 2)
                    indent_spaces = width - 2;
                if (indent_spaces < 0)
                    indent_spaces = 0;
                int written = snprintf(indented, sizeof(indented), "%*s%s", indent_spaces, "", value);
                if (written < 0)
                    written = 0;
                if ((size_t)written >= sizeof(indented))
                    written = (int)sizeof(indented) - 1;
                indented[written] = '\0';
                strncpy(value, indented, sizeof(value) - 1);
                value[sizeof(value) - 1] = '\0';
            }

            if (is_command_col && (is_last_col || (threads_index > col && threads_width_fixed > 0))) {
                // For command when it is effectively the wide column, print up to remaining width, no padding
                mvprintw(y, x, "%.*s", width - 1, value);
            } else {
                mvprintw(y, x, "%-*.*s", width - 1, width - 1, value);
            }
            x += width;
        }

        if (is_selected) {
            if (has_colors())
                attroff(COLOR_PAIR(3) | A_REVERSE);
            else
                attroff(A_REVERSE);
        }
    }

cleanup:
    free(order);
    free(depths);
}

typedef enum {
    VIEW_CPU_MEMORY = 0,  // Detailed CPU and memory panels
    VIEW_PROCESSES = 1    // Minimal CPU/memory, more processes
} view_mode_t;

static void render_minimal_cpu_memory(const cupid_config *config,
                                      double cpu_usage,
                                      const mem_info_t *mem_info,
                                      const cpu_info_t *cpu_info,
                                      int start_row,
                                      int cols) {
    (void)cols; // unused for now
    int y = start_row;
    int x = 2;
    
    // Minimal CPU info
    if (cpu_info && cpu_usage >= 0.0) {
        if (has_colors())
            attron(COLOR_PAIR(1) | A_BOLD);
        mvprintw(y, x, "CPU:");
        if (has_colors())
            attroff(COLOR_PAIR(1) | A_BOLD);
        mvprintw(y, x + 5, "%.1f%%", cpu_usage);
        if (cpu_info->load_avg_1min > 0.0) {
            mvprintw(y, x + 15, "Load: %.2f", cpu_info->load_avg_1min);
        }
        y++;
    }
    
    // Minimal Memory info
    if (mem_info && mem_info->total > 0) {
        double mem_percent = (double)mem_info->used / (double)mem_info->total * 100.0;
        char buf[32];
        if (has_colors())
            attron(COLOR_PAIR(1) | A_BOLD);
        mvprintw(y, x, "Mem:");
        if (has_colors())
            attroff(COLOR_PAIR(1) | A_BOLD);
        format_size_kb_units(mem_info->used, config, buf, sizeof(buf));
        mvprintw(y, x + 5, "%s/", buf);
        format_size_kb_units(mem_info->total, config, buf, sizeof(buf));
        mvprintw(y, x + 12, "%s", buf);
        mvprintw(y, x + 20, "(%.1f%%)", mem_percent);
        y++;
    }
}

static void render_ui(const cupid_config *config,
                      const process_list *list,
                      double cpu_usage,
                      const mem_info_t *mem_info,
                      const cpu_info_t *cpu_info,
                      int selected_row,
                      int scroll_offset,
                      int *visible_rows,
                      int *total_rows,
                      view_mode_t view_mode) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    (void)cols;
    erase();
    box(stdscr, 0, 0);

    mvprintw(1, 2, "cuPID  refresh=%d ms  sort=%s%s  processes=%zu",
             config->refresh_rate_ms,
             config->default_sort,
             config->sort_reverse ? " (desc)" : "",
             list->count);

    int panel_start_row = 2;
    
    // Render based on view mode
    if (view_mode == VIEW_PROCESSES) {
        // Process view: minimal CPU/memory info
        render_minimal_cpu_memory(config, cpu_usage, mem_info, cpu_info, panel_start_row, cols);
        panel_start_row += 2; // Minimal view takes 2 lines
    } else {
        // CPU/Memory view: detailed panels
        // CPU Panel
        if (config->show_cpu_panel && cpu_info) {
            render_cpu_panel(config, cpu_info, panel_start_row, cols);
            // Calculate CPU panel height - must match render_cpu_panel exactly
            int cpu_panel_height = 3; // base: title, model, cores
            // Load average now takes 3 lines (1min, 5min, 15min)
            if (cpu_info->load_avg_1min > 0.0 || cpu_info->load_avg_5min > 0.0 || cpu_info->load_avg_15min > 0.0) {
                cpu_panel_height += 3;
            }
            if (config->cpu_show_per_core && cpu_info->logical_cores > 0) {
                // Blank line after load average (only if per-core is enabled)
                cpu_panel_height += 1;
                // Per-core usage: column-based layout
                // Use same calculation as in render_cpu_panel
                int load_label_width = 11; // "  Load 15m:"
                int bar_start_x = 2 + load_label_width; // x + load_label_width
                int label_width = 4;
                int bar_width = 10;
                int percent_width = 6;
                int spacing = 1;
                int core_width = label_width + bar_width + percent_width + spacing; // 21
                int num_columns = (cols - bar_start_x - 4) / core_width;
                if (num_columns < 1) num_columns = 1;
                if (num_columns > cpu_info->logical_cores) num_columns = cpu_info->logical_cores;
                if (num_columns > 8) num_columns = 8;
                int num_rows = (cpu_info->logical_cores + num_columns - 1) / num_columns;
                cpu_panel_height += num_rows; // Usage rows
                // Blank line between usage and temperatures
                cpu_panel_height += 1;
                // Temperatures (if available): same column layout
                bool has_temps = false;
                if (cpu_info->core_temps) {
                    for (int i = 0; i < cpu_info->logical_cores; i++) {
                        if (cpu_info->core_temps[i] > -500.0) {
                            has_temps = true;
                            break;
                        }
                    }
                }
                if (has_temps) {
                    cpu_panel_height += num_rows; // Temperature rows
                }
            } else {
                if (cpu_info->core_temps) {
                    double avg_temp = -999.0;
                    int temp_count = 0;
                    for (int i = 0; i < cpu_info->logical_cores; i++) {
                        if (cpu_info->core_temps[i] > -500.0) {
                            if (avg_temp < -500.0) avg_temp = 0.0;
                            avg_temp += cpu_info->core_temps[i];
                            temp_count++;
                        }
                    }
                    if (temp_count > 0) cpu_panel_height++;
                }
                if (cpu_info->core_freqs) {
                    int freq_count = 0;
                    for (int i = 0; i < cpu_info->logical_cores; i++) {
                        if (cpu_info->core_freqs[i] > 0.0) {
                            freq_count++;
                            break;
                        }
                    }
                    if (freq_count > 0) cpu_panel_height++;
                }
            }
            panel_start_row += cpu_panel_height + 1; // +1 for spacing
        } else if (config->show_cpu_panel && cpu_usage >= 0.0) {
            // Fallback to simple CPU display
            mvprintw(panel_start_row, 2, "CPU: %.1f%%", cpu_usage);
            panel_start_row += 2;
        } else if (config->show_cpu_panel) {
            mvprintw(panel_start_row, 2, "CPU: --.-%%");
            panel_start_row += 2;
        }

        // Memory Panel
        if (config->show_memory_panel && mem_info) {
            render_memory_panel(config, mem_info, panel_start_row, cols);
            int mem_panel_height = 8; // base height
            if (config->show_swap)
                mem_panel_height += 4; // add swap section
            panel_start_row += mem_panel_height + 1; // +1 for spacing
        } else if (!config->show_cpu_panel) {
            mvprintw(panel_start_row, 2, "Columns: %s", config->columns);
            panel_start_row += 2;
        }
    }

    if (view_mode == VIEW_PROCESSES) {
        mvprintw(rows - 2, 2, "Press 'q' to exit, 'v' to switch view. Use Arrow up / down to move, PgUp/PgDn to scroll.");
    } else {
        mvprintw(rows - 2, 2, "Press 'q' to exit, 'v' to switch view. Use Arrow up / down to move, PgUp/PgDn to scroll.");
    }

    // Adjust table start based on panels
    int table_start = panel_start_row;
    render_process_table(config, list, selected_row, scroll_offset, visible_rows, total_rows, table_start);

    refresh();
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    cupid_config config;
    char config_path[PATH_MAX];
    const char *home = getenv("HOME");
    if (home && *home) {
        snprintf(config_path, sizeof(config_path), "%s/.config/cuPID/config.conf", home);
    } else {
        snprintf(config_path, sizeof(config_path), "./cuPID.conf");
    }
    if (config_load(&config, config_path) != 0) {
        fprintf(stderr,
                "cuPID: Running with built‑in defaults. Edit %s to customize.\n",
                config_path);
    } else {
        fprintf(stderr, "cuPID: Loaded configuration from %s\n", config_path);
    }
    
    // Initialize ncurses
    WINDOW *std = initscr();
    if (!std) {
        fprintf(stderr, "Failed to initialize ncurses.\n");
        return 1;
    }

    if (config.color_enabled && has_colors()) {
        start_color();
        use_default_colors();
        init_pair(1, COLOR_CYAN, -1);   // headers
        init_pair(2, COLOR_YELLOW, -1); // CPU-heavy
        init_pair(3, COLOR_GREEN, -1);  // selected/future use
    }
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    timeout(100); // short poll for input; data refresh is timed separately
    curs_set(0);
    
    // Enable mouse support for proper wheel handling
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);

    process_cache cache;
    process_cache_init(&cache);
    process_list plist;
    process_list_init(&plist);

    struct timespec last_data_refresh;
    clock_gettime(CLOCK_MONOTONIC, &last_data_refresh);
    const double refresh_interval = (config.refresh_rate_ms > 50
                                     ? (double)config.refresh_rate_ms
                                     : 50.0) / 1000.0;

    bool running = true;
    int selected_row = 0;
    int visible_rows = 0;
    int total_rows = 0;
    int scroll_offset = 0;
    bool have_data = false;
    double last_cpu_usage = -1.0;
    mem_info_t last_mem_info = {0};
    bool have_mem_info = false;
    cpu_info_t cpu_info;
    cpu_info_init(&cpu_info);
    bool have_cpu_info = false;
    view_mode_t view_mode = VIEW_CPU_MEMORY; // Start in CPU/Memory view
    struct timespec last_key_input = {0, 0};
    const double key_debounce_interval = 0.05; // 50ms debounce for key inputs
    while (running) {
        bool selection_changed = false;
        bool data_changed = false;

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        double elapsed = timespec_elapsed(last_data_refresh, now);

        if (!have_data || elapsed >= refresh_interval) {
            if (process_list_refresh(&plist, &cache, have_data ? elapsed : refresh_interval, &config) != 0) {
                mvprintw(1, 2, "Failed to read processes.");
                refresh();
            } else {
                have_data = true;
                last_data_refresh = now;
                data_changed = true;
                if (config.show_cpu_panel) {
                    last_cpu_usage = read_cpu_usage_percent();
                    if (read_full_cpu_info(&cpu_info) == 0)
                        have_cpu_info = true;
                }
                if (config.show_memory_panel) {
                    if (read_full_mem_info(&last_mem_info))
                        have_mem_info = true;
                }
            }
        }

        int ch = getch();
        if (ch == 'q' || ch == 'Q') {
            running = false;
        } else if (ch == 'v' || ch == 'V') {
            // Toggle view mode
            view_mode = (view_mode == VIEW_CPU_MEMORY) ? VIEW_PROCESSES : VIEW_CPU_MEMORY;
            selection_changed = true; // Force redraw
        } else if (ch == KEY_MOUSE) {
            // Handle mouse events, including wheel scrolling
            MEVENT event;
            if (getmouse(&event) == OK) {
                // Handle mouse wheel up (BUTTON4) - check both PRESSED and CLICKED
                if ((event.bstate & BUTTON4_PRESSED) || (event.bstate & BUTTON4_CLICKED)) {
                    // Mouse wheel up - move up 1 entry
                    if (selected_row > 0)
                        selected_row--;
                    selection_changed = true;
                    last_key_input = now; // Update debounce timer
                } 
                // Handle mouse wheel down (BUTTON5) - check both PRESSED and CLICKED
                else if ((event.bstate & BUTTON5_PRESSED) || (event.bstate & BUTTON5_CLICKED)) {
                    // Mouse wheel down - move down 1 entry
                    if (selected_row + 1 < total_rows)
                        selected_row++;
                    selection_changed = true;
                    last_key_input = now; // Update debounce timer
                }
            }
        } else if (ch == KEY_UP) {
            // Debounce: ignore if too soon after last key input (likely from mouse wheel)
            double key_elapsed = timespec_elapsed(last_key_input, now);
            if (key_elapsed >= key_debounce_interval || last_key_input.tv_sec == 0) {
                if (selected_row > 0)
                    selected_row--;
                selection_changed = true;
                last_key_input = now;
            }
        } else if (ch == KEY_DOWN) {
            // Debounce: ignore if too soon after last key input (likely from mouse wheel)
            double key_elapsed = timespec_elapsed(last_key_input, now);
            if (key_elapsed >= key_debounce_interval || last_key_input.tv_sec == 0) {
                if (selected_row + 1 < total_rows)
                    selected_row++;
                selection_changed = true;
                last_key_input = now;
            }
        } else if (ch == KEY_PPAGE) { // Page Up
            selected_row -= visible_rows > 0 ? visible_rows : 1;
            if (selected_row < 0)
                selected_row = 0;
            selection_changed = true;
        } else if (ch == KEY_NPAGE) { // Page Down
            selected_row += visible_rows > 0 ? visible_rows : 1;
            if (selected_row >= total_rows)
                selected_row = total_rows > 0 ? total_rows - 1 : 0;
            selection_changed = true;
        }

        if (have_data && (data_changed || selection_changed)) {
            if (selected_row >= (int)plist.count)
                selected_row = (int)plist.count - 1;
            if (selected_row < 0)
                selected_row = 0;

            if (selected_row < scroll_offset)
                scroll_offset = selected_row;
            if (visible_rows > 0 && selected_row >= scroll_offset + visible_rows)
                scroll_offset = selected_row - visible_rows + 1;
            if (scroll_offset < 0)
                scroll_offset = 0;

            render_ui(&config, &plist, last_cpu_usage, have_mem_info ? &last_mem_info : NULL,
                     have_cpu_info ? &cpu_info : NULL, selected_row, scroll_offset, &visible_rows, &total_rows, view_mode);
        }
    }

    process_list_free(&plist);
    process_cache_free(&cache);
    cpu_info_free(&cpu_info);
    endwin();
    return 0;
}