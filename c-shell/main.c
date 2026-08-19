#include <stdio.h>
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

char** split_line(char* in_line)
{
    int curr=0;
    int buffer_size=const_token_size;
    char** tokens_list=malloc(sizeof(char*)*buffer_size); //array of strings
    char* token;

    if (!tokens_list)
    {
        fprintf(stderr, "memory allocation error");
        exit(1);
    }

   
    token=lexor(in_line); //define kr lena baad mein pls :(

    while(token!=NULL)
    {
        tokens_list[curr]=token;
        curr++;

        if (curr>=buffer_size)
        {
            buffer_size+=const_token_size;
            tokens_list=realloc(tokens_list,sizeof(char*)*buffer_size);

            if (!tokens_list)
            {
                fprintf(stderr, "memory allocation error");
                exit(1);
            }
        }

        token=lexor(in_line);
    }

    tokens_list[curr]=NULL;
    return tokens_list;    
}

void input_loop()
{
    // int status;
    char* input_line;
    char** array_strings;

    printf(">");
    input_line=read_line();
    array_strings=split_line(input_line);
    // status=execute(array_strings);
    free(input_line);
    free(array_strings);

    // while(status) for user ne likha kya haiiiii!!! ek baar check project requirements 
    while(1)
    {
        printf(">");
        input_line=read_line();
        array_strings=split_line(input_line);
        // status=execute(array_strings);
        free(input_line);
        free(array_strings);
    }
}

int main()
{
    input_loop();
    return 0;
}