#pragma once

#include <stdbool.h>

typedef enum {
    TREE_VIEW_FLAT = 0,
    TREE_VIEW_EXPANDED,
    TREE_VIEW_COLLAPSED
} tree_view_mode_t;

typedef enum {
    CPU_GROUP_FLAT = 0,
    CPU_GROUP_AGGREGATE,
} cpu_group_mode_t;

typedef struct cupid_config {
    int refresh_rate_ms;
    char default_sort[16];
    bool sort_reverse;
    bool show_header;
    bool color_enabled;
    int max_processes;

    char ui_layout[16];
    bool show_cpu_panel;
    bool show_memory_panel;
    int panel_height;
    int process_list_height; /* -1 means automatic */

    char columns[128];
    char default_filter[128];
    bool show_threads;
    tree_view_mode_t tree_view_default;
    bool highlight_selected;

    bool cpu_show_per_core;
    char memory_units[8];
    bool show_swap;
    bool disk_enabled;
    bool network_enabled;

    bool memory_show_free;
    bool memory_show_available;
    bool memory_show_cached;
    bool memory_show_buffers;

    int command_max_width; /* 0 = auto, -1 = auto reserve for trailing columns */
    cpu_group_mode_t cpu_group_mode; /* how to interpret CPU in tree view */
} cupid_config;

void config_apply_defaults(cupid_config *cfg);
int config_load(cupid_config *cfg, const char *path);
const char *tree_view_mode_to_string(tree_view_mode_t mode);

