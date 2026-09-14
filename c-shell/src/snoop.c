#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "snoop.h"
#include "execute.h"
#include <sys/user.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <sys/ptrace.h>

typedef struct{ //track each syscall
    char name[64]; //string name for syscall
    long sys_num; //sys call num
    double total_time; //execution time (accumulated) in secs
    int calls; //num of times its been called
    int first_seen_order; //order in which first encountered
}sys_stat_t;

#define MAX_SYSCALLS 1024
//initializing teh variables
static sys_stat_t sys_stats[MAX_SYSCALLS];
static int num_syscalls_tracked = 0;
static int first_seen_counter = 0;

//map syscall to stadard name string
static const char *get_syscall_name(long num) 
{
    switch (num) 
    {
        case 0: return "read";
        case 1: return "write";
        case 2: return "open";
        case 3: return "close";
        case 4: return "stat";
        case 5: return "fstat";
        case 6: return "lstat";
        case 7: return "poll";
        case 8: return "lseek";
        case 9: return "mmap";
        case 10: return "mprotect";
        case 11: return "munmap";
        case 12: return "brk";
        case 13: return "rt_sigaction";
        case 14: return "rt_sigprocmask";
        case 15: return "rt_sigreturn";
        case 16: return "ioctl";
        case 17: return "pread64";
        case 18: return "pwrite64";
        case 19: return "readv";
        case 20: return "writev";
        case 21: return "access";
        case 22: return "pipe";
        case 23: return "select";
        case 24: return "sched_yield";
        case 25: return "mremap";
        case 28: return "madvise";
        case 32: return "dup";
        case 33: return "dup2";
        case 35: return "nanosleep";
        case 39: return "getpid";
        case 41: return "socket";
        case 42: return "connect";
        case 43: return "accept";
        case 56: return "clone";
        case 57: return "fork";
        case 58: return "vfork";
        case 59: return "execve";
        case 60: return "exit";
        case 61: return "wait4";
        case 62: return "kill";
        case 63: return "uname";
        case 72: return "fcntl";
        case 78: return "getdents";
        case 79: return "getcwd";
        case 80: return "chdir";
        case 87: return "unlink";
        case 89: return "readlink";
        case 102: return "getuid";
        case 104: return "getgid";
        case 107: return "geteuid";
        case 108: return "getegid";
        case 110: return "getppid";
        case 112: return "setsid";
        case 137: return "statfs";
        case 157: return "prctl";
        case 158: return "arch_prctl";
        case 202: return "futex";
        case 217: return "getdents64";
        case 218: return "set_tid_address";
        case 228: return "clock_gettime";
        case 230: return "clock_nanosleep";
        case 231: return "exit_group";
        case 257: return "openat";
        case 262: return "newfstatat";
        case 273: return "set_robust_list";
        case 293: return "pipe2";
        case 302: return "prlimit64";
        case 318: return "getrandom";
        case 332: return "statx";
        case 334: return "rseq";
        case 435: return "clone3";
        default: return NULL;
    }
}

static int is_non_negative_int(const char *str)  //same as spy.c
{
    if (!str||*str == '\0') 
    {
        return 0;
    }
    for (size_t i=0;str[i]!='\0';i++) 
    {
        if (!isdigit((unsigned char)str[i])) 
        {
            return 0;
        }
    }
    return 1;
}

static int compare_sys_stats(const void *a,const void *b) //for qsort based on order of when syscall first called
{
    const sys_stat_t *sa=(const sys_stat_t *)a;
    const sys_stat_t *sb=(const sys_stat_t *)b;
    if (sb->calls!=sa->calls) 
    {
        return sb->calls-sa->calls;
    }
    return sa->first_seen_order-sb->first_seen_order;
}

static void add_syscall_time(long sys_num, double duration) 
{
    for (int i=0; i<num_syscalls_tracked; i++) //loops through already encountered syscalls
    {
        if (sys_stats[i].sys_num==sys_num) 
        {
            sys_stats[i].calls++;
            sys_stats[i].total_time+=duration; //increast the count and add duration
            return;
        }
    }

    if (num_syscalls_tracked<MAX_SYSCALLS) 
    {
        sys_stat_t *st = &sys_stats[num_syscalls_tracked++]; //ponter to next empty position of syscalls (use it and then pint yo next psotition thats why ++)
        st->sys_num=sys_num;
        const char *name=get_syscall_name(sys_num); //get syscall name
        if(name) 
        {
            strncpy(st->name,name,sizeof(st->name)-1);
        } 
        else 
        {
            snprintf(st->name,sizeof(st->name),"syscall_%ld",sys_num); //if u cant find name return syscall_sysnum
        }
        st->name[sizeof(st->name)-1]='\0'; //null termination
        st->calls=1; //is first occurance
        st->total_time=duration;
        st->first_seen_order=first_seen_counter++;
    }
}

void snoop_cmd(const token_list_t *list) 
{
    if (!list||list->count < 2) //syntax invalid if fewer than 2 tokens
    {
        printf("snoop: invalid syntax\n");
        return;
    }

    pid_t target_pid=-1; //haven't selected a process yet
    int is_attach=0; // 0 is for new command, 1 for attaching to new process

    num_syscalls_tracked=0;
    first_seen_counter=0;

    if (strcmp(list->tokens[1].text,"-p")==0) //if found -p flag
    {
        if (list->count!=3||!is_non_negative_int(list->tokens[2].text)) 
        {
            printf("snoop: invalid syntax\n");
            return;
        }
        target_pid = (pid_t)atoi(list->tokens[2].text); //str to int
        is_attach = 1; //attach to existing process

        char proc_path[256];
        snprintf(proc_path, sizeof(proc_path), "/proc/%d", (int)target_pid); //proc path will be proc/<pid>

        //process doesnt exist
        if (access(proc_path, F_OK)!=0) //0 if pid wrong or terminated
        {
            printf("snoop: no such process\n");
            return;
        }

        //attach to an existing process
        if (ptrace(PTRACE_ATTACH, target_pid, NULL, NULL)<0) 
        {
            printf("snoop: no such process\n");
            return;
        }

        int status; //wait until target process stops cuz of ptrace_attach
        waitpid(target_pid, &status, 0); //mandatory for syncing when using ptrace
    } 
    else 
    {
        token_list_t cmd_tokens={.tokens = &list->tokens[1],.count = list->count - 1}; // tokens after snoop
        const char *raw_cmd=cmd_tokens.tokens[0].text; //get command name
        char *exec_path=NULL;

        if (strchr(raw_cmd,'/')!=NULL)  //command already has a /, check if executable
        {
            if(execute_is_executable(raw_cmd)) 
            {
                exec_path = strdup(raw_cmd); //strdup allocates mem and copiues string
            }
        } 
        else //search for / inside path:
        // if the command does not contain a slash (e.g., just sleep or ls), it needs to look it up in the system's PATH environment variable.
        {
            const char *env_path=getenv("PATH");
            if(env_path) 
            {
                char *path_copy = strdup(env_path); //make writable copy of path cuz strtok_r modifies string while splitting
                char *saveptr = NULL;
                char *dir = strtok_r(path_copy, ":", &saveptr); //split path along :
                while(dir) 
                {
                    char candidate[512];
                    snprintf(candidate, sizeof(candidate), "%s/%s", dir, raw_cmd); //dir/cmd
                    if (execute_is_executable(candidate)) 
                    {
                        exec_path=strdup(candidate); //save dynamically allocated copy of path
                        break;
                    }
                    dir=strtok_r(NULL,":",&saveptr); //move to next dir in path
                }
                free(path_copy);
            }
        }

        if (!exec_path) //if exec path stll Null, cldnt find executable corresponding to comand
        {
            printf("snoop: command not found\n");
            return;
        }

        char *argv[256];//copy commnd tokens into argv
        int argc=0;
        for (size_t i=0; i<cmd_tokens.count && argc<255; i++) 
        {
            argv[argc++]=cmd_tokens.tokens[i].text;
        }
        argv[argc]=NULL; //argv has t end with null

        target_pid = fork(); //fork to create new process

        if (target_pid==0) //child process 
        {
            ptrace(PTRACE_TRACEME,0,NULL,NULL); //parents can trace using ptrace
            execv(exec_path, argv);
            perror("snoop"); //if exec returns, then failed
            exit(1);
        }
        free(exec_path); //parent no longer needs exec path

        if (target_pid<0) 
        {
            perror("snoop: fork failed");
            return;
        }
        int status; //wait for child to stop
        waitpid(target_pid, &status, 0);
    }

    //syscall tracing (syscall related stops need to be diff from norml ones)

    ptrace(PTRACE_SETOPTIONS, target_pid, 0, PTRACE_O_TRACESYSGOOD);

    int status;
    int in_syscall=0; // 0 if nxt syscall entry, 1 if next syscall exit
    struct timespec start_ts; //time of curr syscall
    long current_sys_num=-1; //-1 for no syscalls recorded yet

    while(1) 
    {
        if (ptrace(PTRACE_SYSCALL, target_pid, 0, 0)<0) //ptrace failed 
        {
            break;
        }

        waitpid(target_pid, &status, 0); //wait for target process to stop

        if (WIFEXITED(status)||WIFSIGNALED(status)) 
        {
            break; //nothing left to trace (exited or killed by signal)
        }

        if (WIFSTOPPED(status)) 
        {
            int sig=WSTOPSIG(status); //get signal that cause process to stop
            if (sig==(SIGTRAP | 0x80)||sig==SIGTRAP) //means proc stopped sue to trace trap
            {
                struct user_regs_struct regs; //struct with cpu register values

                if (ptrace(PTRACE_GETREGS, target_pid, 0, &regs)==0)  //give register values of target process
                {
                    if (in_syscall==0) 
                    {
                        current_sys_num=regs.orig_rax; //grab syscall number
                        clock_gettime(CLOCK_MONOTONIC, &start_ts); //start time record
                        in_syscall=1;
                    } 
                    else 
                    {
                        struct timespec end_ts;
                        clock_gettime(CLOCK_MONOTONIC,&end_ts); //rec end time
                        double duration=(end_ts.tv_sec-start_ts.tv_sec)+(end_ts.tv_nsec-start_ts.tv_nsec)/1e9; //elapsed time (ns to s)
                        if (current_sys_num>=0) 
                        {
                            add_syscall_time(current_sys_num, duration); //log into stats array 
                        }
                        in_syscall=0;
                    }
                }
            }
        }
    }

    if(is_attach) 
    {
        ptrace(PTRACE_DETACH, target_pid, NULL, NULL); //stop tracing the process and detach from it (safely)
    }

    qsort(sys_stats, num_syscalls_tracked, sizeof(sys_stat_t), compare_sys_stats); //sort sys stats array

    printf("%-20s %-7s %s\n", "syscall", "calls", "time");

    for (int i=0; i<num_syscalls_tracked; i++) 
    {
        printf("%-20s %-7d %.3fs\n",sys_stats[i].name,sys_stats[i].calls,sys_stats[i].total_time); //final res
    }
    fflush(stdout);
}