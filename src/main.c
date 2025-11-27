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
#include <ncurses.h>
#include "cupidconf.h"

// set up config file in ~/.config/cuPID/config.conf
void setup_config_file() {
    const char *home = getenv("HOME");
    if (!home) {
        printf("Failed to get home directory.\n");
        return;
    }
    
    // Build the config directory path
    size_t dir_len = strlen(home) + strlen("/.config/cuPID") + 1;
    char *dir_path = malloc(dir_len);
    if (!dir_path) {
        printf("Failed to allocate memory.\n");
        return;
    }
    snprintf(dir_path, dir_len, "%s/.config/cuPID", home);
    
    // Create directory if it doesn't exist
    struct stat st = {0};
    if (stat(dir_path, &st) == -1) {
        // Create parent directories recursively
        for (char *p = dir_path + 1; *p; p++) {
            if (*p == '/') {
                *p = '\0';
                if (stat(dir_path, &st) == -1) {
                    mkdir(dir_path, 0755);
                }
                *p = '/';
            }
        }
        // Create the final directory
        if (stat(dir_path, &st) == -1) {
            mkdir(dir_path, 0755);
        }
    }
    
    // Build the config file path
    size_t file_len = strlen(dir_path) + strlen("/config.conf") + 1;
    char *config_path = malloc(file_len);
    if (!config_path) {
        free(dir_path);
        return;
    }
    snprintf(config_path, file_len, "%s/config.conf", dir_path);
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
    
    // cupidconf 0.4+ natively supports ~ expansion in filenames
    cupidconf_t *conf = cupidconf_load("~/.config/cuPID/config.conf");
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