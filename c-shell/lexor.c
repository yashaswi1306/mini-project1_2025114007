#include <stdio.h>

typedef enum {
    STATE_GENERAL,
    STATE_IN_WORD,
    STATE_IN_SINGLE_IC,
    STATE_IN_DOUBLE_IC,
    STATE_AFTER_ESCAPE,
    STATE_AFTER_GT,
    STATE_IN_DOUBLE_IC_ESCAPE
}state;

typedef enum{
    CHAR_SPECIAL,
    CHAR_QUOTE,
    CHAR_ESCAPE,
    CHAR_SPACE,
    CHAR_ORDINARY,
    CHAR_EOF
}character_category;

typedef enum{
    OP_PIPE,
    OP_AMP,
    OP_SEMI,
    OP_LT,
    OP_GT,
    OP_GTGT,
    WORD,

}token_category;

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

void do_stuff()
{
    character_category cat; //currebnt category

    state current_state=STATE_GENERAL;
    state next_state= STATE_GENERAL;

    char token[100];
    int token_length=0;

    do
    {
        int c=getchar();
        cat=translate(c); //char category

        int selector=1000*current_state+cat; // for the switc case laetr on (1000* so no collision (HASHIIINGGGG!!! yayyy trauma :( )))
        next_state=current_state;


        switch (selector)
        {
            // if start from general+word then stay in word
        case 1000*STATE_GENERAL+CHAR_ORDINARY:

            token_length=0; // since u have to start a new word
            // token_length++;
            token[token_length++]=c;

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

            if (c == '|')
            {
                next_state=STATE_AFTER_GT;
            }

            else if (c == '&')
            {
                next_state=STATE_AFTER_GT;
            }

            else if (c == ';')
            {
                next_state=STATE_AFTER_GT;
            }

            else if (c == '<')
            {
                next_state=STATE_AFTER_GT;
            }

            else if (c == '>')
            {
                next_state=STATE_AFTER_GT;
            }

            break;


        case 1000*STATE_GENERAL+ CHAR_EOF:

            next_state=STATE_GENERAL;
            break;

        case 1000*STATE_IN_WORD+CHAR_ORDINARY:

            if (token_length < 99)
                token[token_length++]=c;

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
            next_state=STATE_GENERAL;
            break;

// secial char also ends word so seperate processing for that
        case 1000*STATE_IN_WORD+CHAR_SPECIAL:

            token_length=0;
            ungetc(c, stdin);
            // so token can be processed again in state general
            next_state=STATE_GENERAL;
            break;


        case 1000*STATE_IN_WORD+CHAR_EOF:

            token_length=0;
            next_state=STATE_GENERAL;
            break;


        case 1000*STATE_AFTER_ESCAPE+CHAR_ORDINARY:
        case 1000*STATE_AFTER_ESCAPE+CHAR_SPACE:
        case 1000*STATE_AFTER_ESCAPE+CHAR_SPECIAL:
        case 1000*STATE_AFTER_ESCAPE+CHAR_QUOTE:
        case 1000*STATE_AFTER_ESCAPE+CHAR_ESCAPE:

            if (token_length < 99)
                token[token_length++]=c;

            next_state=STATE_IN_WORD;
            break;

//trailing escape
        case 1000*STATE_AFTER_ESCAPE+CHAR_EOF:

            printf("cshell:invalid syntax\n");
            next_state=STATE_GENERAL;
            break;


        case 1000*STATE_IN_SINGLE_IC+CHAR_QUOTE:

            if (c == '\'')
            {
                next_state=STATE_IN_WORD;
            }
            else
            {
             
                if (token_length < 99)
                    token[token_length++]=c;

                next_state=STATE_IN_SINGLE_IC;
            }
            break;

//sq body
        case 1000*STATE_IN_SINGLE_IC+CHAR_ORDINARY:
        case 1000*STATE_IN_SINGLE_IC+CHAR_SPACE:
        case 1000*STATE_IN_SINGLE_IC+CHAR_SPECIAL:
        case 1000*STATE_IN_SINGLE_IC+CHAR_ESCAPE:

            if (token_length < 99)
                token[token_length++]=c;

            next_state=STATE_IN_SINGLE_IC;

            break;


        case 1000*STATE_IN_SINGLE_IC+CHAR_EOF:
        
            printf("cshell:invalid syntax\n");
            next_state=STATE_GENERAL;
            break;


       
        case 1000*STATE_IN_DOUBLE_IC+CHAR_QUOTE:

            if (c == '"')
            {
                next_state=STATE_IN_WORD;
            }
            else
            {
             
                if (token_length < 99)
                    token[token_length++]=c;

                next_state=STATE_IN_DOUBLE_IC;
            }

            break;


     
        case 1000*STATE_IN_DOUBLE_IC+CHAR_ESCAPE:

            next_state=STATE_IN_DOUBLE_IC_ESCAPE;

            break;

        case 1000*STATE_IN_DOUBLE_IC+CHAR_ORDINARY:
        case 1000*STATE_IN_DOUBLE_IC+CHAR_SPACE:
        case 1000*STATE_IN_DOUBLE_IC+CHAR_SPECIAL:

            if (token_length < 99)
                token[token_length++]=c;
            next_state=STATE_IN_DOUBLE_IC;
            break;


   
        case 1000*STATE_IN_DOUBLE_IC+CHAR_EOF:

            printf("cshell:invalid syntax\n");
            next_state=STATE_GENERAL;
            break;


        case 1000*STATE_IN_DOUBLE_IC_ESCAPE+CHAR_ORDINARY:
        case 1000*STATE_IN_DOUBLE_IC_ESCAPE+CHAR_SPACE:
        case 1000*STATE_IN_DOUBLE_IC_ESCAPE+CHAR_SPECIAL:
        case 1000*STATE_IN_DOUBLE_IC_ESCAPE+CHAR_QUOTE:
        case 1000*STATE_IN_DOUBLE_IC_ESCAPE+CHAR_ESCAPE:

            if (token_length < 99)
                token[token_length++]=c;

            next_state=STATE_IN_DOUBLE_IC;
            break;

        case 1000*STATE_IN_DOUBLE_IC_ESCAPE+CHAR_EOF:

            printf("cshell:invalid syntax\n");

            next_state=STATE_GENERAL;
            break;


        case 1000*STATE_AFTER_GT+CHAR_SPECIAL:

            if (c == '>')
            {
                next_state=STATE_GENERAL;
            }
            else
            {
            
                ungetc(c, stdin);
                // so token can be rocessed again in state general
                next_state=STATE_GENERAL;
            }

            break;
   
        case 1000*STATE_AFTER_GT+CHAR_ORDINARY:
        case 1000*STATE_AFTER_GT+CHAR_SPACE:
        case 1000*STATE_AFTER_GT+CHAR_QUOTE:
        case 1000*STATE_AFTER_GT+CHAR_ESCAPE:


            ungetc(c, stdin);
            // so token can be rocessed again in state general
            next_state=STATE_GENERAL;
            break;


        case 1000*STATE_AFTER_GT+CHAR_EOF:

        next_state=STATE_GENERAL;
            break;

        default:

            printf("MIssed state\n");
            next_state=STATE_GENERAL;
            break;
        }

        current_state=next_state;

    }
    while (cat != CHAR_EOF);
}
