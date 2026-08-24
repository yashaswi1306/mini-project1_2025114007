#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>
#include "locate.h"

#ifndef PATH_MAX
#define PATH_MAX 5000
#endif

//use same functoion as execute.c
int locate_is_executable(const char *filepath) 
{
    struct stat st;
    if (access(filepath, X_OK) != 0) return 0;
    if (stat(filepath, &st) != 0) return 0;
    if (S_ISDIR(st.st_mode)) return 0;
    return 1;
}

//ai written part 
void locate_single(const char *cmd) 
{
    int matches = 0; //number of exec files matched
    char cwd[PATH_MAX+512];
    if (getcwd(cwd, sizeof(cwd)) != NULL)  //get cwd 
    {
        char cwd_candidate[PATH_MAX]; //dekh liyo ye ek baar 

        snprintf(cwd_candidate, sizeof(cwd_candidate), "%s/%s", cwd, cmd);

        if (locate_is_executable(cwd_candidate))  //if its an eecutable file, increase teh nber of matches
        {
            printf("%s\n", cwd_candidate);
            matches++;
        }
    }

    const char *env_path = getenv("PATH");

    if (env_path != NULL && strlen(env_path) > 0) 
    {
        char *path_copy = strdup(env_path);
        if (path_copy != NULL) {
            char *saveptr = NULL;
            char *dir = strtok_r(path_copy, ":", &saveptr);
            while (dir != NULL) {
                char candidate[PATH_MAX + 512];
                if (strlen(dir) == 0) {
                    snprintf(candidate, sizeof(candidate), "%s/%s", cwd, cmd);
                } else if (dir[0] == '/') {
                    snprintf(candidate, sizeof(candidate), "%s/%s", dir, cmd);
                } else {
                    snprintf(candidate, sizeof(candidate), "%s/%s/%s", cwd, dir, cmd);
                }

                if (locate_is_executable(candidate)) {
                    printf("%s\n", candidate);
                    matches++;
                }
                dir = strtok_r(NULL, ":", &saveptr);
            }
            free(path_copy);
        }
    }

    if (matches == 0) {
        printf("locate: command not found (%s)\n", cmd); // no such file found
    }
}

////

void locate (const token_list_t *list)
{
    int arg_count=0; //arguments given to loacte
      for (size_t i = 1; i < list->count; i++) 
      {
        if (list->tokens[i].type == OP_WORD) 
        {
            arg_count++; //count numer of arguments in list AFTER locate keyword
        }
    }

    if(arg_count==0)
    {
        printf("locate: invalid syntax\n"); //if no args, then invalidb code
        return;
    }

    for (size_t i = 1; i < list->count; i++) 
    {
        if (list->tokens[i].type == OP_WORD) 
        {
            locate_single(list->tokens[i].text); //for EACH ARGUMENT, exevute the locate single command
        }
    }
}

