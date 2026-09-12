#ifndef JOBS_H
#define JOBS_H

#include "lexer.h"
#include <sys/types.h>

typedef enum {
    PROC_RUNNING,
    PROC_STOPPED,
    PROC_EXITED
}proc_state_t;

typedef struct {
    pid_t pid;
    char cmd_name[256];
    proc_state_t state;
}proc_info_t;

#define MAX_PIDS_PER_JOB 64

typedef struct {
    int job_id;
    pid_t pgid;
    proc_info_t procs[MAX_PIDS_PER_JOB];
    int active;
    int finished;
    int reported;
    int is_stopped;
    int num_procs;
    char full_cmd[512];
    int exit_status;
    int is_signaled;
}job_entry_t;

void jobs_init(void);
int jobs_alloc_id(void);
void jobs_add(int job_id, pid_t pid, const char *cmd_name, const char *full_cmd);
void jobs_add_pipeline(int job_id, pid_t pgid, const pid_t *pids, const char *const *cmd_names, int num_pids, const char *full_cmd);
void jobs_add_stopped(int job_id, pid_t pgid, const pid_t *pids, const char *const *cmd_names, int num_pids, const char *full_cmd);
void jobs_set_fg_active(int active);
void jobs_check_completed(void);
void execute_command_line(const token_list_t *list);

int jobs_has_stopped(void);
void jobs_send_sighup_all(void);

void jobs_refresh_states(void);
void jobs_refresh_and_print_activities(void);

job_entry_t *jobs_get_by_id(int job_id);
int jobs_pid_is_tracked(pid_t pid);
int jobs_resume_bg(int job_id);
int jobs_resume_fg(int job_id, int timeout_seconds);
int jobs_send_signal_to_job(int job_id, int sig);
int jobs_send_signal_to_pid(pid_t pid, int sig);

#endif