// #include <stdio.h>
// #include <stdlib.h>

// #include <string.h>
#include "part_a.h"

character_category translate(int c)
{
    switch(c)
    {
        case '|': case '&': case '>': case '<': case ';': return CHAR_SPECIAL;

        case '"': case '\'': return CHAR_QUOTE;

        case '\\': return CHAR_ESCAPE;

        case ' ': case '\t': case '\n': case '\r': return CHAR_SPACE;

        case EOF: return CHAR_EOF;

        default: return CHAR_ORDINARY;
    }
}

int do_stuff(const char *line,int *cursor, token_def *token_type)
{
    character_category cat; //currebnt category

    state current_state=STATE_GENERAL;
    state next_state= STATE_GENERAL;

    int token_length=0;
    char token[TOKEN_LEN];

    do
    {
        int c= line[*cursor];
        (*cursor)++;

        if (c=='\0')
        {
            c=EOF;
        }
        else
        {
            c=(unsigned char)c; //read c from input line cuz ur not reading from stdin 
        }

        cat=translate(c); //char category

        int selector=1000*current_state+cat; // for the switc case laetr on (1000* so no collision (HASHIIINGGGG!!! yayyy trauma :( )))
        next_state=current_state;


        switch (selector)
        {
            // if start from general+word then stay in word
        case 1000*STATE_GENERAL+CHAR_ORDINARY:

            token_length=0; // since u have to start a new word
            // token_length++;
            if (token_length<TOKEN_LEN-1)
            {
                token[token_length++]=c;
            }
            else
            {
                printf("cshell:invalid syntax\n");
                return 2; // 2 for invalid in main
            }
            next_state=STATE_IN_WORD;
            break;


        case 1000*STATE_GENERAL+CHAR_SPACE:

            next_state=STATE_GENERAL; //ignore for extra whitespace
            break;


        case 1000*STATE_GENERAL+CHAR_ESCAPE:

            token_length=0;
            next_state=STATE_AFTER_ESCAPE; //to keep flag for state after escape
            break;


        case 1000*STATE_GENERAL+CHAR_QUOTE:

            if (c == '"')
            {
                token_length=0;
                next_state=STATE_IN_DOUBLE_IC;
            }
            else
            {
                token_length=0;
                next_state=STATE_IN_SINGLE_IC;
            }

            break;


        case 1000*STATE_GENERAL+CHAR_SPECIAL:

            if (c == '>')
            {
                next_state=STATE_AFTER_GT;
                break; // cuz is it >> or > (so u need that state to check )
            }

            if (c == '|')
            {
                token_type->type=OP_PIPE;
                token_type->value[0]='|';   
                token_type->value[1]='\0';   
            }

            else if (c == '&')
            {
                token_type->type=OP_AMP;
                token_type->value[0]='&';   
                token_type->value[1]='\0';   
            }

            else if (c == ';')
            {
                token_type->type=OP_SEMI;
                token_type->value[0]=';';   
                token_type->value[1]='\0';   
            }

            else if (c == '<')
            {
                token_type->type=OP_LT;
                token_type->value[0]='<';   
                token_type->value[1]='\0';   
            }


            return 1; //special token mil gya wohoooooo 


        case 1000*STATE_GENERAL+ CHAR_EOF:

            next_state=STATE_GENERAL;
            return 0; //ho gya input line finish

        case 1000*STATE_IN_WORD+CHAR_ORDINARY:

            if (token_length < TOKEN_LEN-1)
                token[token_length++]=c;
            else
            {
                printf("cshell: invalid syntax\n");
                return 2;
            }

            next_state=STATE_IN_WORD;
            break;


        case 1000*STATE_IN_WORD+CHAR_ESCAPE:

            next_state=STATE_AFTER_ESCAPE;
            break;


        case 1000*STATE_IN_WORD+CHAR_QUOTE:

            if (c == '"')
                next_state=STATE_IN_DOUBLE_IC;
            else
                next_state=STATE_IN_SINGLE_IC;

            break;


        case 1000*STATE_IN_WORD+CHAR_SPACE:

            token_length=0;
            token[token_length]='\0'; // terminate that word
            token_type->type=OP_WORD;
            strcpy(token_type->value,token); // cuz poora word copy krna hoga naa

            return 1; // word mil gya wohhhhoo

// secial char also ends word so seperate processing for that
        case 1000*STATE_IN_WORD+CHAR_SPECIAL:

            // so token belongs to NEXT CHAR thats why hello>>world toh >> is NEXT token

            (*cursor)--; 
            // so token can be processed again in state general
            token[token_length]='\0'; // terminate that word
            token_type->type=OP_WORD;
            strcpy(token_type->value,token); // cuz poora word copy krna hoga naa
            return 1; // word mil gya wohhhhoo


        case 1000*STATE_IN_WORD+CHAR_EOF:

            token[token_length]='\0'; // terminate that word
            token_type->type=OP_WORD;
            strcpy(token_type->value,token); // cuz poora word copy krna hoga naa

            return 1; // word mil gya wohhhhoo



        case 1000*STATE_AFTER_ESCAPE+CHAR_ORDINARY:
        case 1000*STATE_AFTER_ESCAPE+CHAR_SPACE:
        case 1000*STATE_AFTER_ESCAPE+CHAR_SPECIAL:
        case 1000*STATE_AFTER_ESCAPE+CHAR_QUOTE:
        case 1000*STATE_AFTER_ESCAPE+CHAR_ESCAPE:

            if (token_length < TOKEN_LEN-1)
                token[token_length++]=c;
            else
            {
                printf("cshell: invalid syntax\n");
                return 2;
            }
            next_state=STATE_IN_WORD;
            break;

//trailing escape
        case 1000*STATE_AFTER_ESCAPE+CHAR_EOF:

            printf("cshell:invalid syntax\n");
            next_state=STATE_GENERAL;
            return 2;
            // break;


        case 1000*STATE_IN_SINGLE_IC+CHAR_QUOTE:

            if (c == '\'')
            {
                next_state=STATE_IN_WORD;
            }
            else
            {
                if (token_length < TOKEN_LEN-1)
                    token[token_length++]=c;
                else
                {
                    printf("cshell: invalid syntax\n");
                    return 2;
                }

                next_state=STATE_IN_SINGLE_IC;
            }
            break;

//sq body
        case 1000*STATE_IN_SINGLE_IC+CHAR_ORDINARY:
        case 1000*STATE_IN_SINGLE_IC+CHAR_SPACE:
        case 1000*STATE_IN_SINGLE_IC+CHAR_SPECIAL:
        case 1000*STATE_IN_SINGLE_IC+CHAR_ESCAPE:

            if (token_length < TOKEN_LEN-1)
                token[token_length++]=c;
            else
            {
                printf("cshell: invalid syntax\n");
                return 2;
            }

            next_state=STATE_IN_SINGLE_IC;

            break;


        case 1000*STATE_IN_SINGLE_IC+CHAR_EOF:
        
            printf("cshell:invalid syntax\n");
            return 2;
            // next_state=STATE_GENERAL;
            // break;
       
        case 1000*STATE_IN_DOUBLE_IC+CHAR_QUOTE:

            if (c == '"')
            {
                next_state=STATE_IN_WORD;
                
            }
            else
            {
             
                if (token_length < TOKEN_LEN-1)
                    token[token_length++]=c;
                else
                {
                    printf("cshell: invalid syntax\n");
                    return 2;
                }

                next_state=STATE_IN_DOUBLE_IC;
            }

            break;


     
        case 1000*STATE_IN_DOUBLE_IC+CHAR_ESCAPE:
            next_state=STATE_IN_DOUBLE_IC_ESCAPE;
            break;

        case 1000*STATE_IN_DOUBLE_IC+CHAR_ORDINARY:
        case 1000*STATE_IN_DOUBLE_IC+CHAR_SPACE:
        case 1000*STATE_IN_DOUBLE_IC+CHAR_SPECIAL:

            if (token_length < TOKEN_LEN-1)
            {
                token[token_length++]=c;
            }
            else
            {
                printf("cshell: invalid syntax\n");
                return 2;
            }

            next_state=STATE_IN_DOUBLE_IC;
            break;


   
        case 1000*STATE_IN_DOUBLE_IC+CHAR_EOF:

            printf("cshell:invalid syntax\n");
            return 2;
            // next_state=STATE_GENERAL;
            // break;


        case 1000*STATE_IN_DOUBLE_IC_ESCAPE+CHAR_ORDINARY:
        case 1000*STATE_IN_DOUBLE_IC_ESCAPE+CHAR_SPACE:
        case 1000*STATE_IN_DOUBLE_IC_ESCAPE+CHAR_SPECIAL:
        case 1000*STATE_IN_DOUBLE_IC_ESCAPE+CHAR_QUOTE:
        case 1000*STATE_IN_DOUBLE_IC_ESCAPE+CHAR_ESCAPE:

            if (c=='"'|| c=='\\')
            {
                if (token_length < TOKEN_LEN-1)
                {
                    token[token_length++]=c;
                }
                else
                {
                    printf("cshell: invalid syntax\n");
                    return 2;
                }
            }
            else
            {
                if (token_length < TOKEN_LEN-2)
                {
                    token[token_length++]='\\';
                    token[token_length++]=c;
                }
                else
                {
                    printf("cshell: invalid syntax\n");
                    return 2;
                }
            }

            next_state=STATE_IN_DOUBLE_IC;
            break;

        case 1000*STATE_IN_DOUBLE_IC_ESCAPE+CHAR_EOF:

            printf("cshell:invalid syntax\n");
            return 2;
            // next_state=STATE_GENERAL;
            // break


        case 1000*STATE_AFTER_GT+CHAR_SPECIAL:

            if (c == '>')
            {
                // cuz >> has more priority than > so it as to be >> not > > 
                token_type->type=OP_GTGT;
                token_type->value[0]='>';   
                token_type->value[1]='>';   
                token_type->value[2]='\0';   
                return 1;
            }
            else
            {
                (*cursor)--; 
                // so token can be processed again in state general
                token_type->type=OP_GT;
                token_type->value[0]='>';   
                token_type->value[1]='\0';   
                return 1;
            }

            break;
   
        case 1000*STATE_AFTER_GT+CHAR_ORDINARY:
        case 1000*STATE_AFTER_GT+CHAR_SPACE:
        case 1000*STATE_AFTER_GT+CHAR_QUOTE:
        case 1000*STATE_AFTER_GT+CHAR_ESCAPE:


            (*cursor)--; 
            // so token can be processed again in state general
            
            token_type->type=OP_GT;
            token_type->value[0]='>';   
            token_type->value[1]='\0';   

            return 1;
            // next_state=STATE_GENERAL;
            // break;


        case 1000*STATE_AFTER_GT+CHAR_EOF:

            token_type->type=OP_GT;
            token_type->value[0]='>';   
            token_type->value[1]='\0';   
            
            return 1;

        default:

            printf("cshell:invalid syntax\n");
            return 2;
            // next_state=STATE_GENERAL;
            // break;
        }

        current_state=next_state;

    }
    while (cat != CHAR_EOF);

    return 0;

}
