#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "resume.h"
#include "jobs.h"
#include <ctype.h>


static int is_all_digits(const char *str)  //is code jst digits
{
    if (!str||*str=='\0') 
    {
        return 0; // NULL string
    }
    for (size_t i=0; str[i]!='\0'; i++) 
    {
        if (!isdigit((unsigned char)str[i])) 
        {
            return 0; //false if even one is non digit
        }
    }
    return 1;
}

void resume_cmd(const token_list_t *list) 
{
    if (!list||list->count<3) //min 3 arguments needed
    {
        printf("resume: invalid syntax\n");
        return;
    }

    const char *target=list->tokens[1].text; //job identifier

    if (!target||target[0]!='%'||!is_all_digits(target+1)) // job identifier begins with %and is all dgits
    {
        printf("resume: invalid syntax\n");
        return;
    }

    int job_id=atoi(target+1); //str to int  (+1 to skip %)

    const char *mode=list->tokens[2].text; //get rewuested resume mode (bg or fg)
    
    if (!mode)  //check mode token exists yaa nahi
    {
        printf("resume: invalid syntax\n");
        return;
    }

    if (strcmp(mode,"bg")==0) //resume in bg
    {
        if (list->count!=3) //must have exactky 3 tokens
        {
            printf("resume: invalid syntax\n");
            return;
        }
        jobs_resume_bg(job_id); //resume job in bg
    } 
    else if (strcmp(mode,"fg")==0) //fg process
    {
        int timeout=0;
        if (list->count==3) 
        {
            timeout=0; //no timeout for 3 tokens
        } 
        else if (list->count==5) //means --timeout t given 
        {
            if (strcmp(list->tokens[3].text,"--timeout")!=0) 
            {
                printf("resume: invalid syntax\n"); // extra argument isnt timeout
                return;
            }
            const char *to_str=list->tokens[4].text; //get timeout value
            if (!is_all_digits(to_str)) 
            {
                printf("resume: invalid syntax\n"); //check timeout is all dgits no
                return;
            }

            timeout = atoi(to_str); //str to int

            if (timeout<=0) 
            {
                printf("resume: invalid syntax\n"); //invalid timeout
                return;
            }
        } 
        else 
        {
            printf("resume: invalid syntax\n"); //any no of tokens != 3 or 5 is incorrect
            return;
        }

        jobs_resume_fg(job_id, timeout); //resume job in fg
    } 
    else 
    {
        printf("resume: invalid syntax\n");
    }
}
