#include "part_a.h"

// LINE  ->  ε
//       |   WORD ARG

// ARG   ->  ε
//       |   WORD    ARG
//       |   OP_LT   TGT
//       |   OP_GT   TGT
//       |   OP_GTGT TGT
//       |   OP_PIPE CMD
//       |   OP_SEMI CMD
//       |   OP_AMP  BG

// CMD   ->  WORD ARG

// TGT   ->  WORD ARG

// BG    ->  ε
//       |   WORD ARG
typedef enum{
    STATE_S, //start state
    STATE_AFTER_WORD, // after a word
    STATE_OPERATOR_AND_OR, // || &&
    STATE_AFTER_PIPE, // > < >>
    STATE_AFTER_SPEC // ; | ^(?)
}state_parser; 

int parse(const token_def *tokens)
{
    state_parser curr_state= STATE_S;

    for(int i = 0; tokens[i].type != OP_EOF; i++)
    {
        switch(curr_state)
        {
            case STATE_S:
                if(tokens[i].type == OP_WORD) 
                {
                    curr_state = STATE_AFTER_WORD;
                }
                else return 0;
                break;

            case STATE_AFTER_WORD:
                if(tokens[i].type == OP_WORD) 
                {
                    curr_state = STATE_AFTER_WORD;
                }

                else if(tokens[i].type == OP_LT) 
                {
                    curr_state = STATE_AFTER_PIPE;
                }

                else if(tokens[i].type == OP_GT) 
                {
                    curr_state = STATE_AFTER_PIPE;
                }
                else if(tokens[i].type == OP_GTGT) 
                {
                    curr_state = STATE_AFTER_PIPE;
                }
                
                else if(tokens[i].type == OP_PIPE) 
                {
                    curr_state = STATE_AFTER_SPEC;
                }

                else if(tokens[i].type == OP_SEMI) 
                {
                    curr_state = STATE_AFTER_SPEC;
                }

                else if(tokens[i].type == OP_AMP) 
                {
                    curr_state = STATE_OPERATOR_AND_OR;
                }
                else return 0;
                break;

            case STATE_AFTER_PIPE:
            case STATE_AFTER_SPEC:
                if(tokens[i].type == OP_WORD) 
                {
                    curr_state = STATE_AFTER_WORD;
                }
                else return 0;
                break;

            case STATE_OPERATOR_AND_OR:
                if(tokens[i].type == OP_WORD) 
                {
                    curr_state = STATE_AFTER_WORD;
                }
                else return 0;
                break;
        }
    }

    if (curr_state == STATE_S || curr_state == STATE_OPERATOR_AND_OR || curr_state == STATE_AFTER_WORD) 
    {
        return 1;
    }
    return 0;
}
