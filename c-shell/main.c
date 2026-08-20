#include <stdio.h>
#include <stdlib.h>
#include "ml.h"

#define const_buffer_size 1024
#define const_token_size 64

char* read_line()
{
    int curr=0; 
    int char_read; // int cuz char cant store -1 for EOF from getchar()
    int buffer_size=const_buffer_size;
    char* in_string=malloc(sizeof(char)*buffer_size);

    if (!in_string)
    {
        fprintf(stderr, "memory allocation error");
        exit(1);
    }

    
    while (1)
    {
        char_read=getchar();

        if (char_read==EOF||char_read=='\n')
        {
            in_string[curr]='\0';
            return in_string;
        }
        else
        {
            in_string[curr]=char_read;
        }
        curr++;

        if (curr>=buffer_size)
        {
            buffer_size+=const_buffer_size;
            in_string=realloc(in_string,sizeof(char)*buffer_size);

            if (!in_string)
            {
                fprintf(stderr, "memory allocation error");
                exit(1);
            }
        }
    }

}

token_def* split_line(char* in_line)
{
    int curr=0;
    int buffer_size=const_token_size;
    
    token_def* tokens_list=malloc(sizeof(token_def)*buffer_size); 
    // char* token;
    int pos=0;


    if (!tokens_list)
    {
        fprintf(stderr, "memory allocation error");
        exit(1);
    }
 
    // token=lexor(in_line); //define kr lena baad mein pls :(

    // while(token!=NULL)
    // {
    //     tokens_list[curr]=token;
    //     curr++;

    //     if (curr>=buffer_size)
    //     {
    //         buffer_size+=const_token_size;
    //         tokens_list=realloc(tokens_list,sizeof(char*)*buffer_size);

    //         if (!tokens_list)
    //         {
    //             fprintf(stderr, "memory allocation error");
    //             exit(1);
    //         }
    //     }

    //     token=lexor(in_line);
    // }

    // tokens_list[curr]=NULL;
    // return tokens_list;  
    
    
    // Now this dsnt work with MY lexer function. So to fix:

    while(1)
    {
        token_def tok_lex;

        int flag=do_stuff(in_line,&pos,&tok_lex); //since lexer gives one token at once
        
        if (flag==0) //EOF
        {
            break;
        }

        if (flag==2) //lex err
        {
            free(tokens_list);
            return NULL; //token ka kya hi karoge jab err aa rha hai
        }

        tokens_list[curr]=tok_lex;
        curr++;


        if (curr>=buffer_size-1) //op_eof bhi toh space lega
        {
            buffer_size+=const_token_size;
            tokens_list=realloc(tokens_list,sizeof(token_def*)*buffer_size);

            if (!tokens_list)
            {
                fprintf(stderr, "memory allocation error");
                exit(1);
            }
        }
    }

    tokens_list[curr].type=OP_EOF;
    tokens_list[curr].value[0]='\0'; // mark eot (eof misleading naam hai bt ab lite :()
    return tokens_list;    
}

void input_loop()
{
    // int status;
    char* input_line;
    token_def* array_strings;

    // printf(">");
    // input_line=read_line();
    // array_strings=split_line(input_line);
    // free(input_line);
    // free(array_strings);

    // while(status) for user ne likha kya haiiiii!!! ek baar check project requirements 
    while(1)
    {
        printf(">");
        input_line=read_line();
        array_strings=split_line(input_line);

        //lex err
        if (array_strings==NULL)
        {
            free(input_line);
            continue;
        }
        
        // if (!some_parser_func(array_strings))
        // {
        //     printf("cshell:invalid syntax\n");
        //     free(input_line);
        //     free(array_strings);
        //     continue
        // }


        free(input_line);
        free(array_strings);
    }
}

int main()
{
    input_loop();
    return 0;
}