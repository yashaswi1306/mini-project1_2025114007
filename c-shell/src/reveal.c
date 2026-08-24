#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
#include "reveal.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

char reveal_shell_home[PATH_MAX];

int cmp_names(const void *a, const void *b) 
{
    return strcmp(*(const char **)a, *(const char **)b); // cmp so that u can do -t for lexographic order
}

//duplicate function tha so removed

void reveal_init(const char *home_dir) // saves a coy of teh home ir into a local variavble
{
    strncpy(reveal_shell_home, home_dir, PATH_MAX - 1);
    reveal_shell_home[PATH_MAX - 1] = '\0';
}

int resolve_target(const char *arg, char *out, size_t outsize) 
{
    if (arg == NULL || strcmp(arg, ".") == 0) 
    {
        // if cwd or no args
        return getcwd(out, outsize) != NULL;
    }

    if (strcmp(arg, "~") == 0) 
    {
        // if ~, then my home dir
        snprintf(out, outsize, "%s", reveal_shell_home);
        return 1;
    }
    if (strcmp(arg, "..") == 0) 
    {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) == NULL) return 0; //gets cwd and stores in cwd, returns null if fails
        char tmp[PATH_MAX + 4]; //temp path
        snprintf(tmp, sizeof(tmp), "%s/..", cwd);  // buils currend dir/ path 
        char *rp = realpath(tmp, NULL); //resolves that path
        if (rp == NULL) return 0; //retirns 0 if teh real path func doesnt work
        snprintf(out, outsize, "%s", rp);
        free(rp);

        return 1;
    }
    if (strcmp(arg, "-") == 0) 
    {
        char *old = getenv("OLDPWD"); //get prev wd
        if (old == NULL) return 0; // if not present
        snprintf(out, outsize, "%s", old);
        return 1;
    }
    // absolute path lookup
    char *rp = realpath(arg, NULL);
    if (rp == NULL) return 0;
    // hop logic
    struct stat st;
    if (stat(rp, &st) != 0 || !S_ISDIR(st.st_mode)) {
        free(rp);
        return 0;
    }
    snprintf(out, outsize, "%s", rp);
    free(rp);
    return 1;
}


void reveal_list_dir(const char *dir_path, const char *prefix,int flag_a, int flag_t)
{
    DIR *d = opendir(dir_path); // open dir path
    if (d == NULL) return;


    size_t cap = 64, count = 0; // item names in dir names
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
            continue;
        if (!flag_a && ent->d_name[0] == '.') // if flag not -a and the file isnt a hidden file, ignore
            continue;
        if (count == cap) 
        {
            cap *= 2; // if the count= capacity, realloc
            char **tmp = realloc(names, sizeof(char *) * cap);
            if (tmp == NULL) break;
            names = tmp;
        }
        names[count++] = strdup(ent->d_name);
    }
    closedir(d);

    // sort acc to lexigraphical order
    qsort(names, count, sizeof(char *), cmp_names);

    // print and recures through files
    for (size_t i = 0; i < count; i++) {
        char full[PATH_MAX];
        snprintf(full, sizeof(full), "%s/%s", dir_path, names[i]);

        struct stat st;
        int is_dir = (stat(full, &st) == 0 && S_ISDIR(st.st_mode));

        if (flag_t) 
        {
            // if -t i teh flag
            if (is_dir)
                printf("%s%s/\n", prefix, names[i]);
            else
                printf("%s%s\n", prefix, names[i]);

            if (is_dir) {
                char new_prefix[PATH_MAX];
                snprintf(new_prefix, sizeof(new_prefix), "%s%s/",
                         prefix, names[i]);
                reveal_list_dir(full, new_prefix, flag_a, flag_t);
            }
        } 
        else 
        {
            // if no flag, jst print in ls style
            printf("%s\n", names[i]);
        }

        free(names[i]);
    }
    free(names);
}


void reveal(const token_list_t *list) 
{
    int flag_a = 0, flag_t = 0; // right now, both flags are 0
    const char *target = NULL;

    // parse flags and the path argument
    for (size_t i = 1; i < list->count; i++) 
    {
        if (list->tokens[i].type != OP_WORD) continue; 
        const char *arg = list->tokens[i].text;

        // resolve flags
        if (arg[0] == '-' && arg[1] != '\0') 
        {
            for (int j = 1; arg[j] != '\0'; j++) 
            {
                if (arg[j] == 'a')
                {
                    flag_a = 1; // flag a
                }
                else if (arg[j] == 't') 
                {
                    flag_t = 1; // flag t
                }
                else 
                {
                    printf("reveal: invalid syntax\n"); //wrong flags
                    return;
                }
            }
        } else {
            // only one ath arg allowed
            if (target != NULL) {
                printf("reveal: invalid syntax\n");
                return;
            }
            target = arg;
        }
    }

    // reslve target directory
    char resolved[PATH_MAX];
    if (!resolve_target(target, resolved, sizeof(resolved))) {
        printf("reveal: no such directory\n");
        return;
    }

    struct stat st;
    if (stat(resolved, &st) != 0 || !S_ISDIR(st.st_mode)) 
    {
        printf("reveal: no such directory\n"); //no such dir
        return;
    }

    reveal_list_dir(resolved, "", flag_a, flag_t); // call reveal for that dir
}

