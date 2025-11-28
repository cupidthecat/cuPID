#define _GNU_SOURCE

#include "process.h"

#include "config.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef MAX_CMD_DISPLAY
#define MAX_CMD_DISPLAY 240
#endif

static long g_page_size_kb = 0;
static long g_ticks_per_sec = 0;
static int g_cpu_count = 0;

static void ensure_system_constants(void) {
    if (g_page_size_kb == 0) {
        long ps = sysconf(_SC_PAGESIZE);
        g_page_size_kb = ps > 0 ? ps / 1024 : 4;
    }
    if (g_ticks_per_sec == 0) {
        long ticks = sysconf(_SC_CLK_TCK);
        g_ticks_per_sec = ticks > 0 ? ticks : 100;
    }
    if (g_cpu_count == 0) {
        long cpus = sysconf(_SC_NPROCESSORS_ONLN);
        g_cpu_count = cpus > 0 ? (int)cpus : 1;
    }
}

void process_list_init(process_list *list) {
    if (!list)
        return;
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void process_list_clear(process_list *list) {
    if (!list)
        return;
    list->count = 0;
}

void process_list_free(process_list *list) {
    if (!list)
        return;
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void process_cache_init(process_cache *cache) {
    if (!cache)
        return;
    cache->entries = NULL;
    cache->count = 0;
    cache->capacity = 0;
}

void process_cache_free(process_cache *cache) {
    if (!cache)
        return;
    free(cache->entries);
    cache->entries = NULL;
    cache->count = 0;
    cache->capacity = 0;
}

static int ensure_cache_capacity(process_cache *cache, size_t needed) {
    if (cache->capacity >= needed)
        return 0;
    size_t new_cap = cache->capacity ? cache->capacity * 2 : 256;
    while (new_cap < needed)
        new_cap *= 2;
    proc_cpu_entry *entries = realloc(cache->entries, new_cap * sizeof(proc_cpu_entry));
    if (!entries)
        return -1;
    cache->entries = entries;
    cache->capacity = new_cap;
    return 0;
}

static int ensure_list_capacity(process_list *list, size_t needed) {
    if (list->capacity >= needed)
        return 0;
    size_t new_cap = list->capacity ? list->capacity * 2 : 256;
    while (new_cap < needed)
        new_cap *= 2;
    process_info *items = realloc(list->items, new_cap * sizeof(process_info));
    if (!items)
        return -1;
    list->items = items;
    list->capacity = new_cap;
    return 0;
}

static unsigned long long cache_lookup(process_cache *cache, pid_t pid, bool *found) {
    if (found)
        *found = false;
    if (!cache || cache->count == 0)
        return 0;
    for (size_t i = 0; i < cache->count; ++i) {
        if (cache->entries[i].pid == pid) {
            if (found)
                *found = true;
            return cache->entries[i].total_ticks;
        }
    }
    return 0;
}

static int cache_append(process_cache *cache, pid_t pid, unsigned long long ticks) {
    if (!cache)
        return -1;
    if (ensure_cache_capacity(cache, cache->count + 1) != 0)
        return -1;
    cache->entries[cache->count].pid = pid;
    cache->entries[cache->count].total_ticks = ticks;
    cache->count++;
    return 0;
}

static void cache_replace(process_cache *cache, process_cache *replacement) {
    if (!cache || !replacement)
        return;
    free(cache->entries);
    *cache = *replacement;
    replacement->entries = NULL;
    replacement->count = 0;
    replacement->capacity = 0;
}

static void username_for_uid(uid_t uid, char *buffer, size_t buflen) {
    if (!buffer || buflen == 0)
        return;
    buffer[0] = '\0';

    struct passwd pwd;
    struct passwd *result = NULL;
    char pwbuf[4096];
    if (getpwuid_r(uid, &pwd, pwbuf, sizeof(pwbuf), &result) == 0 && result) {
        strncpy(buffer, result->pw_name, buflen - 1);
        buffer[buflen - 1] = '\0';
        return;
    }
    snprintf(buffer, buflen, "%d", uid);
}

static void load_cmdline(pid_t pid, const char *fallback, char *buffer, size_t buflen) {
    if (!buffer || buflen == 0)
        return;
    buffer[0] = '\0';

    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    FILE *fp = fopen(path, "r");
    if (!fp) {
        if (fallback) {
            strncpy(buffer, fallback, buflen - 1);
            buffer[buflen - 1] = '\0';
        }
        return;
    }

    size_t written = 0;
    int c;
    while ((c = fgetc(fp)) != EOF && written < buflen - 1) {
        if (c == '\0') {
            if (written == 0)
                continue;
            buffer[written++] = ' ';
        } else {
            buffer[written++] = (char)c;
        }
    }
    buffer[written] = '\0';
    fclose(fp);

    if (written == 0 && fallback) {
        strncpy(buffer, fallback, buflen - 1);
        buffer[buflen - 1] = '\0';
    }
}

static long read_mem_total_kb(void) {
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp)
        return 0;
    char label[64];
    long value = 0;
    char unit[32];
    if (fscanf(fp, "%63s %ld %31s", label, &value, unit) != 3) {
        fclose(fp);
        return 0;
    }
    fclose(fp);
    return value;
}

static int read_process_stat(pid_t pid, process_info *info, unsigned long long *total_ticks) {
    if (!info || !total_ticks)
        return -1;

    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    FILE *fp = fopen(path, "r");
    if (!fp)
        return -1;

    char *line = NULL;
    size_t len = 0;
    ssize_t read = getline(&line, &len, fp);
    fclose(fp);
    if (read == -1) {
        free(line);
        return -1;
    }

    char *ptr = line;
    errno = 0;
    long parsed_pid = strtol(ptr, &ptr, 10);
    if (errno != 0) {
        free(line);
        return -1;
    }
    info->pid = (pid_t)parsed_pid;

    while (*ptr == ' ')
        ptr++;
    if (*ptr != '(') {
        free(line);
        return -1;
    }
    ptr++;
    char *comm_end = strrchr(ptr, ')');
    if (!comm_end) {
        free(line);
        return -1;
    }
    size_t comm_len = comm_end - ptr;
    if (comm_len >= sizeof(info->command))
        comm_len = sizeof(info->command) - 1;
    memcpy(info->command, ptr, comm_len);
    info->command[comm_len] = '\0';

    ptr = comm_end + 2; // skip ") "

    unsigned long long utime = 0;
    unsigned long long stime = 0;
    unsigned long long vsize = 0;
    long long rss = 0;
    pid_t ppid = 0;
    char state = '?';

    int field_index = 3;
    char *saveptr = NULL;
    char *token = strtok_r(ptr, " ", &saveptr);
    while (token) {
        switch (field_index) {
            case 3:
                state = token[0];
                break;
            case 4:
                ppid = (pid_t)strtol(token, NULL, 10);
                break;
            case 14:
                utime = strtoull(token, NULL, 10);
                break;
            case 15:
                stime = strtoull(token, NULL, 10);
                break;
            case 23:
                vsize = strtoull(token, NULL, 10);
                break;
            case 24:
                rss = strtoll(token, NULL, 10);
                break;
            default:
                break;
        }
        token = strtok_r(NULL, " ", &saveptr);
        field_index++;
    }

    info->ppid = ppid;
    info->state = state;
    info->vms_kb = (long)(vsize / 1024);
    info->rss_kb = (long)(rss * g_page_size_kb);

    *total_ticks = utime + stime;

    free(line);
    return 0;
}

static void populate_user_info(pid_t pid, process_info *info) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d", pid);
    struct stat st;
    if (stat(path, &st) == -1) {
        info->uid = 0;
        strcpy(info->user, "?");
        return;
    }
    info->uid = st.st_uid;
    username_for_uid(info->uid, info->user, sizeof(info->user));
}

static void populate_thread_count(pid_t pid, process_info *info) {
    if (!info)
        return;
    info->threads = 0;

    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    FILE *fp = fopen(path, "r");
    if (!fp)
        return;

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "Threads:", 8) == 0) {
            int t = 0;
            if (sscanf(line + 8, "%d", &t) == 1 && t >= 0) {
                info->threads = t;
            }
            break;
        }
    }
    fclose(fp);
}

static int double_cmp(double a, double b) {
    if (a < b)
        return -1;
    if (a > b)
        return 1;
    return 0;
}

static const char *g_sort_key = "cpu";
static bool g_sort_reverse = false;

static int process_compare(const void *lhs, const void *rhs) {
    const process_info *a = lhs;
    const process_info *b = rhs;
    int result = 0;
    if (strcasecmp(g_sort_key, "cpu") == 0) {
        result = double_cmp(b->cpu_percent, a->cpu_percent);
    } else if (strcasecmp(g_sort_key, "memory") == 0 || strcasecmp(g_sort_key, "mem") == 0) {
        result = double_cmp(b->mem_percent, a->mem_percent);
    } else if (strcasecmp(g_sort_key, "pid") == 0) {
        if (a->pid < b->pid)
            result = -1;
        else if (a->pid > b->pid)
            result = 1;
    } else if (strcasecmp(g_sort_key, "name") == 0 || strcasecmp(g_sort_key, "command") == 0) {
        result = strcasecmp(a->command, b->command);
    } else {
        result = double_cmp(b->cpu_percent, a->cpu_percent);
    }

    if (g_sort_reverse)
        result = -result;
    return result;
}

int process_list_refresh(process_list *list,
                         process_cache *cache,
                         double elapsed_seconds,
                         const struct cupid_config *config) {
    if (!list || !cache || !config)
        return -1;

    if (elapsed_seconds <= 0.0)
        elapsed_seconds = 0.001;

    ensure_system_constants();

    DIR *proc_dir = opendir("/proc");
    if (!proc_dir)
        return -1;

    process_list_clear(list);

    process_cache new_cache;
    process_cache_init(&new_cache);

    long mem_total_kb = read_mem_total_kb();
    if (mem_total_kb <= 0)
        mem_total_kb = 1;

    struct dirent *entry;
    while ((entry = readdir(proc_dir)) != NULL) {
        if (!isdigit((unsigned char)entry->d_name[0]))
            continue;

        pid_t pid = (pid_t)strtol(entry->d_name, NULL, 10);
        process_info info;
        memset(&info, 0, sizeof(info));

        unsigned long long total_ticks = 0;
        if (read_process_stat(pid, &info, &total_ticks) != 0)
            continue;

        populate_user_info(pid, &info);
        populate_thread_count(pid, &info);
        char comm_copy[sizeof(info.command)];
        strncpy(comm_copy, info.command, sizeof(comm_copy));
        comm_copy[sizeof(comm_copy) - 1] = '\0';
        load_cmdline(pid, comm_copy, info.command, sizeof(info.command));

        bool found = false;
        unsigned long long prev_ticks = cache_lookup(cache, pid, &found);
        if (found && elapsed_seconds > 0.0) {
            double delta_ticks = (double)(total_ticks - prev_ticks);
            if (delta_ticks < 0)
                delta_ticks = 0;
            double cpu = (delta_ticks / (double)g_ticks_per_sec) / elapsed_seconds * 100.0;
            if (g_cpu_count > 1)
                cpu /= (double)g_cpu_count;
            if (cpu < 0.0)
                cpu = 0.0;
            if (cpu > 100.0)
                cpu = 100.0;
            info.cpu_percent = cpu;
        } else {
            info.cpu_percent = 0.0;
        }

        info.mem_percent = ((double)info.rss_kb / (double)mem_total_kb) * 100.0;
        if (info.mem_percent < 0.0)
            info.mem_percent = 0.0;

        if (cache_append(&new_cache, pid, total_ticks) != 0) {
            closedir(proc_dir);
            process_cache_free(&new_cache);
            return -1;
        }

        if (ensure_list_capacity(list, list->count + 1) != 0) {
            closedir(proc_dir);
            process_cache_free(&new_cache);
            return -1;
        }
        list->items[list->count++] = info;
    }

    closedir(proc_dir);

    /* Optional CPU grouping: aggregate children into parents for tree view */
    if (config->cpu_group_mode == CPU_GROUP_AGGREGATE &&
        config->tree_view_default != TREE_VIEW_FLAT && list->count > 0) {
        double *agg = calloc(list->count, sizeof(double));
        if (agg) {
            for (size_t i = 0; i < list->count; ++i)
                agg[i] = list->items[i].cpu_percent;
            for (size_t i = 0; i < list->count; ++i) {
                pid_t ppid = list->items[i].ppid;
                for (size_t j = 0; j < list->count; ++j) {
                    if (list->items[j].pid == ppid) {
                        agg[j] += list->items[i].cpu_percent;
                        break;
                    }
                }
            }
            for (size_t i = 0; i < list->count; ++i) {
                double v = agg[i];
                if (v < 0.0)
                    v = 0.0;
                if (v > 100.0)
                    v = 100.0;
                list->items[i].cpu_percent = v;
            }
            free(agg);
        }
    }

    g_sort_key = config->default_sort;
    g_sort_reverse = config->sort_reverse;
    qsort(list->items, list->count, sizeof(process_info), process_compare);

    if (config->max_processes > 0 && list->count > (size_t)config->max_processes)
        list->count = (size_t)config->max_processes;

    cache_replace(cache, &new_cache);

    return 0;
}

