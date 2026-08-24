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

#ifndef PATH_MAX
#define PATH_MAX 5000
#endif

typedef struct{
    token_t *tokens;
    size_t count;
}stage_t;

typedef struct{
    char *path;
    int is_append;
}output_redir_t;

int execute_is_executable(const char *filepath) {
    struct stat st;
    if (access(filepath, X_OK) != 0) return 0; //does path have exec permissin
    if (stat(filepath, &st) != 0) return 0; //get info about te path
    if (S_ISDIR(st.st_mode)) return 0; //dir not executables
    return 1; //if executable hai, return 1
}


void execute_cmd(const token_list_t *list) 
{
    if (list == NULL || list->count == 0) 
    {
        return;// if no token list nothing to execute
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

    for (size_t i=0; i<group_count; i++) 
    {
        if (list->tokens[i].type == OP_LT) //so if < somewhere(input redir)
        {
            if (i+1<group_count&&list->tokens[i+1].type==OP_WORD) 
            { 
                // must be followed by a <
                input_files[num_inputs++]=list->tokens[i+1].text;
                i++; // since filename already processed, skip it
            } 
            else 
            {
                printf("cshell: invalid syntax\n"); // no word after <, so no filename to get input from
                return;
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
                return;
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
                return;
            }
        } 
        else if (list->tokens[i].type==OP_WORD&&argc<255) 
        {
            argv[argc++]=list->tokens[i].text; // so argv has stuff like cat, echo, file.txt etc.
        }
    }

    argv[argc]=NULL; // so the argv MUST end with a NULL

    if (argc==0) 
    {
        return;
    }

    // check if input file exist and can be opened with O_RDONLY
    for (int i=0; i<num_inputs; i++) 
    {
        int fd = open(input_files[i], O_RDONLY); // check all input files can be opened
        if (fd < 0) 
        {
            printf("cshell: no such file or directory\n");
            return;
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
            mid=O_APPEND;
        } 
        else 
        {
            mid=O_TRUNC;
        }
        int flags=O_WRONLY|O_CREAT|mid;

        out_fds[i]=open(output_files[i].path, flags, 0644);
        if (out_fds[i]<0) // if cant create a file to write
        {
            printf("cshell: unable to create file for writing\n"); 
            for (int j = 0; j < i; j++) 
            {
                close(out_fds[j]); // close all open files
            }
            return;
        }
    }

    const char *raw_cmd = argv[0]; // read command from argv

    int skip_cwd = 0; //skip the cwd? 
    const char *cmd_name = raw_cmd;

    if (raw_cmd[0] == '%') // means DONT current dir
    {
        skip_cwd = 1; // skip cwd
        cmd_name = raw_cmd + 1; 
        argv[0] = (char *)cmd_name; //argv must contain command with %
    }

    //fulle executable path

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
        return;
    }

    pid_t pid = fork(); // create a child process
    if (pid == 0) 
    {
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
        else if (num_inputs > 1) 
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

        execv(exec_path, argv); 
        perror("cshell"); // if exec succeds, then this line isnt run
        exit(1);
    } 
    else if (pid > 0) 
    {
        // child inherited output file descriptors durin fork
        for (int i = 0; i < num_outputs; i++) 
        {
            close(out_fds[i]);
        }
        int status;
        waitpid(pid, &status, 0); // waut for command to finish
    } 
    else 
    {
        perror("cshell: fork failed"); // if fork fails
        for (int i = 0; i < num_outputs; i++) 
        {
            close(out_fds[i]);
        }
    }

    free(exec_path);
}




void execute_pipeline(const token_list_t *list) 
{
    if (list==NULL||list->count==0) 
    {
        return;
    }

    // count number of pipeline stages, so if ; or &, u break thr stages there, if | then increase the number fo stages
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
        
        if (list->count>0&&list->tokens[0].type==OP_WORD)  //is forst command a word(cat, exec, ocate et.)
        {
            const char *cmd_name = list->tokens[0].text;

            if (strcmp(cmd_name, "hop")==0) 
            {
                hop(list);
                return;
            } 
            else if (strcmp(cmd_name, "reveal")==0) 
            {
                reveal(list);
                return;
            }
            else if (strcmp(cmd_name, "peek")==0) 
            {
                peek(list);
                return;
            } 
            else if (strcmp(cmd_name,"locate")==0) 
            {
                locate(list);
                return;
            }
        }
        execute_cmd(list);
        return;
    }

    // if multi stage pipeline setup
    stage_t stages[256]; // each pipeline stage
    int stage_count = 0; // which stage were on
    size_t start_idx = 0;

    for (size_t i=0; i<list->count; i++)  // loop through entire token list
    {
        if (list->tokens[i].type==OP_SEMI||list->tokens[i].type==OP_AMP) 
        {
            if (i > start_idx && stage_count<256) 
            {
                stages[stage_count].tokens = &list->tokens[start_idx];
                stages[stage_count].count = i-start_idx;//are tehre actually any tokesn in thisstage
                stage_count++;
            }
            break;
        }
        if (list->tokens[i].type == OP_PIPE) 
        {
            if (stage_count < 256) 
            {
                stages[stage_count].tokens=&list->tokens[start_idx];
                stages[stage_count].count=i-start_idx; //are tehre actually any tokesn in this pipeline
                stage_count++;
            }
            start_idx=i+1; // increase teh start index by 1 cuz one stage has been done
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

    if (stage_count<num_stages) // if extracted stages are NOT equal to og ones, theres something wrong with pipeline
    {
        printf("cshell: invalid syntax\n");
        return;
    }

    for (int i=0; i<stage_count; i++) 
    {
        if (stages[i].count == 0) {
            printf("cshell: invalid syntax\n"); // if a target is e mpty, its an invalid syntax
            return;
        }
    }

    int pipes[256][2];
    for (int i=0; i<stage_count-1; i++) 
    {
        if (pipe(pipes[i])<0) // if pipe failed
        {
            perror("cshell: pipe failed");
            return;
        }
    }

    pid_t pids[256];
    for (int i=0; i<stage_count; i++) 
    {
        pids[i] = fork();
        if (pids[i] == 0) 
        {
            // child process per stage (stage list gone through by i)

            // does code have <?? then u needinput redirect
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
                dup2(pipes[i - 1][0], STDIN_FILENO); // if no < and its not first process, then input has been piped from somewhere else
            }

            // if has > or >> , then output redirect
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
                dup2(pipes[i][1], STDOUT_FILENO);
            }

           // close all pipe descriptors  
            for (int j=0; j<stage_count-1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            /* 4. Package stage tokens */
            token_list_t stage_list = {
                .tokens = stages[i].tokens,
                .count = stages[i].count
            };

            // same as for the single stage command
            if (stage_list.count > 0 && stage_list.tokens[0].type == OP_WORD) 
            {
                const char *cmd_name = stage_list.tokens[0].text;
                if (strcmp(cmd_name, "hop")==0) 
                {
                    hop(&stage_list);
                    exit(0);
                } 
                else if (strcmp(cmd_name, "reveal")==0) 
                {
                    reveal(&stage_list);
                    exit(0);
                } 
                else if (strcmp(cmd_name, "peek")==0) 
                {
                    peek(&stage_list);
                    exit(0);
                } 
                else if (strcmp(cmd_name, "locate")==0) 
                {
                    locate(&stage_list);
                    exit(0);
                }
            }

            execute_cmd(&stage_list);
            exit(0);
        }
    }

    // parent shell cloases every pipe descriptor it holds
    for (int j = 0; j < stage_count - 1; j++) 
    {
        close(pipes[j][0]);
        close(pipes[j][1]);
    }

    // paret shell waits for all commands in pipeline
    for (int i = 0; i < stage_count; i++) 
    {
        int status;
        waitpid(pids[i], &status, 0);
    }
}
