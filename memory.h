#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "config.h"

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

// Memory data reading
bool read_full_mem_info(mem_info_t *info);

// Memory panel rendering
void render_memory_panel(const cupid_config *config, const mem_info_t *mem, int start_row, int cols);

// Memory formatting utilities (used by process table too)
void format_size_kb_units(long kb, const cupid_config *config, char *buffer, size_t len);

