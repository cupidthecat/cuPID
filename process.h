#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

typedef struct {
    pid_t pid;
    pid_t ppid;
    uid_t uid;
    char user[32];
    char state;
    char command[256];
    double cpu_percent;
    double mem_percent;
    long rss_kb;
    long vms_kb;
    int threads;
} process_info;

typedef struct {
    process_info *items;
    size_t count;
    size_t capacity;
} process_list;

typedef struct {
    pid_t pid;
    unsigned long long total_ticks;
} proc_cpu_entry;

typedef struct {
    proc_cpu_entry *entries;
    size_t count;
    size_t capacity;
} process_cache;

struct cupid_config;

void process_list_init(process_list *list);
void process_list_clear(process_list *list);
void process_list_free(process_list *list);

void process_cache_init(process_cache *cache);
void process_cache_free(process_cache *cache);

int process_list_refresh(process_list *list,
                         process_cache *cache,
                         double elapsed_seconds,
                         const struct cupid_config *config);

