/*
cuPID is a simple process manager for Linux 
written by @frankischilling
*/
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <wordexp.h>
#include <ncurses.h>
#include "cupidconf.h"

// Expand ~ to home directory using wordexp (POSIX)
char* expand_path(const char *path) {
    wordexp_t p;
    if (wordexp(path, &p, 0) == 0) {
        char *expanded = strdup(p.we_wordv[0]);
        wordfree(&p);
        return expanded;
    }
    return NULL;
}

// set up config file in ~/.config/cuPID/config.conf
void setup_config_file() {
    char *config_path = expand_path("~/.config/cuPID/config.conf");
    if (!config_path) {
        printf("Failed to expand config path.\n");
        return;
    }
    
    // Create directory if it doesn't exist
    char *dir_path = strdup(config_path);
    char *last_slash = strrchr(dir_path, '/');
    if (last_slash) {
        *last_slash = '\0';
        // Create parent directories recursively
        for (char *p = dir_path + 1; *p; p++) {
            if (*p == '/') {
                *p = '\0';
                struct stat st = {0};
                if (stat(dir_path, &st) == -1) {
                    mkdir(dir_path, 0755);
                }
                *p = '/';
            }
        }
        // Create the final directory
        struct stat st = {0};
        if (stat(dir_path, &st) == -1) {
            mkdir(dir_path, 0755);
        }
    }
    free(dir_path);
    
    FILE *file = fopen(config_path, "w");
    if (!file) {
        printf("Failed to create config file: %s\n", config_path);
        free(config_path);
        return;
    }
    fclose(file);
    free(config_path);
}

int main(int argc, char *argv[]) {
    setup_config_file();
    char *config_path = expand_path("~/.config/cuPID/config.conf");
    if (!config_path) {
        printf("Failed to expand config path.\n");
        return 1;
    }
    
    cupidconf_t *conf = cupidconf_load(config_path);
    free(config_path);
    if (!conf) {
        printf("Failed to load configuration.\n");
        return 1;
    }
    printf("Configuration loaded successfully.\n");
    cupidconf_free(conf);
    // setup window ncurses 
    WINDOW *win = newwin(LINES, COLS, 0, 0);
    box(win, 0, 0);
    wrefresh(win);
    getch();
    delwin(win);
    endwin();   
    return 0;
}