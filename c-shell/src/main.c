#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <limits.h>
#include "lexer.h"
#include "parser.h"
#include "hop.h"
#include "reveal.h"
#include "peek.h"
#include "locate.h"
#include "execute.h"
#include <errno.h>
#include <signal.h>
#include "jobs.h"

#define MAX_INPUT_LEN 1024

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static char home_dir[PATH_MAX]; // dir shell was started in
static char username[LOGIN_NAME_MAX + 1]; //user name
static char hostname_buf[256]; // host name

// get identity for shell startup
static void init_shell_identity(void) {
    if (getcwd(home_dir, sizeof(home_dir)) == NULL) 
    {
        strcpy(home_dir, "/"); //get cwd
    }

    struct passwd *pw = getpwuid(getuid());
    if (pw != NULL && pw->pw_name != NULL) 
    {
        strncpy(username, pw->pw_name, sizeof(username) - 1); //get username else jst use user
        username[sizeof(username) - 1] = '\0';
    } 
    else 
    {
        strcpy(username, "user");
    }

    if (gethostname(hostname_buf, sizeof(hostname_buf)) != 0) 
    {
        strcpy(hostname_buf, "host"); //get host name
    }
}

// if cwd has home as ancestor, prefix of hime dir is replaced with ~
static void get_display_path(char *out, size_t outsize) 
{
    char cwd[PATH_MAX]; 
    if (getcwd(cwd, sizeof(cwd)) == NULL) 
    {
        snprintf(out, outsize, "?"); // if cwd is NULL
        return;
    }

    size_t home_len = strlen(home_dir); //sixe of home dir

    if (strcmp(cwd, home_dir) == 0) 
    {
        snprintf(out, outsize, "~"); // if prefix is home dir
        return;
    }

    if (strncmp(cwd, home_dir, home_len) == 0 && cwd[home_len] == '/') 
    {
        snprintf(out, outsize, "~%s", cwd + home_len); // write paths cwd+home dir
        return;
    }

    snprintf(out, outsize, "%s", cwd);
}

static void print_prompt(void) 
{
    char display_path[PATH_MAX];
    get_display_path(display_path, sizeof(display_path));
    //printing the display path prompt
    printf("<%s@%s:%s> ", username, hostname_buf, display_path);
    fflush(stdout);
}

static volatile sig_atomic_t prompt_interrupted=0;

static void sigint_handler(int sig) 
{
    (void)sig;
    prompt_interrupted=1;
}

static void sigtstp_handler(int sig) 
{
    (void)sig;
    prompt_interrupted=1;
}

int main(void) 
{
    char line[MAX_INPUT_LEN+1];
    static char accum_line[MAX_INPUT_LEN+1] = "";

    init_shell_identity();
    hop_init(home_dir);
    reveal_init(home_dir);
    jobs_init();
    
    struct sigaction sa_int;
    memset(&sa_int, 0, sizeof(sa_int));
    sa_int.sa_handler = sigint_handler;
    sigemptyset(&sa_int.sa_mask); //ensures nothing (signal) blocked when handler runs
    sa_int.sa_flags = 0;
    sigaction(SIGINT, &sa_int, NULL); //so ctrl c dsnt kill shell

    struct sigaction sa_tstp;
    memset(&sa_tstp, 0, sizeof(sa_tstp));
    sa_tstp.sa_handler = sigtstp_handler;
    sigemptyset(&sa_tstp.sa_mask);
    sa_tstp.sa_flags = 0;
    sigaction(SIGTSTP, &sa_tstp, NULL); //so ctrl z dsnt freexe shit

    //when shell launches a foreground process, the shell must temporarily move itself into the background
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN); //allows shell to regain conrol 

    int eof_warned = 0; //has user been warned about bg jobs when tryna exit

    while (1) {
        if (accum_line[0] == '\0') //shell ready for brand new command, not continuing a typed one
        {
            jobs_check_completed(); //clean bg jobs taht were fin
            print_prompt();
        }

        if (fgets(line, sizeof(line), stdin) == NULL) 
        {
            if (errno == EINTR) //was failure to read from shell due to a signal interrupt
            {
                if (prompt_interrupted) 
                {
                    printf("\n");
                    prompt_interrupted = 0;
                    accum_line[0] = '\0';
                }
                clearerr(stdin);
                continue;
            }

            if (accum_line[0] != '\0') //ctrl d but half wirteen line
            {
                if (!isatty(STDIN_FILENO) && feof(stdin)) 
                {
                    // In batch mode with no trailing newline, fall through to execute accum_line
                }
                else 
                {
                    // Requirement 9: Ctrl-D only counts as EOF on an empty line.
                    // If the line already has typed text, keep that text and stay alive.
                    clearerr(stdin);
                    continue;
                }
            }
            else 
            {
                if (jobs_has_stopped()) 
                {
                    if (!eof_warned) 
                    {
                        printf("cshell: there are stopped jobs\n");
                        fflush(stdout);
                        eof_warned = 1;
                        clearerr(stdin);
                        continue;
                    }
                }
                jobs_send_sighup_all();
                break;
            }
        }

        if (line[0] != '\0') 
        {
            size_t cur = strlen(accum_line);
            size_t add = strlen(line);
            if (cur + add < sizeof(accum_line)) 
            {
                strcat(accum_line, line);
            }
        }

        size_t total_len = strlen(accum_line);
        if (total_len == 0) 
        {
            continue;
        }

        if (accum_line[total_len - 1] != '\n' && isatty(STDIN_FILENO)) 
        {
            // Requirement 9: Ctrl-D only counts as EOF on an empty line.
            // If the line already has typed text, keep that text and stay alive.
            clearerr(stdin);
            continue;
        }

        if (accum_line[total_len - 1] == '\n') 
        {
            accum_line[total_len - 1] = '\0';
        }

        eof_warned = 0;

        int lex_error = 0;
        token_list_t *tokens = lex_line(accum_line, &lex_error); //lex the line first

        accum_line[0] = '\0';

        if (lex_error) 
        {
            printf("cshell: invalid syntax\n"); //if lex error invalid syntax
            continue;
        }

        int valid = parse_tokens(tokens); // step 2 is parsing

        if (!valid) 
        {
            printf("cshell: invalid syntax\n"); // if parse error , invalid syntax
            free_token_list(tokens);
            continue;
        }

        if (tokens->count > 0) 
        {
            execute_command_line(tokens);
        }

        free_token_list(tokens);
    }

    jobs_send_sighup_all();
    printf("\n");
    return 0;
}