#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>
#include <time.h>
#include "hop.h"

#define MAX_ENTRIES 256

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

//so struct shld hav the name of dir, time of hop and number of hops, name of directpory and the numer of hops right
typedef struct {
    char path[PATH_MAX]; //path of directory
    int  frequency; //number of hits        
    long last_access; //time of hop
}frecency_entry_t;

char history_path[PATH_MAX];
char hop_shell_home[PATH_MAX];

frecency_entry_t db[MAX_ENTRIES];
int db_count = 0;

void save_history() {
    FILE *fp = fopen(history_path, "w");
    if (fp == NULL) 
    {
        return;               
    }
    for (int i = 0; i < db_count; i++)
    {
        fprintf(fp, "%d %ld %s\n",db[i].frequency, db[i].last_access, db[i].path);
    }
    fclose(fp);
}

void load_history() {
    FILE *fp = fopen(history_path, "r");
    if (fp == NULL) {
        return;
    }
    db_count = 0;
    char line[PATH_MAX + 128];
    while (db_count < MAX_ENTRIES && fgets(line, sizeof(line), fp) != NULL) 
    {
        int freq;
        long last_acc;
        if (sscanf(line, "%d %ld", &freq, &last_acc) == 2) // if valid line, so has the correct len
        {
            char *p = line;

            while (*p && (*p == ' ' || *p == '\t')) p++; //ignotr the starting tabs
            while (*p && (*p >= '0' && *p <= '9')) p++; //ignotre the freq numbers
            while (*p && (*p == ' ' || *p == '\t')) p++; //ignore the beeck ke spaces
            while (*p && ((*p >= '0' && *p <= '9') || *p == '-')) p++; //ignore the lastaccess numbers
            while (*p && (*p == ' ' || *p == '\t')) p++; //ignore the spavces before the actual address

            size_t len_p = strlen(p);
            while (len_p > 0 && (p[len_p - 1] == '\n' || p[len_p - 1] == '\r')) {
                p[len_p - 1] = '\0'; 
                len_p--;
            }

            if (len_p > 0) {
                db[db_count].frequency = freq;
                db[db_count].last_access = last_acc;
                strncpy(db[db_count].path, p, PATH_MAX - 1);
                db[db_count].path[PATH_MAX - 1] = '\0';
                db_count++;
            }
        }
    }
    fclose(fp);
}

//scoring ,ethid. Based on frequency, if no frequency, then last access // THIS IS NOT FRECENCY CHANGE IT
int score(const frecency_entry_t *e) 
{
    int weight;
    long now =(long)time(NULL);
    long t_d = now-e->last_access;

    if (t_d<3600)
    {
        weight = 400; // within last hour
    }
    else if (t_d<86400)
    {
        weight=200; // within last day
    }
    else if (t_d<604800) 
    {
        weight = 100;
    }
    else
    {
        weight = 50; //def for anything older
    }

    return e->frequency*weight; //HENCE thsi is fecency cuz itd frequency*recency
}

void update_frecency(const char *abs_path) {

    //does this entry already exist
    for (int i = 0; i < db_count; i++) 
    {
        if (strcmp(db[i].path, abs_path) == 0) 
        {
            db[i].frequency++; //if found, just update the frequency and last access time
            db[i].last_access = (long)time(NULL);
            save_history(); //save the new array
            return;
        }
    }

    if (db_count < MAX_ENTRIES) //still more space in teh db, then jst add it with freq =1
    {
        strncpy(db[db_count].path, abs_path, PATH_MAX - 1);
        db[db_count].path[PATH_MAX - 1] = '\0';
        db[db_count].frequency = 1;
        db[db_count].last_access = (long)time(NULL);
        db_count++;
    } 
    else 
    {
        //noi space then evict the least accessed entry
        int min_idx = 0;
        int min_sc =score(&db[0]);
        for (int i=1; i<db_count; i++) 
        {
            int s = score(&db[i]);
            if (s<min_sc) 
            {
                min_sc =s;
                min_idx = i;
            }
        }
        strncpy(db[min_idx].path, abs_path, PATH_MAX-1);
        db[min_idx].path[PATH_MAX-1] = '\0';
        db[min_idx].frequency = 1;
        db[min_idx].last_access = (long)time(NULL);
    }
    save_history(); //save to file
}

const char *frecency_lookup(const char *name) 
{

    int order[MAX_ENTRIES];
    for (int i = 0; i < db_count; i++) order[i] = i;

   //sort teh db array to rank the higher rank one on top
    for (int i = 0; i < db_count - 1; i++) {
        for (int j = i + 1; j < db_count; j++) {
            if (score(&db[order[j]]) > score(&db[order[i]]))
            {
                int k= order[i];
                order[i] = order[j];
                order[j] = k;
            }
        }
    }

    for (int i = 0; i < db_count; i++) 
    {
        int idx = order[i];
        if (strstr(db[idx].path, name) != NULL) {
            struct stat st;
            if (stat(db[idx].path, &st) == 0 && S_ISDIR(st.st_mode)) {
                return db[idx].path;
            }
        }
    }
    return NULL;
}

//code to hop to target . THen it make s the OLDPWD into ur CWD 

int do_chdir(const char *target) 
{
    char prev[PATH_MAX]; 
    
    if (getcwd(prev, sizeof(prev)) == NULL) 
    {
        prev[0] = '\0'; //st0re the directory before hoppung
    }

    if (chdir(target) != 0)  //cldnt hop to proper dir
    {
        return 0;
    }

    char landed[PATH_MAX];
    if (getcwd(landed, sizeof(landed)) != NULL) 
    {
        update_frecency(landed);
    }

    if (prev[0] != '\0') 
    {
        setenv("OLDPWD", prev, 1); //save OLD PWD FOR hop ..
    }
    return 1;
}


void hop_init(const char *home_dir) 
{
    strncpy(hop_shell_home, home_dir, PATH_MAX - 1); //path of shells home dir
    hop_shell_home[PATH_MAX - 1] = '\0'; //save that path in the global variable plus null terminator

    char exe_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1); //find path of currentloy running executable
    if (len > 0)  //if readlink worked
    {
        exe_path[len] = '\0';
        
        char *last_slash = strrchr(exe_path, '/');
        if (last_slash != NULL)
        {
            *last_slash = '\0';
            strncpy(history_path, exe_path, PATH_MAX - 15); //cpies executable into history path
            history_path[PATH_MAX - 15] = '\0';
            strcat(history_path, "/hop_hist");  
        } 
        else 
        {
            snprintf(history_path, sizeof(history_path), "%s/hop_hist", home_dir); //print path with hop history
        }
    } else 
    {
        snprintf(history_path, sizeof(history_path), "%s/hop_hist", home_dir); //load old  hist cuz readlink didnt work
    }

    load_history();
}

void hop(const token_list_t *list) {
    //if no arguments, treat it like hop ~
    if (list->count <= 1) 
    {
        do_chdir(hop_shell_home);
        return;
    }

    //process itt in sequential order
    for (size_t i = 1; i < list->count; i++) 
    {
        // make sure to skip non words
        if (list->tokens[i].type != OP_WORD) 
        {
            continue;
        }

        const char *arg = list->tokens[i].text;

        // if ~ then go home
        if (strcmp(arg, "~") == 0) 
        {
            do_chdir(hop_shell_home);
            continue;
        }

        // if . then stay in directory
        if (strcmp(arg, ".") == 0) 
        {
            continue;
        }

        // .. then go to parent dir
        if (strcmp(arg, "..") == 0) 
        {
            do_chdir("..");
            continue;
        }

        // previous cwd print in termibal
        if (strcmp(arg, "-") == 0) 
        {
            char *old = getenv("OLDPWD");
            if (old != NULL) {
                printf("%s\n", old);
                if (!do_chdir(old)) {
                    printf("hop: No such directory\n");
                }
            }
            continue;
        }

        // absolute path try
        {
            struct stat st;
            if (stat(arg, &st) == 0 && S_ISDIR(st.st_mode)) {
                if (!do_chdir(arg)) {
                    printf("hop: No such directory\n");
                }
                continue;
            }
        }

        // frecency lookup
        {
            const char *match = frecency_lookup(arg);
            if (match != NULL) {
                if (!do_chdir(match)) {
                    printf("hop: No such directory\n");
                }
                continue;
            }
        }
        // nothing matched
        printf("hop: No such directory\n");
    }
}
