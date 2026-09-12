#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ping.h"
#include "jobs.h"
#include <ctype.h>

static int is_non_negative_integer(const char *str) //same function reused AGAIN
{
    if (!str||*str == '\0') 
    {
        return 0;
    }
    for (size_t i=0; str[i]!='\0'; i++) 
    {
        if (!isdigit((unsigned char)str[i])) 
        {
            return 0;
        }
    }
    return 1;
}

void ping_cmd(const token_list_t *list) 
{
    if (!list||list->count!=3) //incorrect args number 
    {
        printf("ping: invalid syntax\n");
        return;
    }

    const char *target=list->tokens[1].text; //get target process or job
    const char *sig_str=list->tokens[2].text; // get signal num in str

    // Signal num is only positive no
    if (!is_non_negative_integer(sig_str)) 
    {
        printf("ping: invalid syntax\n");
        return;
    }

    int raw_sig=atoi(sig_str); //siganl number str to int
    int actual_sig=raw_sig%64; //do signal nuumber modulo 64

    if (target[0]=='%')  //if target is a job
    {
        if (!is_non_negative_integer(target+1)) // taret+1 so that if its %n, it returns n 
        {
            printf("ping: invalid syntax\n");
            return;
        }

        int job_id=atoi(target+1); //str to int

        if (!jobs_send_signal_to_job(job_id,actual_sig)) // if job doesnt exist
        {
            printf("ping: no such process found\n");
            return;
        }
        printf("Sent signal %d to %s\n",raw_sig,target); //signal successfully ent (print)
        fflush(stdout);
    } 
    else //target is pid
    {
        if (!is_non_negative_integer(target)) //invalid pid
        {
            printf("ping: invalid syntax\n");
            return;
        }
        pid_t pid=(pid_t)atoi(target); //str to int
        if (!jobs_send_signal_to_pid(pid, actual_sig)) 
        {
            printf("ping: no such process found\n"); //send signal directly to specified process and error if it dsnt exist
            return;
        }
        printf("Sent signal %d to %s\n",raw_sig,target); //print that signal successfully sent
        fflush(stdout);
    }
}
