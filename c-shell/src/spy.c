#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "spy.h"
#include "execute.h"
#include <unistd.h> // to readlink()
#include <dirent.h> // forn  _dir() funcs
#include <ctype.h> //for is_digit
#include <sys/stat.h> //since code uses stat()
#include <sys/types.h> //sys level data types

#define MAX_MEM_FILES 512

typedef struct{
    char path[512];
}mem_file_t; // since files mapped to memory using mmap

static int compare_num_fds(const void *a, const void *b) // for qsort to sort file descriptors
{
    int num_a=atoi(*(const char **)a);
    int num_b=atoi(*(const char **)b);
    return num_a-num_b;
}

static int is_non_negative_int(const char *str) //is pid a digit
{
    if (!str||*str=='\0') 
    {
        return 0; // NULL STRINGS INVALid
    }
    for (size_t i=0; str[i]!='\0'; i++) 
    {
        if (!isdigit((unsigned char)str[i])) 
        {
            return 0; //non digit
        }
    }
    return 1; //valid for pid
}

static const char *get_file_type(const char *path,const char *link_target) // get file type of a path
{
    struct stat st;
    if (lstat(path,&st)==0) //does file path use lstat
    {
        if (S_ISDIR(st.st_mode)) return "DIR"; //dir
        if (S_ISCHR(st.st_mode)) return "CHR"; //char device
        if (S_ISSOCK(st.st_mode)) return "SOCK"; //socket
        if (S_ISREG(st.st_mode)) return "REG"; //regular file        
        if (S_ISBLK(st.st_mode)) return "BLK"; //block device
        if (S_ISFIFO(st.st_mode)) return "FIFO"; //pipe or fifo
        if (S_ISLNK(st.st_mode)) return "LNK"; //symbolic link

    }

    if (link_target && stat(link_target,&st) == 0) // stat the resolves link traget
    {
        if (S_ISDIR(st.st_mode)) return "DIR"; //dir
        if (S_ISCHR(st.st_mode)) return "CHR"; //char dev
        if (S_ISLNK(st.st_mode)) return "LNK"; //sybmolic link
        if (S_ISSOCK(st.st_mode)) return "SOCK"; //socket
        if (S_ISREG(st.st_mode)) return "REG"; //reg file
        if (S_ISBLK(st.st_mode)) return "BLK"; //block device
        if (S_ISFIFO(st.st_mode)) return "FIFO"; //fifo
       
    }

    if (link_target) //dtring matching as fallback (for pseudo files and pipes)
    {
        if (strncmp(link_target,"/dev/pts/",9)==0||strncmp(link_target,"/dev/tty", 8)==0) 
        {
            return "CHR";
        }
        if (strncmp(link_target,"pipe:",5)==0||strncmp(link_target,"anon_inode",10)==0) 
        {
            return "FIFO";
        }
        if (strncmp(link_target,"socket:",7)==0) 
        {
            return "SOCK";
        }
    }

    return "REG"; //regular file by default
}

void spy_cmd(const token_list_t *list) 
{
    pid_t target_pid=getpid(); //curr process id

    if (list->count>2) // at most 1 arg accepted
    {
        printf("spy: invalid syntax\n");
        return;
    }

    if (list->count==2) //if pid given, validate and parse it
    {
        const char *pid_str=list->tokens[1].text;
        if (!is_non_negative_int(pid_str)) 
        {
            printf("spy: invalid syntax\n"); //invalid if not positive number
            return;
        }
        target_pid = (pid_t)atoi(pid_str);
    }

    char proc_path[256]; //does target process exist in /proc
    snprintf(proc_path, sizeof(proc_path),"/proc/%d",(int)target_pid);
    if (access(proc_path,F_OK) != 0) 
    {
        printf("spy: no such process\n");
        return;
    }

    //table header for printing
    printf("%-7s %-5s %-7s %s\n","PID","FD","TYPE","PATH");

    //cwd
    char cwd_link[256], cwd_target[512]; //link stores proc cwd path, target stored dir processes cwd points to 
    snprintf(cwd_link,sizeof(cwd_link),"/proc/%d/cwd",(int)target_pid);

    ssize_t len=readlink(cwd_link,cwd_target,sizeof(cwd_target)-1); //read symbolic link
    
    if (len>0) //readlink succeeded
    {
        cwd_target[len]='\0'; //null terminator
        const char *type=get_file_type(cwd_target, cwd_target);
        printf("%-7d %-5s %-7s %s\n",(int)target_pid,"cwd",type,cwd_target); //print info
    }

    //executable file
    char exe_link[256],exe_target[512];
    snprintf(exe_link, sizeof(exe_link),"/proc/%d/exe",(int)target_pid);
    len=readlink(exe_link, exe_target, sizeof(exe_target) - 1);
    if (len>0) 
    {
        exe_target[len]='\0';
        const char *type = get_file_type(exe_target,exe_target);
        printf("%-7d %-5s %-7s %s\n",(int)target_pid,"txt",type,exe_target);
    }

    //mem mappedfiles
    char maps_path[256];
    snprintf(maps_path, sizeof(maps_path),"/proc/%d/maps",(int)target_pid);
    FILE *mf=fopen(maps_path, "r"); //open maps file
    if (mf) //opens successfully
    {
        mem_file_t seen[MAX_MEM_FILES];
        int seen_count=0; //seen stores paths already encountred

        char line[1024];
        while (fgets(line,sizeof(line),mf)) // read file line by ine
        {
            char *slash=strchr(line,'/'); //find first / in line
            if (slash) 
            {
                size_t path_len=strlen(slash);
                if (path_len>0&&slash[path_len-1]=='\n')  //remove newline cuz fgets add it
                {
                    slash[path_len-1] = '\0';
                }

                int duplicate=0; //we havent found this path before

                for (int i=0; i<seen_count; i++) 
                {
                    if (strcmp(seen[i].path, slash) == 0) 
                    {
                        duplicate=1; //compare with other oaths till u find duplicate 
                        break;
                    }
                }

                if (!duplicate&&seen_count<MAX_MEM_FILES) //not duplicate, and there is place left in max mem files
                {
                    strncpy(seen[seen_count].path,slash,sizeof(seen[seen_count].path)-1);
                    seen[seen_count].path[sizeof(seen[seen_count].path)-1]='\0';
                    seen_count++;

                    const char *type=get_file_type(slash, slash);
                    printf("%-7d %-5s %-7s %s\n", (int)target_pid,"mem",type,slash);
                }
            }
        }
        fclose(mf);
    }

    //numeric file descriptors
    char fd_dir_path[256];
    snprintf(fd_dir_path, sizeof(fd_dir_path), "/proc/%d/fd", (int)target_pid);
    DIR *dir=opendir(fd_dir_path); //open the fddir containing the processes open 
    if(dir) 
    {
        char *fd_names[512]; //fd file name
        int fd_count = 0; //count of names

        struct dirent *entry;
        while ((entry=readdir(dir))!=NULL) //read dir one entry at a time
        {
            if (is_non_negative_int(entry->d_name)) 
            {
                if (fd_count<512) 
                {
                    fd_names[fd_count++]=strdup(entry->d_name); //store nam eif spave in array
                }
            }
        }
        closedir(dir);

        qsort(fd_names,fd_count,sizeof(char *),compare_num_fds); //sort in numerical order

        for (int i=0; i<fd_count; i++) 
        {
            char fd_link[512],fd_target[512];

            snprintf(fd_link, sizeof(fd_link), "/proc/%d/fd/%s", (int)target_pid, fd_names[i]); //construct the fds proc path
            len = readlink(fd_link,fd_target,sizeof(fd_target)-1); //resove symbolic link
            if (len>0) //if resolved successfully
            {
                fd_target[len]='\0';
                const char *type = get_file_type(fd_link, fd_target);
                printf("%-7d %-5s %-7s %s\n",(int)target_pid,fd_names[i],type,fd_target);
            }
            free(fd_names[i]);
        }
    }
    fflush(stdout);
}