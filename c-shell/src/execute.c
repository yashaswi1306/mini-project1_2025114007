#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <limits.h>
#include "execute.h"
#include "hop.h"
#include "reveal.h"
#include "peek.h"
#include "locate.h"
#include "jobs.h"
#include "activities.h"
#include "resume.h"
#include "ping.h"
#include "spy.h"
#include "snoop.h"
#include <signal.h>


#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef struct{
    token_t *tokens;
    size_t count;
}stage_t; //so u can break the piped command into stages

typedef struct{
    char *path;
    int is_append;
}output_redir_t; //for utput redorection

int execute_is_executable(const char *filepath) {
    struct stat st;
    if (access(filepath, X_OK) != 0) return 0; //does path have exec permissin
    if (stat(filepath, &st) != 0) return 0; //get info about te path
    if (S_ISDIR(st.st_mode)) return 0; //dir not executables
    return 1; //if executable hai, return 1
}


static int execute_cmd_internal(const token_list_t *list, int is_bg, int job_id, const char *bg_cmd_name, const char *full_cmd_str) 
{
    if (list == NULL || list->count == 0) 
    {
        return 1;// if no token list nothing to execute
    }

    char *argv[256]; // number of arguments to be passed to exec
    int argc=0;

    char *input_files[256]; //input files for redirection
    int num_inputs=0;

    output_redir_t output_files[256];  //output files for redirection
    int num_outputs=0;

    size_t group_count = 0;

    for (size_t i=0; i<list->count; i++) 
    {
        if (list->tokens[i].type==OP_SEMI||list->tokens[i].type==OP_AMP) 
        {
            break; // stop reading afetr semi colon or &, so echo hi; echo hello only prints hi
        }
        group_count++;
    }

    for (size_t i=0; i<group_count; i++) //only read till group count
    {
        if (list->tokens[i].type == OP_LT) //so if < somewhere(input redir)
        {
            if (i+1<group_count&&list->tokens[i+1].type==OP_WORD) 
            { 
                // < must be followed by a filename
                input_files[num_inputs++]=list->tokens[i+1].text;
                i++; // since filename already processed, skip it
            } 
            else 
            {
                printf("cshell: invalid syntax\n"); // no word after <, so no filename to get input from
                return 0;
            }
        } 
        else if (list->tokens[i].type==OP_GT) 
        {
            if (i+1<group_count&&list->tokens[i+1].type==OP_WORD) 
            {
                output_files[num_outputs].path = list->tokens[i+1].text;
                output_files[num_outputs].is_append = 0; //append to output files
                num_outputs++;
                i++; // already processed filename
            } else 
            {
                printf("cshell: invalid syntax\n"); // no word after > so issue hai
                return 0;
            }
        } 
        else if (list->tokens[i].type==OP_GTGT) 
        {
            if (i+1<group_count&&list->tokens[i+1].type==OP_WORD) 
            {
                output_files[num_outputs].path = list->tokens[i + 1].text; 
                output_files[num_outputs].is_append = 1;
                num_outputs++;
                i++; //same logic as gt for redir
            } 
            else 
            {
                printf("cshell: invalid syntax\n");
                return 0;
            }
        } 
        else if (list->tokens[i].type==OP_WORD&&argc<255) 
        {
            argv[argc++]=list->tokens[i].text; // so argv has stuff like cat, echo etc.
        }
    }

    argv[argc]=NULL; // so the argv MUST end with a NULL

    if (argc==0) 
    {
        return 1;
    }

    // check if input file exist and can be opened with O_RDONLY
    for (int i=0; i<num_inputs; i++) 
    {
        int fd = open(input_files[i], O_RDONLY); // check all input files can be opened
        if (fd < 0) 
        {
            printf("cshell: no such file or directory\n");
            return 0;
        }
        close(fd);
    }

    // check all output files can be written in
    int out_fds[256];
    for (int i=0; i<num_outputs; i++) 
    {
        int mid;
        if (output_files[i].is_append) 
        {
            mid=O_APPEND; //>>
        } 
        else 
        {
            mid=O_TRUNC; //>
        }
        int flags=O_WRONLY|O_CREAT|mid; //(open for writing/create/appen or truncate)

        out_fds[i]=open(output_files[i].path, flags, 0644); // owner: r/w, group: r, others; read
        if (out_fds[i]<0) // if cant create a file to write
        {
            printf("cshell: unable to create file for writing\n"); 
            for (int j = 0; j < i; j++) 
            {
                close(out_fds[j]); // close all open files
            }
            return 0;
        }
    }

    const char *raw_cmd = argv[0]; // read command from argv

    int skip_cwd = 0; //skip the cwd since %
    const char *cmd_name = raw_cmd;

    if (raw_cmd[0] == '%') // means DONT search curr dir first
    {
        skip_cwd = 1; // skip cwd
        cmd_name = raw_cmd + 1; 
        argv[0] = (char *)cmd_name; //argv must contain command with %
    }

    //fully executable path

    char *exec_path = NULL;

    // if command has /, treat as a literal path

    if (!skip_cwd && strchr(cmd_name, '/') != NULL) 
    {
        if (execute_is_executable(cmd_name)) 
        {
            exec_path = strdup(cmd_name); // save a dynamically allocated copy of te path
        }
    } 
    else 
    {
        // check cwd ONLY if code dsnt have %
        if (!skip_cwd) 
        {
            char cwd[PATH_MAX]; // buffer store cwd
            if (getcwd(cwd, sizeof(cwd)) != NULL) 
            {
                char cwd_candidate[PATH_MAX + 512];
                snprintf(cwd_candidate, sizeof(cwd_candidate), "%s/%s", cwd, cmd_name); //contruct cwd/command
                if (execute_is_executable(cwd_candidate))  //checl file executable or not
                {
                    exec_path = strdup(cwd_candidate);//found itttt
                }
            }
        }

        // if command not found in cwd, look through teh path
        if (exec_path==NULL) 
        {
            const char *env_path=getenv("PATH"); //get path

            if (env_path!=NULL && strlen(env_path) > 0) 
            {
                char *path_copy = strdup(env_path); //make copy of path
                if (path_copy!=NULL) 
                {
                    char *saveptr = NULL; //save ptr to save where the path is in string
                    char *dir = strtok_r(path_copy,":",&saveptr);
                    while (dir != NULL) //continue until there are no more path directores
                    {
                        char candidate[PATH_MAX+512]; //buffer for string
                        if (strlen(dir) == 0) 
                        {
                            char cwd[PATH_MAX]; //cwd store
                            if (getcwd(cwd, sizeof(cwd))!=NULL) 
                            {
                                snprintf(candidate, sizeof(candidate), "%s/%s", cwd, cmd_name); //cwd found
                            } 
                            else 
                            {
                                snprintf(candidate, sizeof(candidate), "%s", cmd_name);
                            }
                        } 
                        else 
                        {
                            snprintf(candidate, sizeof(candidate), "%s/%s", dir, cmd_name); //normal path dir
                        }

                        if (execute_is_executable(candidate))  //check if executable or not
                        {
                            exec_path = strdup(candidate);
                            break;
                        }
                        dir = strtok_r(NULL, ":", &saveptr); //move to next path dir
                    }

                    free(path_copy);
                }
            }
        }
    }

    // fi exec path still NULL, executable cant be found

    if (exec_path==NULL) 
    {
        printf("cshell: command not found (%s)\n", cmd_name);
        for (int i=0; i<num_outputs; i++) 
        {
            close(out_fds[i]);
        }
        return 0;
    }
    int sync_pfd[2];
    if(is_bg)  //added for bg tasks
    {
        if (pipe(sync_pfd)<0) 
        {
            perror("cshell: pipe failed");
            for (int i=0; i<num_outputs; i++) 
            {
                close(out_fds[i]);
            }
            free(exec_path);
            return 0;
        }
    }

    pid_t pid=fork(); // create a child process
    if (pid==0)  //childddd
    {
        setpgid(0, 0); //new process group for chuld (necessary for job control)

        if(is_bg) 
        {
            close(sync_pfd[1]); //chil waits till parent writes one byte

            char ch;
            ssize_t r=read(sync_pfd[0], &ch, 1);
            (void)r;
            close(sync_pfd[0]); //done with synchronization pipeline
        }

        //resets the signals: ctrl z shld NOT SUSPEND SHELL, bt it shld suspend child
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);

        // if child process is input redirection
        if (num_inputs == 1) 
        {
            int in_fd = open(input_files[0], O_RDONLY); //open file read only
            if (in_fd >= 0) 
            {
                dup2(in_fd, STDIN_FILENO); //read from file, NOT terminal
                close(in_fd);
            }
        } 
        else if (num_inputs > 1) //multiple input rdir (echo hi > a.txt > b.txt)
        {
            int pfd[2]; // if more than one file, combine contents via pipe

            if (pipe(pfd) == 0)  
            {
                pid_t feeder = fork(); // another child process
                if (feeder == 0) 
                {
                    close(pfd[0]);
                    for (int i=0; i<num_inputs; i++)  // read only input file
                    {
                        int f = open(input_files[i], O_RDONLY); // open on read only
                        if (f >= 0) // if file opne
                        {
                            char buf[4096];
                            ssize_t bytes; // read chunks from file
                            while ((bytes = read(f, buf, sizeof(buf))) > 0) 
                            {
                                ssize_t written = 0; //keep track of chunls being written
                                while (written < bytes) 
                                {
                                    ssize_t w = write(pfd[1], buf + written, bytes - written);
                                    if (w <= 0) 
                                    {
                                        break; // if wrote failed, stop thsi chunk
                                    }
                                    written += w;
                                }
                            }
                            close(f);
                        }
                    }
                    close(pfd[1]);
                    exit(0);
                }
                close(pfd[1]);
                dup2(pfd[0], STDIN_FILENO);

                close(pfd[0]);
                waitpid(feeder, NULL, 0);
            }
        }

        // output redir, same logic
        if (num_outputs == 1) {
            dup2(out_fds[0], STDOUT_FILENO);
            close(out_fds[0]);
        } 
        else if (num_outputs > 1) 
        {
            int out_pfd[2];
            if (pipe(out_pfd) == 0)
             {
                pid_t writer = fork();
                if (writer == 0) 
                {
                    close(out_pfd[1]);
                    char buf[4096];
                    ssize_t bytes;
                    while ((bytes = read(out_pfd[0], buf, sizeof(buf))) > 0) 
                    {
                        for (int i = 0; i < num_outputs; i++) 
                        {
                            ssize_t written = 0;
                            while (written < bytes) {
                                ssize_t w = write(out_fds[i], buf+written, bytes-written);
                                if (w <= 0) break;
                                written += w;
                            }
                        }
                    }
                    close(out_pfd[0]);
                    for (int i = 0; i < num_outputs; i++) 
                    {
                        close(out_fds[i]);
                    }
                    exit(0);
                }
                close(out_pfd[0]);
                dup2(out_pfd[1], STDOUT_FILENO);
                close(out_pfd[1]);
                for (int i = 0; i < num_outputs; i++) {
                    close(out_fds[i]);
                }
            }
        }

        execv(exec_path, argv); //child is requested program (ls -l, so exec path os /usr/bin/ls)
        perror("cshell"); // if exec succeds, then this line isnt run
        exit(1);
    } 
    else if (pid > 0) //parent
    {
        setpgid(pid, pid); //put in diff process group

        // child inherited output file descriptors durin fork
        for (int i = 0; i < num_outputs; i++) 
        {
            close(out_fds[i]);
        }

        if (is_bg) 
        {
            close(sync_pfd[0]); //closes read
            printf("[%d] %d\n", job_id, (int)pid); //print job info
            fflush(stdout);
            ssize_t w = write(sync_pfd[1], "1", 1); //signal child to continue
            (void)w;
            close(sync_pfd[1]);

            jobs_add(job_id, pid, bg_cmd_name, full_cmd_str); //add to job list(record bg job to shell job table)
        } 
        else //not bg
        {
            if (isatty(STDIN_FILENO)) 
            {
                tcsetpgrp(STDIN_FILENO, pid); //stdin is termial, so guven terminal to child proc grp
            }

            int status;
            waitpid(pid, &status, WUNTRACED); // wait for command to finish or stop

            if (isatty(STDIN_FILENO)) 
            {
                tcsetpgrp(STDIN_FILENO, getpgrp()); //gve terminal control to shell
            }

            if (WIFSTOPPED(status)) //detect stopped process
            {
                int s_id = jobs_alloc_id(); //create job id for stopped jobs
                const char *cnames[1] = { cmd_name }; 
                pid_t pids[1] = { pid };
                const char *disp_cmd = (full_cmd_str && full_cmd_str[0] != '\0') ? full_cmd_str : cmd_name;
                jobs_add_stopped(s_id, pid, pids, cnames, 1, disp_cmd); //save stopped process in job table
                printf("[%d] + Stopped  %s\n", s_id, disp_cmd); //print Stopped job
                fflush(stdout);
                free(exec_path);
                return 0;
            }
        }
    } 
    else 
    {
        perror("cshell: fork failed"); // if fork fails
        for (int i = 0; i < num_outputs; i++) 
        {
            close(out_fds[i]);
        }
        if (is_bg) 
        {
            close(sync_pfd[0]);
            close(sync_pfd[1]);
        }
        free(exec_path);
        return 0;
    }

    free(exec_path);
    return 1;
}

static void execute_stage_child(const token_list_t *list) //executes one stage
{
    if (list == NULL || list->count == 0) 
    {
        exit(0);
    }

    if (list->tokens[0].type == OP_WORD) 
    {
        const char *cmd_name = list->tokens[0].text; //frst token : cmmand name
        if (strcmp(cmd_name, "hop") == 0) { hop(list); exit(0); }
        else if (strcmp(cmd_name, "reveal") == 0) { reveal(list); exit(0); }
        else if (strcmp(cmd_name, "peek") == 0) { peek(list); exit(0); }
        else if (strcmp(cmd_name, "locate") == 0) { locate(list); exit(0); }
        else if (strcmp(cmd_name, "activities") == 0) { activities(); exit(0); }
        else if (strcmp(cmd_name, "resume") == 0) { resume_cmd(list); exit(0); }
        else if (strcmp(cmd_name, "ping") == 0) { ping_cmd(list); exit(0); }
        else if (strcmp(cmd_name, "spy") == 0) { spy_cmd(list); exit(0); }
        else if (strcmp(cmd_name, "snoop") == 0) { snoop_cmd(list); exit(0); }
    }
    //so do cmd(ist); exit (0)

    char *argv[256];
    int argc = 0;
    const char *input_files[256];
    int num_inputs = 0;
    typedef struct {
        const char *path;
        int is_append;
    } redir_output_t; //ouput files saved withi is_append flag for append or truncate
    redir_output_t output_files[256];
    int num_outputs = 0;

    for (size_t i = 0; i < list->count; i++) 
    {
        if (list->tokens[i].type == OP_LT) 
        {
            if (i + 1 < list->count && list->tokens[i + 1].type == OP_WORD) 
            {
                input_files[num_inputs++] = list->tokens[i + 1].text;
                i++;
            } 
            else 
            {
                printf("cshell: invalid syntax\n");
                exit(1);
            }
        } 
        else if (list->tokens[i].type == OP_GT) 
        {
            if (i + 1 < list->count && list->tokens[i + 1].type == OP_WORD) 
            {
                output_files[num_outputs].path = list->tokens[i + 1].text;
                output_files[num_outputs].is_append = 0;
                num_outputs++;
                i++;
            } 
            else 
            {
                printf("cshell: invalid syntax\n");
                exit(1);
            }
        } 
        else if (list->tokens[i].type == OP_GTGT) 
        {
            if (i + 1 < list->count && list->tokens[i + 1].type == OP_WORD) 
            {
                output_files[num_outputs].path = list->tokens[i + 1].text;
                output_files[num_outputs].is_append = 1;
                num_outputs++;
                i++;
            } 
            else 
            {
                printf("cshell: invalid syntax\n");
                exit(1);
            }
        } 
        else if (list->tokens[i].type == OP_WORD && argc < 255) 
        {
            argv[argc++] = list->tokens[i].text;
        }
    }
    argv[argc] = NULL;

    if (argc == 0) 
    {
        exit(0);
    }

    for (int i = 0; i < num_inputs; i++) 
    {
        int fd = open(input_files[i], O_RDONLY);
        if (fd < 0) 
        {
            printf("cshell: no such file or directory\n");
            exit(1);
        }
        close(fd);
    }

    int out_fds[256];
    for (int i = 0; i < num_outputs; i++) 
    {
        int mid = output_files[i].is_append ? O_APPEND : O_TRUNC;
        int flags = O_WRONLY | O_CREAT | mid;
        out_fds[i] = open(output_files[i].path, flags, 0644);
        if (out_fds[i] < 0) 
        {
            printf("cshell: unable to create file for writing\n");
            exit(1);
        }
    }

    const char *raw_cmd = argv[0];
    int skip_cwd = 0;
    const char *cmd_name = raw_cmd;
    if (raw_cmd[0] == '%') 
    {
        skip_cwd = 1;
        cmd_name = raw_cmd + 1;
        argv[0] = (char *)cmd_name;
    }

    char *exec_path = NULL;
    if (!skip_cwd && strchr(cmd_name, '/') != NULL) 
    {
        if (execute_is_executable(cmd_name)) 
        {
            exec_path = strdup(cmd_name);
        }
    } 
    else 
    {
        if (!skip_cwd) 
        {
            char cwd[PATH_MAX];
            if (getcwd(cwd, sizeof(cwd)) != NULL) 
            {
                char cwd_candidate[PATH_MAX + 512];
                snprintf(cwd_candidate, sizeof(cwd_candidate), "%s/%s", cwd, cmd_name);
                if (execute_is_executable(cwd_candidate)) 
                {
                    exec_path = strdup(cwd_candidate);
                }
            }
        }
        if (exec_path == NULL) 
        {
            const char *env_path = getenv("PATH");
            if (env_path != NULL && strlen(env_path) > 0) 
            {
                char *path_copy = strdup(env_path);
                if (path_copy != NULL) 
                {
                    char *saveptr = NULL;
                    char *dir = strtok_r(path_copy, ":", &saveptr);
                    while (dir != NULL) 
                    {
                        char candidate[PATH_MAX + 512];
                        if (strlen(dir) == 0) 
                        {
                            char cwd[PATH_MAX];
                            if (getcwd(cwd, sizeof(cwd)) != NULL) 
                            {
                                snprintf(candidate, sizeof(candidate), "%s/%s", cwd, cmd_name);
                            } 
                            else 
                            {
                                snprintf(candidate, sizeof(candidate), "%s", cmd_name);
                            }
                        } 
                        else 
                        {
                            snprintf(candidate, sizeof(candidate), "%s/%s", dir, cmd_name);
                        }
                        if (execute_is_executable(candidate)) 
                        {
                            exec_path = strdup(candidate);
                            break;
                        }
                        dir = strtok_r(NULL, ":", &saveptr);
                    }
                    free(path_copy);
                }
            }
        }
    }

    if (exec_path == NULL) 
    {
        printf("cshell: command not found (%s)\n", cmd_name);
        exit(1);
    }

    signal(SIGINT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);

    if (num_inputs == 1) 
    {
        int in_fd = open(input_files[0], O_RDONLY);
        if (in_fd >= 0) 
        {
            dup2(in_fd, STDIN_FILENO);
            close(in_fd);
        }
    } 
    else if (num_inputs > 1) 
    {
        int pfd[2];
        if (pipe(pfd) == 0) 
        {
            pid_t feeder = fork();
            if (feeder == 0) 
            {
                close(pfd[0]);
                for (int i = 0; i < num_inputs; i++) 
                {
                    int f = open(input_files[i], O_RDONLY);
                    if (f >= 0) 
                    {
                        char buf[4096];
                        ssize_t bytes;
                        while ((bytes = read(f, buf, sizeof(buf))) > 0) 
                        {
                            ssize_t written = 0;
                            while (written < bytes) 
                            {
                                ssize_t w = write(pfd[1], buf + written, bytes - written);
                                if (w <= 0) break;
                                written += w;
                            }
                        }
                        close(f);
                    }
                }
                close(pfd[1]);
                exit(0);
            }
            close(pfd[1]);
            dup2(pfd[0], STDIN_FILENO);
            close(pfd[0]);
            waitpid(feeder, NULL, 0);
        }
    }

    if (num_outputs == 1) 
    {
        dup2(out_fds[0], STDOUT_FILENO);
        close(out_fds[0]);
    } 
    else if (num_outputs > 1) 
    {
        int out_pfd[2];
        if (pipe(out_pfd) == 0) 
        {
            pid_t writer = fork();
            if (writer == 0) 
            {
                close(out_pfd[1]);
                char buf[4096];
                ssize_t bytes;
                while ((bytes = read(out_pfd[0], buf, sizeof(buf))) > 0) 
                {
                    for (int i = 0; i < num_outputs; i++) 
                    {
                        ssize_t written = 0;
                        while (written < bytes) 
                        {
                            ssize_t w = write(out_fds[i], buf + written, bytes - written);
                            if (w <= 0) break;
                            written += w;
                        }
                    }
                }
                close(out_pfd[0]);
                for (int i = 0; i < num_outputs; i++) 
                {
                    close(out_fds[i]);
                }
                exit(0);
            }
            close(out_pfd[0]);
            dup2(out_pfd[1], STDOUT_FILENO);
            close(out_pfd[1]);
            for (int i = 0; i < num_outputs; i++) 
            {
                close(out_fds[i]);
            }
        }
    }

    execv(exec_path, argv);
    perror("cshell");
    exit(1);
} //ye jo poora code hai woh already kis function mein tah isse accja iska func bana ke ccall kr dete (BUT IM NOT HANGING THIS CUZ STUFF BREAKS AND I NEED TO STUDY FOR MIDSEMS :( (line 589 to 774))

int execute_cmd(const token_list_t *list) 
{
    return execute_cmd_internal(list, 0, 0, NULL, NULL); //calls execute cmd internal with is_bg=0 so a fg process
}

int execute_pipeline_fg(const token_list_t *list, const char *full_cmd_str)  //execute pipeline in fg
{
    if (list==NULL||list->count==0) 
    {
        return 1;
    }

    // count number of pipeline stages
    int num_stages = 1;

    for (size_t i=0; i<list->count; i++) 
    {
        if (list->tokens[i].type==OP_SEMI||list->tokens[i].type==OP_AMP) 
        {
            break;
        }
        if (list->tokens[i].type==OP_PIPE) 
        {
            num_stages++;
        }
    }

    if (num_stages==1) 
    {
        if (list->count>0&&list->tokens[0].type==OP_WORD) 
        {
            const char *cmd_name = list->tokens[0].text;

            if (strcmp(cmd_name, "hop")==0) 
            {
                hop(list);
                return 1;
            } 
            else if (strcmp(cmd_name, "reveal")==0) 
            {
                reveal(list);
                return 1;
            }
            else if (strcmp(cmd_name, "peek")==0) 
            {
                peek(list);
                return 1;
            } 
            else if (strcmp(cmd_name,"locate")==0) 
            {
                locate(list);
                return 1;
            }
            else if (strcmp(cmd_name, "activities")==0) 
            {
                activities();
                return 1;
            }
            else if (strcmp(cmd_name, "resume")==0) 
            {
                resume_cmd(list);
                return 1;
            }
            else if (strcmp(cmd_name, "ping")==0) 
            {
                ping_cmd(list);
                return 1;
            }
            else if (strcmp(cmd_name, "spy")==0) 
            {
                spy_cmd(list);
                return 1;
            }
            else if (strcmp(cmd_name, "snoop")==0) 
            {
                snoop_cmd(list);
                return 1;
            }
        }
        return execute_cmd_internal(list, 0, 0, NULL, full_cmd_str); //if one stage, jst call that command
    }

    // if multi stage pipeline setup
    stage_t stages[256]; //array with all pipeline stages
    int stage_count=0;
    size_t start_idx=0; //where curr stage belongs

    for (size_t i=0; i<list->count; i++) 
    {
        if (list->tokens[i].type==OP_SEMI||list->tokens[i].type==OP_AMP) 
        {
            if (i > start_idx && stage_count<256) 
            {
                stages[stage_count].tokens = &list->tokens[start_idx];
                stages[stage_count].count = i-start_idx;
                stage_count++;
            }
            break;
        }
        if (list->tokens[i].type == OP_PIPE) 
        {
            if (stage_count < 256) 
            {
                stages[stage_count].tokens=&list->tokens[start_idx];
                stages[stage_count].count=i-start_idx;
                stage_count++;
            }
            start_idx=i+1;
        }
    }

    if (start_idx < list->count && stage_count < 256) 
    {
        if (list->tokens[start_idx].type!=OP_SEMI&&list->tokens[start_idx].type!=OP_AMP) 
        {
            stages[stage_count].tokens=&list->tokens[start_idx];
            stages[stage_count].count=list->count - start_idx;
            stage_count++;
        }
    }

    if (stage_count<num_stages) 
    {
        printf("cshell: invalid syntax\n");
        return 0;
    }

    for (int i=0; i<stage_count; i++) 
    {
        if (stages[i].count == 0) {
            printf("cshell: invalid syntax\n");
            return 0;
        }
    }

    int pipes[256][2];
    for (int i=0; i<stage_count-1; i++) 
    {
        if (pipe(pipes[i])<0) 
        {
            perror("cshell: pipe failed");
            return 0;
        }
    }

    pid_t pids[256]; //create pipes
    for (int i=0; i<stage_count; i++)  //num of pipes = no of stages-1
    {
        pids[i] = fork(); //fork once per stage
        if (pids[i] == 0) 
        {
            if (i == 0) 
            {
                setpgid(0, 0); //frst proc grp
            } 
            else 
            {
                setpgid(0, pids[0]); //join frst proc grp
            }

            int has_input_redir = 0;
            for (size_t k=0; k<stages[i].count; k++) 
            {
                if (stages[i].tokens[k].type==OP_LT) 
                {
                    has_input_redir=1;
                    break;
                }
            }
            if (!has_input_redir && i > 0) {
                dup2(pipes[i - 1][0], STDIN_FILENO); //i recives input from prev stage
            }

            int has_output_redir = 0;
            for (size_t k=0; k<stages[i].count; k++) {
                if (stages[i].tokens[k].type==OP_GT||stages[i].tokens[k].type==OP_GTGT) 
                {
                    has_output_redir = 1;
                    break;
                }
            }
            if (!has_output_redir && i < stage_count - 1) 
            {
                dup2(pipes[i][1], STDOUT_FILENO); //receives output from prev stage
            }

            for (int j=0; j<stage_count-1; j++) { //close unused file descriptors
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            token_list_t stage_list = { //stage token list (temp for jst this stage)
                .tokens = stages[i].tokens,
                .count = stages[i].count
            };

            execute_stage_child(&stage_list); //execute stage
            exit(0);
        }
        else 
        {
            if (i == 0) //frst proc becomes proc grp leader (inside the parent proc now)
            {
                setpgid(pids[0], pids[0]);
            } 
            else 
            {
                setpgid(pids[i], pids[0]); //rest follow
            }
        }
    }

    for (int j = 0; j < stage_count - 1; j++) 
    {
        close(pipes[j][0]);
        close(pipes[j][1]);
    }

    if (isatty(STDIN_FILENO)) 
    {
        tcsetpgrp(STDIN_FILENO, pids[0]); //give terminal contril to the pielines proc grp
    }

    int any_stopped = 0;
    for (int i = 0; i < stage_count; i++) 
    {
        int status;
        waitpid(pids[i], &status, WUNTRACED); //wait for each proc
        if (WIFSTOPPED(status)) 
        {
            any_stopped = 1; //if stopped proc, remember
        }
    }

    if (isatty(STDIN_FILENO)) 
    {
        tcsetpgrp(STDIN_FILENO, getpgrp()); //terminal control returns to shell
    }

    if (any_stopped) //if stopped, save (like in a prev code, same implementation)
    {
        int s_id = jobs_alloc_id();
        const char *stage_cmds[256];
        for (int i = 0; i < stage_count; i++) 
        {
            stage_cmds[i] = stages[i].tokens[0].text;
        }
        const char *disp = (full_cmd_str && full_cmd_str[0] != '\0') ? full_cmd_str : stages[0].tokens[0].text;
        jobs_add_stopped(s_id, pids[0], pids, stage_cmds, stage_count, disp); //save stopped pipeline
        printf("[%d] + Stopped    %s\n", s_id, disp);
        fflush(stdout);
        return 0;
    }

    return 1;
}

int execute_pipeline(const token_list_t *list) 
{
    return execute_pipeline_fg(list, NULL);
}

int execute_pipeline_bg(const token_list_t *list, int job_id, const char *full_cmd_str) 
{
    if (list == NULL || list->count == 0) 
    {
        return 1;
    }

    const char *raw_cmd = "";
    for (size_t k = 0; k < list->count; k++) 
    {
        if (list->tokens[k].type == OP_WORD) 
        {
            raw_cmd = list->tokens[k].text;  //find command name
            break;
        }
    }
    if (raw_cmd[0] == '%') 
    {
        raw_cmd++; //remove % if present
    }
    const char *slash = strrchr(raw_cmd, '/'); //find last slash
    const char *cmd_name = slash ? slash + 1 : raw_cmd; // so if it has slash get stuff after slash, else jst command. So /usr/bin/ls: ls and grep : grep

    int num_stages = 1;
    for (size_t i = 0; i < list->count; i++) 
    {
        if (list->tokens[i].type == OP_SEMI || list->tokens[i].type == OP_AMP) 
        {
            break;
        }
        if (list->tokens[i].type == OP_PIPE) 
        {
            num_stages++;
        }
    }

    if (num_stages == 1) //bg buildins MUST run in child
    {
        if (list->count > 0 && list->tokens[0].type == OP_WORD) 
        {
            const char *c_name = list->tokens[0].text;
            if (strcmp(c_name, "hop") == 0 || strcmp(c_name, "reveal") == 0 ||
                strcmp(c_name, "peek") == 0 || strcmp(c_name, "locate") == 0 ||
                strcmp(c_name, "activities") == 0 || strcmp(c_name, "resume") == 0 ||
                strcmp(c_name, "ping") == 0 || strcmp(c_name, "spy") == 0 ||
                strcmp(c_name, "snoop") == 0) 
            {
                int sync_pfd[2]; //synchronization pipe
                if (pipe(sync_pfd) < 0) 
                {
                    perror("cshell: pipe failed");
                    return 0;
                }

                pid_t pid = fork();
                if (pid < 0) 
                {
                    perror("cshell: fork failed");
                    close(sync_pfd[0]);
                    close(sync_pfd[1]);
                    return 0;
                }
                if (pid == 0) //so fork, and run builtin in child
                {
                    close(sync_pfd[1]);
                    setpgid(0, 0);

                    char ch;
                    ssize_t r = read(sync_pfd[0], &ch, 1); //parent starts job so child WAITS
                    (void)r;
                    close(sync_pfd[0]);

                    signal(SIGINT, SIG_DFL);
                    signal(SIGTSTP, SIG_DFL);
                    signal(SIGTTIN, SIG_DFL);
                    signal(SIGTTOU, SIG_DFL);

                    if (strcmp(c_name, "hop") == 0) hop(list);
                    else if (strcmp(c_name, "reveal") == 0) reveal(list);
                    else if (strcmp(c_name, "peek") == 0) peek(list);
                    else if (strcmp(c_name, "locate") == 0) locate(list);
                    else if (strcmp(c_name, "activities") == 0) activities();
                    else if (strcmp(c_name, "resume") == 0) resume_cmd(list);
                    else if (strcmp(c_name, "ping") == 0) ping_cmd(list);
                    else if (strcmp(c_name, "spy") == 0) spy_cmd(list);
                    else if (strcmp(c_name, "snoop") == 0) snoop_cmd(list);
                    exit(0);
                }

                setpgid(pid, pid);
                close(sync_pfd[0]);
                printf("[%d] %d\n", job_id, (int)pid); //parent prints job
                fflush(stdout);
                ssize_t w = write(sync_pfd[1], "1", 1); //child proceeds
                (void)w;
                close(sync_pfd[1]);

                jobs_add(job_id, pid, cmd_name, full_cmd_str);
                return 1;
            }
        }
        return execute_cmd_internal(list, 1, job_id, cmd_name, full_cmd_str);
    }
//again do:
//split into s=tages
//create pipes
//fork each
//connect to stdin/stdout
//create one proc grp
//sync childeren
//regster pipeline as ONE JOB

    stage_t stages[256];
    int stage_count = 0;
    size_t start_idx = 0;

    for (size_t i = 0; i < list->count; i++) 
    {
        if (list->tokens[i].type == OP_SEMI || list->tokens[i].type == OP_AMP) 
        {
            if (i > start_idx && stage_count < 256) 
            {
                stages[stage_count].tokens = &list->tokens[start_idx];
                stages[stage_count].count = i - start_idx;
                stage_count++;
            }
            break;
        }
        if (list->tokens[i].type == OP_PIPE) 
        {
            if (stage_count < 256) 
            {
                stages[stage_count].tokens = &list->tokens[start_idx];
                stages[stage_count].count = i - start_idx;
                stage_count++;
            }
            start_idx = i + 1;
        }
    }

    if (start_idx < list->count && stage_count < 256) 
    {
        if (list->tokens[start_idx].type != OP_SEMI && list->tokens[start_idx].type != OP_AMP) 
        {
            stages[stage_count].tokens = &list->tokens[start_idx];
            stages[stage_count].count = list->count - start_idx;
            stage_count++;
        }
    }

    if (stage_count < num_stages) 
    {
        printf("cshell: invalid syntax\n");
        return 0;
    }

    for (int i = 0; i < stage_count; i++) 
    {
        if (stages[i].count == 0) 
        {
            printf("cshell: invalid syntax\n");
            return 0;
        }
    }

    int pipes[256][2];
    for (int i = 0; i < stage_count - 1; i++) 
    {
        if (pipe(pipes[i]) < 0) 
        {
            perror("cshell: pipe failed");
            return 0;
        }
    }
//same stuff repeated (comments to explain in prev instance of the code)
    int sync_pfd[2]; 
    if (pipe(sync_pfd) < 0) 
    {
        perror("cshell: pipe failed");
        for (int j = 0; j < stage_count - 1; j++) 
        {
            close(pipes[j][0]);
            close(pipes[j][1]);
        }
        return 0;
    }

    pid_t pids[256];
    for (int i = 0; i < stage_count; i++) 
    {
        pids[i] = fork();
        if (pids[i] == 0) 
        {
            if (i == 0) 
            {
                setpgid(0, 0);
            } 
            else 
            {
                setpgid(0, pids[0]);
            }

            close(sync_pfd[1]);
            char ch;
            ssize_t r = read(sync_pfd[0], &ch, 1);
            (void)r;
            close(sync_pfd[0]);

            int has_input_redir = 0;
            for (size_t k = 0; k < stages[i].count; k++) 
            {
                if (stages[i].tokens[k].type == OP_LT) 
                {
                    has_input_redir = 1;
                    break;
                }
            }
            if (!has_input_redir && i > 0) 
            {
                dup2(pipes[i - 1][0], STDIN_FILENO);
            }

            int has_output_redir = 0;
            for (size_t k = 0; k < stages[i].count; k++) 
            {
                if (stages[i].tokens[k].type == OP_GT || stages[i].tokens[k].type == OP_GTGT) 
                {
                    has_output_redir = 1;
                    break;
                }
            }
            if (!has_output_redir && i < stage_count - 1) 
            {
                dup2(pipes[i][1], STDOUT_FILENO);
            }

            for (int j = 0; j < stage_count - 1; j++) 
            {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            token_list_t stage_list = {
                .tokens = stages[i].tokens,
                .count = stages[i].count
            };

            execute_stage_child(&stage_list);
            exit(0);
        }
        else 
        {
            if (i == 0) 
            {
                setpgid(pids[0], pids[0]);
            } 
            else 
            {
                setpgid(pids[i], pids[0]);
            }
        }
    }

    for (int j = 0; j < stage_count - 1; j++) 
    {
        close(pipes[j][0]);
        close(pipes[j][1]);
    }

    close(sync_pfd[0]);
    printf("[%d] %d\n", job_id, (int)pids[0]);
    fflush(stdout);
    ssize_t w = write(sync_pfd[1], "1", 1);
    (void)w;
    close(sync_pfd[1]);

    const char *stage_cmds[256];
    for (int i = 0; i < stage_count; i++) 
    {
        stage_cmds[i] = stages[i].tokens[0].text;
    }
    const char *disp = (full_cmd_str && full_cmd_str[0] != '\0') ? full_cmd_str : stages[0].tokens[0].text;
    jobs_add_pipeline(job_id, pids[0], pids, stage_cmds, stage_count, disp);
    return 1;
}
