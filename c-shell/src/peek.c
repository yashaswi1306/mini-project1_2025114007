#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "peek.h"

#define CHUNK 4096

typedef struct{
    char **items;
    size_t count;
    size_t cap;
}lines_t;

void lines_init(lines_t *l) 
{
    l->cap=64; 
    l->count=0;
    l->items=malloc(sizeof(char *)*l->cap); //basic initialization for the lines_t struct 
}

void lines_push(lines_t *l, char *s) 
{
    if (l->count==l->cap) 
    {
        l->cap *= 2;
        l->items = realloc(l->items, sizeof(char *)*l->cap); //realloc if mem limit reached
    }
    l->items[l->count++] = s; //else jst add to nxt free position
}

void lines_free(lines_t *l) 
{
    for (size_t i=0; i<l->count; i++) 
    {
        free(l->items[i]); //free all ur pointers
    }
    free(l->items);
}

void read_lines(FILE *fp, lines_t*la) {
    char *line=NULL;
    size_t cap=0;
    ssize_t len;

    int is_terminal=0;

    if((fp==stdin)&&(isatty(STDIN_FILENO))) //isatty checks if open file desciption refers to terminal or something else
    {
        is_terminal=1;
    }
    while ((len=getline(&line, &cap, fp))!=-1)   // not reached eof
    {
        if (len>0&&line[len-1]=='\n') 
        {
            line[len-1]='\0';
        }

        lines_push(la,strdup(line));
        if (is_terminal) 
        {
            break; // run only once for teminal. Without thsi it was running on terminal till ctrl c
        }
    }
    free(line);
}

void read_lines_backward(int fd, lines_t *la) 
{
    off_t fsize=lseek(fd, 0, SEEK_END); //gets end of file (int giving issues in full code idk why :()

    if (fsize<=0) 
    {
        return;
    }

    char word[CHUNK];
    char *lo = NULL;   // \n in current chunk (left over)
    size_t lo_len=0;
    off_t pos=fsize;

    while (pos>0) 
    {
        ssize_t toread;// kitte bytes read krne hai?

        if((off_t)CHUNK<pos)
        {
            toread=CHUNK; // if more than chunk bytes remain, read those
        }
        else
        {
            toread=(ssize_t)pos; //else jst read the chunk bytes
        }

        pos -= toread; //pointer shld move that many bytes back. So if its 4 bytes in like 100, the pointer shld move back so it can read 97 98 99 100 whilst forwrs (since lines reversed NOT chunks)

        lseek(fd,pos,SEEK_SET); // move file cursor to pos

        ssize_t n = read(fd,word,toread); // read all the to read bytes. N tells hpw many btes read
        
        if (n<=0) 
        {
            break; //means there was prolly an issue. Fix that
        }

        ssize_t end=n; // so n is part of word NOT read yet

        for (ssize_t i=n-1; i>=0; i--) 
        {
            if (word[i]=='\n') //if new line found
            {
                ssize_t seg=end-(i+1); //calculates hars left after \n

                size_t total=(size_t)seg+lo_len; //seg is current chunk, lo is left over from prev chunk

                char *line = malloc(total+1);
                if (seg>0) 
                {
                    memcpy(line,word+i+1, seg);
                }
                if (lo_len>0) 
                {
                    memcpy(line+seg, lo, lo_len); // if leftover part from prev chunk, apend it to another
                }
                line[total]='\0'; // null terminator
                lines_push(la,line); //push

                free(lo);
                lo=NULL; //hence lo is NULL since no leftover chars
                lo_len=0; //no leftover chars
                end=i;
            }
        }

        if (end>0) 
        {
            size_t nl=(size_t)end+lo_len; //mem alloc curr leftover + prev leftover

            char *nlo = malloc(nl+1);
            memcpy(nlo,word,end);

            if (lo_len>0) // if stuff already leftover from prev chunk
            {
                memcpy(nlo+end, lo,lo_len);
            }
            
            nlo[nl]='\0';
            free(lo);  //memory free
            lo=nlo; //lo oits to new combined string
            lo_len=nl; //leftover chars in lo
        }
    }
    if (lo!=NULL) 
    { 
        lines_push(la,lo); //la now has reverse lines
    }
}


int process_source(const char *filename, int r_flag, int n_flag,int *running_num) 
{
    int use_stdin =0;
    if (filename==NULL||strcmp(filename, "-")==0)
    {
        use_stdin=1;
    }

    if (!use_stdin) 
    {
        struct stat st; //stat stores file info 
        if (stat(filename, &st) != 0) 
        {
            printf("peek: no such file or directory\n"); // if file dsnt exist
            return -1;
        }
        if (S_ISDIR(st.st_mode)) 
        {
            printf("peek: is a directory\n"); // if file is a dir
            return -1;
        }
    }

    if (use_stdin) 
    {
        if (!r_flag)
        {
            // if not rev flag
            char *line=NULL;
            size_t cap=0;
            ssize_t len;

            int is_terminal=isatty(STDIN_FILENO); // check if connected directly to teh terminal

            while ((len=getline(&line, &cap, stdin))!=-1)// till not eof, get line reurns num of chars read
            {
                if (len>0&&line[len-1]=='\n') 
                {
                    line[len-1]='\0'; // remove the new line
                }
                if (line[0]=='\0') 
                {
                    printf("\n"); // empty line
                } 
                else if(n_flag)// n_flag used
                {
                    printf("%d %s\n", ++(*running_num), line); // prin nums
                } 
                else
                {
                    printf("%s\n", line); // no -n
                }

                fflush(stdout);
                if (is_terminal) 
                {
                    break; // run only once for terminal
                }
            }
            free(line);
            return 0;
        }

        // dynamic array of lines
        lines_t la; 
        lines_init(&la);
        read_lines(stdin, &la); // rad from stdin

        if (!n_flag) {
            // n_flag used
            for (size_t i=la.count; i>0; i--)
            {
                printf("%s\n", la.items[i-1]);
            }
        } 
        else 
        {
            // -r and -n
            int *nums = calloc(la.count, sizeof(int)); // give int to every idx

            for (size_t i=0; i<la.count; i++)
            {
                if (la.items[i][0] != '\0') nums[i] = ++(*running_num);
            }

            for (size_t i=la.count; i>0; i--) // print backwards
            {
                size_t idx=i-1; // i is array idx
                if (la.items[idx][0]=='\0') // empty line
                {
                    printf("\n");
                }
                else 
                {
                    printf("%d %s\n", nums[idx], la.items[idx]);
                }
            }
            free(nums);
        }
        lines_free(&la);
        return 0;
    }

    if (r_flag && !n_flag) {

        int fd = open(filename, O_RDONLY);
        if (fd < 0) 
        { 
            printf("peek: no such file or directory\n"); // failed to open file
            return -1; 
        }
        lines_t la; 
        lines_init(&la);

        read_lines_backward(fd, &la);  // resd backwrads
        close(fd);

        for (size_t i=0; i<la.count; i++)
        {
            printf("%s\n", la.items[i]); //since lines already reversed, jst print them normally
        }

        lines_free(&la);
        return 0;
    }


    // regular file with no flags
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) 
    { 
        printf("peek: no such file or directory\n");
        return -1; 
    }


    lines_t la; 
    lines_init(&la);
    read_lines(fp, &la); // read fle forard into la
    fclose(fp);

    if (r_flag && n_flag) 
    {
        int *nums=calloc(la.count, sizeof(int));

        for (size_t i=0; i<la.count;i++)
        {
            if (la.items[i][0]!='\0') 
            {
                nums[i]=++(*running_num);
            }
        }

        for (size_t i=la.count; i>0; i--) 
        {
            size_t idx = i-1;
            if (la.items[idx][0]=='\0') 
            {
                printf("\n");
            }
            else 
            {
                printf("%d %s\n", nums[idx], la.items[idx]);
            }
        }
        free(nums);
    } 
    else if(n_flag) 
    {
        for (size_t i=0; i<la.count; i++) 
        {
            if (la.items[i][0]=='\0')
            {
                printf("\n");
            }
            else 
            {
                printf("%d %s\n", ++(*running_num), la.items[i]);
            }
        }
    } 
    else 
    {
        for (size_t i=0; i<la.count; i++)
        {
            printf("%s\n", la.items[i]);
        }
    }
    lines_free(&la);
    return 0;
}



void peek(const token_list_t *list) 
{
    int n_flag=0;
    int r_flag=0; //flag values

    //sice peek can have multiple args, collect them all
    const char *files[256];
    int file_count=0;

    // so gothrough every peek arg after peek
    for (size_t i=1;i<list->count; i++) 
    {
        // If NOT a word, Ignore it
        if (list->tokens[i].type!=OP_WORD) 
        {
            continue;
        }

        // get string stored in text
        const char *arg=list->tokens[i].text;

        if (arg[0]=='-'&&arg[1]!='\0') // so strat char is - and then next isnt \0 so its -. not -\0
        {

            for (int j=1; arg[j]!='\0'; j++) 
            {
                if (arg[j]=='n')
                {
                    n_flag=1; // nflag is used
                }
                else if (arg[j]=='r') 
                {
                    r_flag=1; // r flag is used 
                }
                else 
                { 
                    printf("peek: incorrect flag\n"); 
                    return; 
                }
            }
        } 
        else 
        {
            if (file_count < 256) 
            {
                files[file_count++]=arg; // if not a flag then its a file 
            }
        }
    }

    int running_num = 0; // keeps track of file nums

    if (file_count == 0) 
    {
        // if no files; read from stdin
        process_source(NULL, r_flag, n_flag, &running_num);
    } 
    else 
    {
        for (int i = 0; i < file_count; i++)
        {
            process_source(files[i], r_flag, n_flag, &running_num); //else process each file
        }
    }
}


