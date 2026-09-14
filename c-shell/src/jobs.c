#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "execute.h"
#include "jobs.h"
#include "lexer.h"
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>

#define MAX_BG_JOBS 256

static job_entry_t jobs_list[MAX_BG_JOBS]; //store bg jobs
static int next_job_id=1; //next job number
static volatile sig_atomic_t fg_active=0; //is fg job active or not (is it blocking the shell)


int jobs_alloc_id(void) 
{
    return next_job_id++; //ret curr id, then increment it
}

static void sigchld_handler(int sig) 
{
    (void)sig;
    int saved_errno=errno; //save err cuz handler may change it
    int status;

    for (int i=0; i<MAX_BG_JOBS; i++) //ignores SIGTTOU and SIGTTIN, so if shell tries to write to the terminal while running a process in the background, the Linux kernel would freeze the shell.
    {
        if (jobs_list[i].active&&!jobs_list[i].finished) //active unfinised jobs to run
        {
            for (int k=0; k<jobs_list[i].num_procs; k++) 
            {
                pid_t pid=jobs_list[i].procs[k].pid;
                pid_t res=waitpid(pid, &status, WNOHANG | WUNTRACED | WCONTINUED);
                //check prc w/o blocking it
                if (res==pid) 
                {
                    if (WIFEXITED(status)||WIFSIGNALED(status)) 
                    {
                        jobs_list[i].procs[k].state=PROC_EXITED; //proc finished
                        if (pid==jobs_list[i].pgid) 
                        {
                            jobs_list[i].finished=1; //job finished
                            jobs_list[i].exit_status=status;
                            jobs_list[i].is_signaled=WIFSIGNALED(status);
                        }
                    } 
                    else if (WIFSTOPPED(status)) 
                    {
                        jobs_list[i].procs[k].state=PROC_STOPPED; //prcess stopped
                        jobs_list[i].is_stopped=1;
                    } 
                    else if (WIFCONTINUED(status)) 
                    {
                        jobs_list[i].procs[k].state=PROC_RUNNING; //process resumed
                        jobs_list[i].is_stopped=0;
                    }
                }
            }
        }
    }
    errno=saved_errno; //restor errno
}

void jobs_init(void) 
{
    struct sigaction sa;
    memset(&sa,0,sizeof(sa)); //clear signal actiokn structure
    sa.sa_handler=sigchld_handler; //use SIGCHILD handler
    sigemptyset(&sa.sa_mask); //no extra signals blocked
    sa.sa_flags=0; // dont restart blocked syscalls
    if (sigaction(SIGCHLD,&sa,NULL)<0) 
    {
        perror("cshell: sigaction failed"); //insrallation failure
    }

    signal(SIGTTOU, SIG_IGN); //dont stop shell on terminal out control
    signal(SIGTTIN, SIG_IGN); //dont stop shell on terminal in control
}


void jobs_add(int job_id, pid_t pid, const char *cmd_name, const char *full_cmd) 
{
    pid_t pids[1]={pid}; //one proc pipeline
    const char *names[1]={cmd_name}; //store command name
    jobs_add_pipeline(job_id, pid, pids, names, 1, full_cmd); //reuse pipeline func
}

void jobs_set_fg_active(int active) 
{
    fg_active = active;
}

int jobs_has_stopped(void) //check f stopped jobs exist
{
    jobs_refresh_states();
    for (int i=0; i<MAX_BG_JOBS; i++) 
    {
        if (jobs_list[i].active&&jobs_list[i].is_stopped) 
        {
            return 1; //atleast one stopped job xists
        }
    }
    return 0;
}

void jobs_send_sighup_all(void) 
{
    for (int i=0; i<MAX_BG_JOBS; i++) 
    {
        if (jobs_list[i].active && jobs_list[i].pgid > 1) 
        {
            kill(-jobs_list[i].pgid, SIGHUP); //send sighup to whole proc group
        }
    }
}

static int compare_job_ptrs(const void *a, const void *b) //qsorr fnc for comparing
{
    const job_entry_t *j1=*(const job_entry_t * const *)a;
    const job_entry_t *j2=*(const job_entry_t * const *)b;
    return j1->job_id-j2->job_id; //sort buy job num
}

job_entry_t *jobs_get_by_id(int job_id) 
{
    jobs_refresh_states(); //state is curr (recheck)
    for (int i = 0; i<MAX_BG_JOBS; i++) 
    {
        if (jobs_list[i].active && jobs_list[i].job_id == job_id) 
        {
            for (int k = 0; k < jobs_list[i].num_procs; k++) 
            {
                if (jobs_list[i].procs[k].state != PROC_EXITED) 
                {
                    return &jobs_list[i]; //return matching active job
                }
            }
        }
    }
    return NULL; //job not found
}

void jobs_check_completed(void) 
{
    jobs_refresh_states(); //update states

    for (int i=0; i<MAX_BG_JOBS; i++) 
    {
        if (jobs_list[i].active && jobs_list[i].finished && !jobs_list[i].reported && !jobs_list[i].is_stopped) 
        {
            const char *first_cmd = jobs_list[i].procs[0].cmd_name;
            printf("%s with pid %d exited %s\n",
                   first_cmd,
                   (int)jobs_list[i].pgid,
                   jobs_list[i].is_signaled ? "abnormally" : "normally"); //how does teh job end?
            fflush(stdout);
            jobs_list[i].reported=1; //prevent duplicates
            jobs_list[i].active=0; //remove active jobs
        }
    }
}

int jobs_pid_is_tracked(pid_t pid) 
{
    jobs_refresh_states(); //update proc
    for (int i=0; i<MAX_BG_JOBS; i++) 
    {
        if (jobs_list[i].active) 
        {
            for (int k = 0; k < jobs_list[i].num_procs; k++) 
            {
                if (jobs_list[i].procs[k].pid==pid && jobs_list[i].procs[k].state!=PROC_EXITED) 
                {
                    return 1; //pid f active job
                }
            }
        }
    }
    return 0; // pid idnt tracked
}



void jobs_add_pipeline(int job_id, pid_t pgid, const pid_t *pids, const char *const *cmd_names, int num_pids, const char *full_cmd) 
{
    for (int i = 0; i<MAX_BG_JOBS; i++) 
    {
        if (!jobs_list[i].active) //find empty slot
        {
            jobs_list[i].job_id=job_id;
            jobs_list[i].pgid=pgid;

            if (num_pids>MAX_PIDS_PER_JOB)
            {
            jobs_list[i].num_procs=MAX_PIDS_PER_JOB;
            }
            else
            {
            jobs_list[i].num_procs=num_pids;
            }

            //list num of stored processes
            for (int k = 0; k < jobs_list[i].num_procs; k++) 
            {
                jobs_list[i].procs[k].pid = pids[k]; //store pid
                strncpy(jobs_list[i].procs[k].cmd_name, cmd_names[k], sizeof(jobs_list[i].procs[k].cmd_name)-1); //copy cmd name
                jobs_list[i].procs[k].cmd_name[sizeof(jobs_list[i].procs[k].cmd_name)-1]='\0';
                jobs_list[i].procs[k].state = PROC_RUNNING;
            }
            if (full_cmd) 
            {
                strncpy(jobs_list[i].full_cmd, full_cmd, sizeof(jobs_list[i].full_cmd) - 1);
                jobs_list[i].full_cmd[sizeof(jobs_list[i].full_cmd) - 1] = '\0';
            } 
            else 
            {
                jobs_list[i].full_cmd[0] = '\0'; //no command string
            }
            jobs_list[i].active = 1;
            jobs_list[i].is_stopped = 0;
            jobs_list[i].finished = 0;
            jobs_list[i].reported = 0;
            jobs_list[i].exit_status = 0;
            jobs_list[i].is_signaled = 0;
            break;
        }
    }
}

void jobs_add_stopped(int job_id, pid_t pgid, const pid_t *pids, const char *const *cmd_names, int num_pids, const char *full_cmd) 
{
    jobs_add_pipeline(job_id, pgid, pids, cmd_names, num_pids, full_cmd); //add normally
    for (int i=0; i<MAX_BG_JOBS; i++) 
    {
        if (jobs_list[i].active&&jobs_list[i].job_id==job_id) 
        {
            jobs_list[i].is_stopped=1; //whole job stopped
            for (int k = 0; k < jobs_list[i].num_procs; k++) 
            {
                jobs_list[i].procs[k].state=PROC_STOPPED; //stop all processes
            }
            break;
        }
    }
}

int jobs_send_signal_to_job(int job_id, int sig) 
{
    job_entry_t *job = jobs_get_by_id(job_id);
    if (!job) 
    {
        return 0;
    }
    kill(-job->pgid, sig); //signal entire job / process grp
    return 1; //process
}

int jobs_send_signal_to_pid(pid_t pid, int sig) 
{
    if (!jobs_pid_is_tracked(pid)) 
    {
        return 0;
    }
    kill(pid, sig); //signal ONLY this process
    return 1;
}


void jobs_refresh_and_print_activities(void) 
{
    jobs_refresh_states(); //update first

    job_entry_t *active_jobs[MAX_BG_JOBS];
    int count = 0;

    for (int i = 0; i<MAX_BG_JOBS; i++) 
    {
        if (!jobs_list[i].active) 
        {
            continue; //ignore inactive jobs
        }

        int has_alive=0;
        for (int k=0; k<jobs_list[i].num_procs; k++) 
        {
            if (jobs_list[i].procs[k].state != PROC_EXITED) 
            {
                has_alive=1;  //job ha live process
                break;
            }
        }
        if (!has_alive) continue;

        active_jobs[count++]=&jobs_list[i]; //add to printable list
    }

    qsort(active_jobs, count,sizeof(job_entry_t *),compare_job_ptrs); //sort by job ids

    for (int i=0; i<count; i++) 
    {
        job_entry_t *job = active_jobs[i];

        // print [job_num] pgid pgid_value 
        printf("[%d] pgid %d\n", job->job_id, (int)job->pgid);

        // Process lines: "  pid command_name state"
        for (int k = 0; k < job->num_procs; k++) 
        {
            if (job->procs[k].state != PROC_EXITED) 
            {
                const char *st_str = (job->procs[k].state == PROC_STOPPED) ? "Stopped" : "Running";
                printf("  %d %s %s\n", (int)job->procs[k].pid, job->procs[k].cmd_name, st_str); //print each process
            }
        }
    }
    fflush(stdout);
}

static volatile sig_atomic_t resume_timed_out = 0;

static void resume_timeout_handler(int sig) 
{
    (void)sig;
    resume_timed_out = 1;
}


// RESEUME IMPLEMENTATION

int jobs_resume_bg(int job_id) 
{
    job_entry_t *job = jobs_get_by_id(job_id);
    if (!job) 
    {
        printf("resume: no such job\n"); //no such existing job
        return 0;
    }

    kill(-job->pgid, SIGCONT); //cont entire proc grp
    job->is_stopped=0;

    for (int k=0; k<job->num_procs; k++) 
    {
        if (job->procs[k].state==PROC_STOPPED) 
        {
            job->procs[k].state=PROC_RUNNING; //mark proc as runnig
        }
    }

    printf("[%d] + Running %s\n", job->job_id, job->full_cmd);
    fflush(stdout);
    return 1; //success
}


int jobs_resume_fg(int job_id, int timeout_seconds) 
{
    job_entry_t *job = jobs_get_by_id(job_id);
    if (!job) 
    {
        printf("resume: no such job\n");
        return 0; //no such job
    }

    printf("%s\n", job->full_cmd);
    fflush(stdout);

    if (isatty(STDIN_FILENO)) 
    {
        tcsetpgrp(STDIN_FILENO, job->pgid); //terminal control to job
    }

    kill(-job->pgid, SIGCONT); //resume whole proc grp
    job->is_stopped = 0;
    for (int k=0; k<job->num_procs; k++) 
    {
        if (job->procs[k].state==PROC_STOPPED) 
        {
            job->procs[k].state=PROC_RUNNING;
        }
    }

    resume_timed_out = 0;
    struct sigaction old_sa,sa;
    if (timeout_seconds>0) 
    {
        memset(&sa,0,sizeof(sa));
        sa.sa_handler=resume_timeout_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags=0;
        sigaction(SIGALRM,&sa,&old_sa);
        alarm(timeout_seconds); //start timeout timer
    }

    jobs_set_fg_active(1); //tell fg is actibe to shell

    int any_stopped=0;
    int all_done=0;

    while(1) 
    {
        if(resume_timed_out) 
        {
            break; //stop waitin when timeout
        }

        int alive_count=0;
        any_stopped=0;

        for (int k=0; k<job->num_procs; k++) 
        {
            if (job->procs[k].state==PROC_EXITED) 
            {
                continue; //ignore finished jobs
            }

            int status;
            pid_t res = waitpid(job->procs[k].pid, &status, WUNTRACED | WNOHANG);
            //chec proc w/o blocking

            if (res==job->procs[k].pid) 
            {
                if (WIFEXITED(status)||WIFSIGNALED(status)) 
                {
                    job->procs[k].state=PROC_EXITED; //proc ended 
                } 
                else if (WIFSTOPPED(status)) 
                {
                    job->procs[k].state=PROC_STOPPED;
                    any_stopped=1; //job stopped
                }
            } 
            else if (res<0 && errno==ECHILD) 
            {
                job->procs[k].state=PROC_EXITED; 
            } 
            else 
            {
                alive_count++; //process still alive
                if (job->procs[k].state==PROC_STOPPED) 
                {
                    any_stopped=1;
                }
            }
        }

        if (alive_count==0) 
        {
            all_done=1; //all proc finished
            break;
        }

        if (any_stopped) 
        {
            break; //job stopped
        }
        struct timespec req = { .tv_sec = 0, .tv_nsec = 10000000 };
        nanosleep(&req, NULL); //sleep before checking again (10ms)
    }

    if (timeout_seconds>0) 
    {
        alarm(0); //cancel timeout
        sigaction(SIGALRM,&old_sa,NULL); //restore old alarm handler
    }

    if (isatty(STDIN_FILENO)) 
    {
        tcsetpgrp(STDIN_FILENO, getpgrp()); //giev terminal back to shell
    }

    jobs_set_fg_active(0); //fg job is no loner ative

    if (resume_timed_out) 
    {
        kill(-job->pgid, SIGTERM); //terminate timed out job
        printf("resume: job timed out\n");
        fflush(stdout);
        job->active=0;
        job->finished=1;
        return 1;
    }

    if (any_stopped) 
    {
        job->is_stopped = 1;
        printf("[%d] + Stopped %s\n", job->job_id, job->full_cmd);
        fflush(stdout);
        return 0;
    }

    if (all_done) 
    {
        job->active=0;
        job->finished=1; //job completed
    }

    return 1; //resume succeeded
}


static void reconstruct_cmd_str(const token_list_t *list, char *out, size_t out_len) 
{
    out[0]='\0'; //start wuth empty string
    size_t cur = 0;
    for (size_t i=0; i<list->count; i++) 
    {
        const char *text = "";
        switch (list->tokens[i].type) 
        {
            case OP_WORD: text = list->tokens[i].text ? list->tokens[i].text : ""; break;
            case OP_PIPE: text = "|"; break;
            case OP_LT: text = "<"; break;
            case OP_GT: text = ">"; break;
            case OP_GTGT: text = ">>"; break;
            default: break;
        }
        if (text[0]!='\0') 
        {
            if (cur>0&&cur+1< out_len) 
            {
                out[cur++]=' '; //speerate tokens with space
                out[cur]='\0';
            }
            size_t tlen = strlen(text);
            if (cur+tlen<out_len) 
            {
                strcpy(out+cur, text); //add token to comamnd
                cur+=tlen;
            }
        }
    }
}

void execute_command_line(const token_list_t *list) 
{
    if (list==NULL||list->count==0) 
    {
        return; //noting to execute
    }

    size_t i=0;
    while (i<list->count) 
    {
        size_t start = i;
        while (i<list->count && list->tokens[i].type!=OP_SEMI && list->tokens[i].type!=OP_AMP) 
        {
            i++;
        }
        //end of curr command

        size_t count=i-start;
        int is_bg=0;
        if (i<list->count) 
        {
            if (list->tokens[i].type==OP_AMP) 
            {
                is_bg=1; //& means bg
            }
            i++;
        }

        if (count==0) 
        {
            continue; //empty command -> ignore
        }

        token_list_t cmd_tokens={.tokens = &list->tokens[start],.count = count};
        // create token list for this command

        char full_cmd_str[512];
        reconstruct_cmd_str(&cmd_tokens, full_cmd_str, sizeof(full_cmd_str)); //rebuils command string
        
        if (is_bg) 
        {
            int job_id = jobs_alloc_id(); //bg job id get
            execute_pipeline_bg(&cmd_tokens, job_id, full_cmd_str); //execute w/o waiting
        } 
        else 
        {
            jobs_set_fg_active(1); //fg execution
            int ok=execute_pipeline_fg(&cmd_tokens, full_cmd_str); //exec plus wait
            jobs_set_fg_active(0); //report cmpleted fg jobs
            jobs_check_completed(); //report fin bg job
            
            if (!ok) 
            {
                break; //stop if fg failure
            }
        }
    }

}

void jobs_refresh_states(void) 
{
    int status;
    for (int i=0; i<MAX_BG_JOBS; i++) 
    {
        if (!jobs_list[i].active) 
        {
            continue; //ignptre inactive jobs
        }

        int any_alive=0;
        int any_stopped=0;

        for (int k = 0; k < jobs_list[i].num_procs; k++) 
        {
            if (jobs_list[i].procs[k].state == PROC_EXITED) 
            {
                continue; //already fin
            }

            pid_t pid = jobs_list[i].procs[k].pid;
            pid_t res = waitpid(pid, &status, WNOHANG | WUNTRACED | WCONTINUED);
            //curr process state

            if (res == pid) 
            {
                if (WIFEXITED(status) || WIFSIGNALED(status)) 
                {
                    jobs_list[i].procs[k].state = PROC_EXITED; //proc ended
                    if (pid == jobs_list[i].pgid) 
                    {
                        jobs_list[i].finished = 1;
                        jobs_list[i].exit_status = status;
                        jobs_list[i].is_signaled = WIFSIGNALED(status);
                    }
                    continue;
                } 
                else if (WIFSTOPPED(status)) 
                {
                    jobs_list[i].procs[k].state=PROC_STOPPED; //proc stopped
                } 
                else if (WIFCONTINUED(status)) 
                {
                    jobs_list[i].procs[k].state=PROC_RUNNING; //proc continued
                }
            } 
            else if (res<0&&errno==ECHILD) 
            {
                jobs_list[i].procs[k].state=PROC_EXITED; //child no longer exists
                continue;
            }

            // inspect /proc/pid/stat
            char statpath[64];
            snprintf(statpath, sizeof(statpath), "/proc/%d/stat", (int)pid); //buld process-stat path

            FILE *sf=fopen(statpath,"r");

            if (sf) 
            {
                char line[256];
                if (fgets(line, sizeof(line), sf)) 
                {
                    char *rparen = strrchr(line, ')');
                    //find end of proc name

                    if (rparen&&*(rparen+1)==' ') 
                    {
                        char state_c=*(rparen + 2); //extract proc srate char
                        if (state_c=='T'||state_c=='t') 
                        {
                            jobs_list[i].procs[k].state = PROC_STOPPED;
                        } 
                        else if (state_c=='Z') 
                        {
                            jobs_list[i].procs[k].state = PROC_EXITED;
                        }
                    }
                }
                fclose(sf);
            } 
            else 
            {
                jobs_list[i].procs[k].state = PROC_EXITED;
                continue;
            }

            if (jobs_list[i].procs[k].state != PROC_EXITED) 
            {
                any_alive=1; //atleas one is alive
                if (jobs_list[i].procs[k].state == PROC_STOPPED) 
                {
                    any_stopped=1; //atleast one is stopped
                }
            }
        }

        jobs_list[i].is_stopped=any_stopped; //update job level state
        if (!any_alive) 
        {
            if (jobs_list[i].reported || jobs_list[i].is_stopped) 
            {
                jobs_list[i].active=0; //remove complete job from active list
            }
        }
    }
}