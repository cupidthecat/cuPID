#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "cupidconf.h"

static bool is_known_key(const char *key) {
    if (!key || !*key)
        return false;
    static const char *known[] = {
        "refresh_rate",
        "default_sort",
        "sort_reverse",
        "show_header",
        "color_enabled",
        "max_processes",
        "ui_layout",
        "show_cpu_panel",
        "show_memory_panel",
        "panel_height",
        "process_list_height",
        "columns",
        "default_filter",
        "show_threads",
        "tree_view_default",
        "highlight_selected",
        "cpu_show_per_core",
        "memory_units",
        "show_swap",
        "memory_show_free",
        "memory_show_available",
        "memory_show_cached",
        "memory_show_buffers",
        "disk_enabled",
        "network_enabled",
        "command_max_width",
        "cpu_group_mode",
        NULL
    };
    for (const char **p = known; *p; ++p) {
        if (strcasecmp(key, *p) == 0)
            return true;
    }
    return false;
}

static bool parse_bool(const char *value, bool fallback) {
    if (!value)
        return fallback;

    if (strcasecmp(value, "1") == 0 || strcasecmp(value, "true") == 0 ||
        strcasecmp(value, "yes") == 0 || strcasecmp(value, "on") == 0) {
        return true;
    }
    if (strcasecmp(value, "0") == 0 || strcasecmp(value, "false") == 0 ||
        strcasecmp(value, "no") == 0 || strcasecmp(value, "off") == 0) {
        return false;
    }
    return fallback;
}

static int parse_int(const char *value, int min, int max, int fallback) {
    if (!value)
        return fallback;

    char *end = NULL;
    long parsed = strtol(value, &end, 10);
    if (end == value || *end != '\0')
        return fallback;

    if (parsed < min)
        parsed = min;
    if (parsed > max)
        parsed = max;
    return (int)parsed;
}

static void copy_string(char *dest, size_t dest_size, const char *value, const char *fallback) {
    const char *src = value && *value ? value : fallback;
    if (!src)
        src = "";
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
}

const char *tree_view_mode_to_string(tree_view_mode_t mode) {
    switch (mode) {
        case TREE_VIEW_EXPANDED:
            return "expanded";
        case TREE_VIEW_COLLAPSED:
            return "collapsed";
        case TREE_VIEW_FLAT:
        default:
            return "flat";
    }
}

static tree_view_mode_t parse_tree_mode(const char *value, tree_view_mode_t fallback) {
    if (!value)
        return fallback;

    if (strcasecmp(value, "expanded") == 0)
        return TREE_VIEW_EXPANDED;
    if (strcasecmp(value, "collapsed") == 0)
        return TREE_VIEW_COLLAPSED;
    if (strcasecmp(value, "flat") == 0)
        return TREE_VIEW_FLAT;
    return fallback;
}

static bool validate_sort_key(const char *key) {
    return (strcasecmp(key, "cpu") == 0 || strcasecmp(key, "memory") == 0 ||
            strcasecmp(key, "pid") == 0 || strcasecmp(key, "name") == 0);
}

static bool validate_ui_layout(const char *layout) {
    return (strcasecmp(layout, "compact") == 0 || strcasecmp(layout, "detailed") == 0 ||
            strcasecmp(layout, "minimal") == 0);
}

static bool validate_memory_units(const char *units) {
    return (strcasecmp(units, "kb") == 0 || strcasecmp(units, "mb") == 0 ||
            strcasecmp(units, "gb") == 0 || strcasecmp(units, "auto") == 0);
}

void config_apply_defaults(cupid_config *cfg) {
    if (!cfg)
        return;

    cfg->refresh_rate_ms = 1000;
    copy_string(cfg->default_sort, sizeof(cfg->default_sort), "cpu", "cpu");
    cfg->sort_reverse = false;
    cfg->show_header = true;
    cfg->color_enabled = true;
    cfg->max_processes = 0;

    copy_string(cfg->ui_layout, sizeof(cfg->ui_layout), "detailed", "detailed");
    cfg->show_cpu_panel = true;
    cfg->show_memory_panel = true;
    cfg->panel_height = 3;
    cfg->process_list_height = -1;

    copy_string(cfg->columns, sizeof(cfg->columns), "pid,user,cpu,mem,command,threads",
                "pid,user,cpu,mem,command,threads");
    copy_string(cfg->default_filter, sizeof(cfg->default_filter), "", "");
    cfg->show_threads = false;
    cfg->tree_view_default = TREE_VIEW_FLAT;
    cfg->highlight_selected = true;

    cfg->cpu_show_per_core = false;
    copy_string(cfg->memory_units, sizeof(cfg->memory_units), "auto", "auto");
    cfg->show_swap = true;
    cfg->disk_enabled = false;
    cfg->network_enabled = false;

    cfg->memory_show_free = true;
    cfg->memory_show_available = true;
    cfg->memory_show_cached = true;
    cfg->memory_show_buffers = true;

    cfg->command_max_width = -1;
    cfg->cpu_group_mode = CPU_GROUP_FLAT;
}

static void apply_overrides_from_conf(cupid_config *cfg, cupidconf_t *conf) {
    const char *value = NULL;

    value = cupidconf_get(conf, "refresh_rate");
    if (value)
        cfg->refresh_rate_ms = parse_int(value, 100, 60000, cfg->refresh_rate_ms);

    value = cupidconf_get(conf, "default_sort");
    if (value && validate_sort_key(value))
        copy_string(cfg->default_sort, sizeof(cfg->default_sort), value, cfg->default_sort);

    value = cupidconf_get(conf, "sort_reverse");
    cfg->sort_reverse = parse_bool(value, cfg->sort_reverse);

    value = cupidconf_get(conf, "show_header");
    cfg->show_header = parse_bool(value, cfg->show_header);

    value = cupidconf_get(conf, "color_enabled");
    cfg->color_enabled = parse_bool(value, cfg->color_enabled);

    value = cupidconf_get(conf, "max_processes");
    if (value)
        cfg->max_processes = parse_int(value, 0, 100000, cfg->max_processes);

    value = cupidconf_get(conf, "ui_layout");
    if (value && validate_ui_layout(value))
        copy_string(cfg->ui_layout, sizeof(cfg->ui_layout), value, cfg->ui_layout);

    value = cupidconf_get(conf, "show_cpu_panel");
    cfg->show_cpu_panel = parse_bool(value, cfg->show_cpu_panel);

    value = cupidconf_get(conf, "show_memory_panel");
    cfg->show_memory_panel = parse_bool(value, cfg->show_memory_panel);

    value = cupidconf_get(conf, "panel_height");
    if (value)
        cfg->panel_height = parse_int(value, 1, 10, cfg->panel_height);

    value = cupidconf_get(conf, "process_list_height");
    if (value)
        cfg->process_list_height = parse_int(value, -1, 1000, cfg->process_list_height);

    value = cupidconf_get(conf, "columns");
    if (value)
        copy_string(cfg->columns, sizeof(cfg->columns), value, cfg->columns);

    value = cupidconf_get(conf, "default_filter");
    if (value)
        copy_string(cfg->default_filter, sizeof(cfg->default_filter), value, cfg->default_filter);

    value = cupidconf_get(conf, "show_threads");
    cfg->show_threads = parse_bool(value, cfg->show_threads);

    value = cupidconf_get(conf, "tree_view_default");
    cfg->tree_view_default = parse_tree_mode(value, cfg->tree_view_default);

    value = cupidconf_get(conf, "highlight_selected");
    cfg->highlight_selected = parse_bool(value, cfg->highlight_selected);

    value = cupidconf_get(conf, "cpu_show_per_core");
    cfg->cpu_show_per_core = parse_bool(value, cfg->cpu_show_per_core);

    value = cupidconf_get(conf, "memory_units");
    if (value && validate_memory_units(value))
        copy_string(cfg->memory_units, sizeof(cfg->memory_units), value, cfg->memory_units);

    value = cupidconf_get(conf, "show_swap");
    cfg->show_swap = parse_bool(value, cfg->show_swap);

    value = cupidconf_get(conf, "memory_show_free");
    cfg->memory_show_free = parse_bool(value, cfg->memory_show_free);

    value = cupidconf_get(conf, "memory_show_available");
    cfg->memory_show_available = parse_bool(value, cfg->memory_show_available);

    value = cupidconf_get(conf, "memory_show_cached");
    cfg->memory_show_cached = parse_bool(value, cfg->memory_show_cached);

    value = cupidconf_get(conf, "memory_show_buffers");
    cfg->memory_show_buffers = parse_bool(value, cfg->memory_show_buffers);

    value = cupidconf_get(conf, "disk_enabled");
    cfg->disk_enabled = parse_bool(value, cfg->disk_enabled);

    value = cupidconf_get(conf, "network_enabled");
    cfg->network_enabled = parse_bool(value, cfg->network_enabled);

    value = cupidconf_get(conf, "command_max_width");
    if (value) {
        int w = parse_int(value, -1, 512, cfg->command_max_width);
        cfg->command_max_width = w;
    }

    value = cupidconf_get(conf, "cpu_group_mode");
    if (value) {
        if (strcasecmp(value, "aggregate") == 0)
            cfg->cpu_group_mode = CPU_GROUP_AGGREGATE;
        else
            cfg->cpu_group_mode = CPU_GROUP_FLAT;
    }
}

int config_load(cupid_config *cfg, const char *path) {
    if (!cfg || !path)
        return -1;

    config_apply_defaults(cfg);

    cupidconf_t *conf = cupidconf_load(path);
    if (!conf) {
        fprintf(stderr,
                "cuPID: Failed to load config file '%s'. Using built‑in defaults.\n",
                path);
        return -1;
    }

    apply_overrides_from_conf(cfg, conf);

    // Warn about unknown / misspelled keys
    for (cupidconf_entry *e = conf->entries; e; e = e->next) {
        if (!is_known_key(e->key)) {
            fprintf(stderr,
                    "cuPID: Unknown config option '%s' in %s (ignored).\n",
                    e->key,
                    path);
        }
    }

    cupidconf_free(conf);
    return 0;
}

