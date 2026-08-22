#ifndef HOP_H
#define HOP_H

#define PATH_SIZE 5000
#include "part_a.h"
#include <sys/stat.h>

typedef struct hop_command{
    char path[PATH_SIZE];
    int hits; //for frecency
    struct hop_command *next;
}hop_command;

void load_cache();
void save_cache();
int hop(int argc, char**argv, const char*home_dir);

#endif
