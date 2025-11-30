#include "memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ncurses.h>

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

void format_size_kb_units(long kb, const cupid_config *config, char *buffer, size_t len) {
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

bool read_full_mem_info(mem_info_t *info) {
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

void render_memory_panel(const cupid_config *config, const mem_info_t *mem, int start_row, int cols) {
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

