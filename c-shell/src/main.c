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

int main(void) 
{
    char line[MAX_INPUT_LEN + 1];

    init_shell_identity();
    hop_init(home_dir);
    reveal_init(home_dir);

    print_prompt();
    while (fgets(line, sizeof(line), stdin) != NULL) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') 
        {
            line[len - 1] = '\0';
        }

        int lex_error = 0;
        token_list_t *tokens = lex_line(line, &lex_error); //lex the line first

        if (lex_error) 
        {
            printf("cshell: invalid syntax\n"); //if lex error invalid syntax
            print_prompt();
            continue;
        }

        int valid = parse_tokens(tokens); // step 2 is parsing

        if (!valid) 
        {
            printf("cshell: invalid syntax\n"); // if parse error , invalid syntax
            free_token_list(tokens);
            print_prompt();
            continue;
        }

    
        for (size_t i = 0; i < tokens->count; i++) 
        {
            if (tokens->tokens[i].type == OP_SEMI || tokens->tokens[i].type == OP_AMP) 
            {
                tokens->count = i; //trucate tokens at ; or & so that u have 1st command grp
                //so for echo hi; echo hello, only echo hi is seen
                break;
            }
        }

        
        if (tokens->count > 0 && tokens->tokens[0].type == OP_WORD) {
            execute_pipeline(tokens); //pipeline execution
        }

        free_token_list(tokens);

        print_prompt();
    }

    printf("\n");
    return 0;
}
