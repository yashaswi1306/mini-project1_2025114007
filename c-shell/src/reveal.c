#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
#include "reveal.h"

// fallback path max length
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

// global variable to track shell's home directory
char reveal_shell_home[PATH_MAX];

// comparison function for qsort to sort names alphabetically
int cmp_names(const void *a, const void *b) 
{
    return strcmp(*(const char **)a, *(const char **)b); // compare for lexicographical order
}

// save a copy of the home dir into a local variable
void reveal_init(const char *home_dir) 
{
    strncpy(reveal_shell_home, home_dir, PATH_MAX - 1);
    reveal_shell_home[PATH_MAX - 1] = '\0'; //so ~ knows where to point
}

// resolve target path shortcuts like ., ~, .., or -
int resolve_target(const char *arg, char *out, size_t outsize) 
{
    if (arg == NULL || strcmp(arg, ".") == 0) 
    {
        // if cwd or no args specified
        return getcwd(out, outsize) != NULL; //get cwd
    }

    if (strcmp(arg, "~") == 0) 
    {
        // if ~, use shell home dir
        snprintf(out, outsize, "%s", reveal_shell_home);
        return 1;
    }
    if (strcmp(arg, "..") == 0) 
    {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) == NULL) return 0; // get current dir
        char tmp[PATH_MAX + 4]; // temp path buffer
        snprintf(tmp, sizeof(tmp), "%s/..", cwd);  // build parent path (so it makes cwd=a/b into a/b/..)
        char *rp = realpath(tmp, NULL); // resolve actual path gets a/b
        if (rp == NULL) return 0; // fail if realpath errors out
        snprintf(out, outsize, "%s", rp);
        free(rp);

        return 1;
    }
    if (strcmp(arg, "-") == 0) 
    {
        char *old = getenv("OLDPWD"); // get previous working dir
        if (old == NULL) return 0; // if OLDPWD not set, fail
        snprintf(out, outsize, "%s", old);
        return 1;
    }

    // gets absolite path from root dir
    char *rp = realpath(arg, NULL);
    if (rp == NULL) return 0;

    // check if it's actually a valid directory
    struct stat st;
    if (stat(rp, &st) != 0 || !S_ISDIR(st.st_mode)) { //stat fetches metadata (makes sure we can acccess it)
        free(rp);
        return 0;
    }
    snprintf(out, outsize, "%s", rp);
    free(rp);
    return 1;
}

// read directory and list contents (with support for -a and -t recursive tree flags)
void reveal_list_dir(const char *dir_path, const char *prefix, int flag_a, int flag_t)
{
    DIR *d = opendir(dir_path); // open target directory stream
    if (d == NULL) return;

    size_t cap = 64, count = 0; // track dynamic array capacity and item count for storing all fileneames
    char **names = malloc(sizeof(char *) * cap);
    if (names == NULL) 
    { 
        closedir(d); 
        return; 
    }

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) 
    {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue; // skip current and parent dir markers
        if (!flag_a && ent->d_name[0] == '.') 
            continue; // if -a flag is off, ignore hidden files
        
        if (count == cap) 
        {
            cap *= 2; // double capacity if array is full
            char **tmp = realloc(names, sizeof(char *) * cap);
            if (tmp == NULL) break;
            names = tmp;
        }
        names[count++] = strdup(ent->d_name);
    }
    closedir(d);

    // sort names alphabetically
    qsort(names, count, sizeof(char *), cmp_names);

    // print files and recursively traverse directories if flag_t is set
    for (size_t i = 0; i < count; i++) {
        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", dir_path, names[i]); //builds full file path

        struct stat st;
        int is_dir = (stat(full, &st) == 0 && S_ISDIR(st.st_mode)); //checks if its a dir

        if (flag_t) 
        {
            // if -t flag is enabled (tree style print)
            if (is_dir)
                printf("%s%s/\n", prefix, names[i]);
            else
                printf("%s%s\n", prefix, names[i]);

            if (is_dir) {
                char new_prefix[PATH_MAX];
                snprintf(new_prefix, sizeof(new_prefix), "%s%s/", prefix, names[i]);
                reveal_list_dir(full, new_prefix, flag_a, flag_t); // recurse into subdirectory
            }
        } 
        else 
        {
            // standard ls-style print
            printf("%s\n", names[i]);
        }

        free(names[i]);
    }
    free(names);
}

// main entrypoint for the reveal command
void reveal(const token_list_t *list) 
{
    int flag_a = 0, flag_t = 0; // start with both flags turned off
    const char *target = NULL;

    // parse command flags and path argument from tokens
    for (size_t i = 1; i < list->count; i++) 
    {
        if (list->tokens[i].type != OP_WORD) continue; 
        const char *arg = list->tokens[i].text;

        // check if token is a flag
        if (arg[0] == '-' && arg[1] != '\0') 
        {
            for (int j = 1; arg[j] != '\0'; j++) 
            {
                if (arg[j] == 'a')
                {
                    flag_a = 1; // enable hidden files flag
                }
                else if (arg[j] == 't') 
                {
                    flag_t = 1; // enable recursive tree view flag
                }
                else 
                {
                    printf("reveal: invalid syntax\n"); // bad flag error
                    return;
                }
            }
        } else {
            // only allow one path argument (if not flag then path)
            if (target != NULL) {
                printf("reveal: invalid syntax\n");
                return;
            }
            target = arg;
        }
    }

    // resolve target directory path
    char resolved[PATH_MAX];
    if (!resolve_target(target, resolved, sizeof(resolved))) {
        printf("reveal: no such directory\n"); //translates .. , ~ etc. to clean absolute path
        return;
    }

    // verify path exists and is a directory
    struct stat st;
    if (stat(resolved, &st) != 0 || !S_ISDIR(st.st_mode)) 
    {
        printf("reveal: no such directory\n"); 
        return;
    }

    reveal_list_dir(resolved, "", flag_a, flag_t); // trigger directory listing
}