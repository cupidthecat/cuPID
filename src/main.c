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
#include <ncurses.h>
#include "config.h"
#include "process.h"

static double timespec_elapsed(struct timespec prev, struct timespec current) {
    double sec = (double)(current.tv_sec - prev.tv_sec);
    double nsec = (double)(current.tv_nsec - prev.tv_nsec) / 1e9;
    return sec + nsec;
}

static double read_cpu_usage_percent(void) {
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

typedef struct {
    long total;
    long used;
    long free;
    long available;
    long cached;
    long buffers;
    long swap_total;
    long swap_used;
    long swap_free;
} mem_info_t;

static bool read_full_mem_info(mem_info_t *info) {
    if (!info)
        return false;

    memset(info, 0, sizeof(mem_info_t));

    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp)
        return false;

    char key[64];
    long value = 0;
    char unit[32];
    int found = 0;

    while (fscanf(fp, "%63s %ld %31s", key, &value, unit) == 3) {
        if (strcmp(key, "MemTotal:") == 0) {
            info->total = value;
            found++;
        } else if (strcmp(key, "MemFree:") == 0) {
            info->free = value;
            found++;
        } else if (strcmp(key, "MemAvailable:") == 0) {
            info->available = value;
            found++;
        } else if (strcmp(key, "Cached:") == 0) {
            info->cached = value;
            found++;
        } else if (strcmp(key, "Buffers:") == 0) {
            info->buffers = value;
            found++;
        } else if (strcmp(key, "SwapTotal:") == 0) {
            info->swap_total = value;
            found++;
        } else if (strcmp(key, "SwapFree:") == 0) {
            info->swap_free = value;
            found++;
        }
        if (found >= 7)
            break;
    }
    fclose(fp);

    if (info->total <= 0)
        return false;

    info->used = info->total - info->available;
    if (info->used < 0)
        info->used = 0;

    info->swap_used = info->swap_total - info->swap_free;
    if (info->swap_used < 0)
        info->swap_used = 0;

    return true;
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

static void format_size_kb_auto(long kb, char *buffer, size_t len) {
    if (!buffer || len == 0)
        return;
    const double size = (double)kb;
    if (size >= 1024 * 1024) {
        snprintf(buffer, len, "%.1fG", size / (1024 * 1024));
    } else if (size >= 1024) {
        snprintf(buffer, len, "%.1fM", size / 1024);
    } else {
        snprintf(buffer, len, "%ldK", kb);
    }
}

static void format_size_kb_units(long kb, const cupid_config *config, char *buffer, size_t len) {
    if (!buffer || len == 0)
        return;
    if (!config || !config->memory_units[0]) {
        format_size_kb_auto(kb, buffer, len);
        return;
    }

    if (strcasecmp(config->memory_units, "kb") == 0) {
        snprintf(buffer, len, "%ldK", kb);
    } else if (strcasecmp(config->memory_units, "mb") == 0) {
        double mb = (double)kb / 1024.0;
        snprintf(buffer, len, "%.1fM", mb);
    } else if (strcasecmp(config->memory_units, "gb") == 0) {
        double gb = (double)kb / (1024.0 * 1024.0);
        snprintf(buffer, len, "%.2fG", gb);
    } else { // auto
        format_size_kb_auto(kb, buffer, len);
    }
}

static void render_memory_panel(const cupid_config *config, const mem_info_t *mem, int start_row, int cols) {
    (void)cols; // unused for now
    if (!mem)
        return;

    int y = start_row;
    char buf[32];

    if (has_colors())
        attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(y++, 2, "Memory");
    if (has_colors())
        attroff(COLOR_PAIR(1) | A_BOLD);

    double mem_percent = mem->total > 0 ? (double)mem->used / (double)mem->total * 100.0 : 0.0;
    format_size_kb_units(mem->total, config, buf, sizeof(buf));
    mvprintw(y++, 2, "  Total: %s", buf);
    format_size_kb_units(mem->used, config, buf, sizeof(buf));
    mvprintw(y++, 2, "  Used:  %s (%.1f%%)", buf, mem_percent);
    if (config->memory_show_free) {
        format_size_kb_units(mem->free, config, buf, sizeof(buf));
        mvprintw(y++, 2, "  Free:  %s", buf);
    }
    if (config->memory_show_available) {
        format_size_kb_units(mem->available, config, buf, sizeof(buf));
        mvprintw(y++, 2, "  Avail: %s", buf);
    }
    if (config->memory_show_cached) {
        format_size_kb_units(mem->cached, config, buf, sizeof(buf));
        mvprintw(y++, 2, "  Cached: %s", buf);
    }
    if (config->memory_show_buffers) {
        format_size_kb_units(mem->buffers, config, buf, sizeof(buf));
        mvprintw(y++, 2, "  Buffers: %s", buf);
    }

    if (config->show_swap && mem->swap_total > 0) {
        double swap_percent = mem->swap_total > 0
                                  ? (double)mem->swap_used / (double)mem->swap_total * 100.0
                                  : 0.0;
        mvprintw(y++, 2, "  Swap:");
        format_size_kb_units(mem->swap_total, config, buf, sizeof(buf));
        mvprintw(y++, 2, "    Total: %s", buf);
        format_size_kb_units(mem->swap_used, config, buf, sizeof(buf));
        mvprintw(y++, 2, "    Used:  %s (%.1f%%)", buf, swap_percent);
        format_size_kb_units(mem->swap_free, config, buf, sizeof(buf));
        mvprintw(y++, 2, "    Free:  %s", buf);
    }
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

static void render_ui(const cupid_config *config,
                      const process_list *list,
                      double cpu_usage,
                      const mem_info_t *mem_info,
                      int selected_row,
                      int scroll_offset,
                      int *visible_rows,
                      int *total_rows) {
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

    if (config->show_cpu_panel && cpu_usage >= 0.0) {
        mvprintw(2, 2, "CPU: %.1f%%", cpu_usage);
    } else if (config->show_cpu_panel) {
        mvprintw(2, 2, "CPU: --.-%%");
    }

    int panel_start_row = 3;
    if (config->show_memory_panel && mem_info) {
        render_memory_panel(config, mem_info, panel_start_row, cols);
        int mem_panel_height = 8; // base height
        if (config->show_swap)
            mem_panel_height += 4; // add swap section
        panel_start_row += mem_panel_height + 1; // +1 for spacing
    } else {
        mvprintw(panel_start_row, 2, "Columns: %s", config->columns);
        panel_start_row += 2;
    }

    mvprintw(rows - 2, 2, "Press 'q' to exit. Use Arrow up / down to move, PgUp/PgDn to scroll.");

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
                if (config.show_cpu_panel)
                    last_cpu_usage = read_cpu_usage_percent();
                if (config.show_memory_panel) {
                    if (read_full_mem_info(&last_mem_info))
                        have_mem_info = true;
                }
            }
        }

        int ch = getch();
        if (ch == 'q' || ch == 'Q') {
            running = false;
        } else if (ch == KEY_UP) {
            if (selected_row > 0)
                selected_row--;
            selection_changed = true;
        } else if (ch == KEY_DOWN) {
            if (selected_row + 1 < total_rows)
                selected_row++;
            selection_changed = true;
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

            render_ui(&config, &plist, last_cpu_usage, have_mem_info ? &last_mem_info : NULL, selected_row, scroll_offset, &visible_rows, &total_rows);
        }
    }

    process_list_free(&plist);
    process_cache_free(&cache);
    endwin();
    return 0;
}