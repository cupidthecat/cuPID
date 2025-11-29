#pragma once

#include <stdbool.h>
#include "config.h"

typedef struct {
    char model_name[128];
    int physical_cores;
    int logical_cores;
    double *core_usage;  // per-core CPU usage percentages
    double *core_temps;  // per-core temperatures in Celsius
    double load_avg_1min;
    double load_avg_5min;
    double load_avg_15min;
    double *core_freqs;  // per-core frequencies in MHz
} cpu_info_t;

// CPU info management
void cpu_info_init(cpu_info_t *info);
void cpu_info_free(cpu_info_t *info);

// CPU data reading
int read_full_cpu_info(cpu_info_t *info);
double read_cpu_usage_percent(void);

// CPU panel rendering
void render_cpu_panel(const cupid_config *config, const cpu_info_t *cpu, int start_row, int cols);

