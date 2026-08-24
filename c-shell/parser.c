#include "parser.h"

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
// typedef enum{
//     STATE_S, //start state
//     STATE_AFTER_WORD, // after a word
//     STATE_OPERATOR_AND_OR, // || &&
//     STATE_AFTER_PIPE, // > < >>
//     STATE_AFTER_SPEC // ; | ^(?)
// }state_parser;  too complicated hard to debug


typedef struct{
    const token_t *toks; //pointer to array
    size_t pos; //pos of tokens
    size_t n; // num of tokens
}parser_t;

static int at_end(const parser_t*p)//check uf nothing left ot parse
{
    return p->pos>=p->n;
}


static token_category token_type_parser(const parser_t *p) 
{
    return p->toks[p->pos].type;
}

static int ARG(parser_t *p);
static int CMD(parser_t *p);
static int TGT(parser_t *p);
static int BG(parser_t *p);

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
static int CMD(parser_t *p) 
{
    if (at_end(p)||token_type_parser(p)!=OP_WORD) 
    {
        return 0; //check if cmdgoes to word or not
    }
    p->pos++;
    return ARG(p); //after word parsed, it has to parse arg next
}

static int TGT(parser_t *p) 
{
    if (at_end(p)||token_type_parser(p)!=OP_WORD) {
        return 0;
    }
    p->pos++;
    return ARG(p);
}


static int BG(parser_t *p) 
{
    if (at_end(p)) 
    {
        return 1;
    }
    return CMD(p); //goes to word end or goes t word arg which ius same as cmd
}

static int ARG(parser_t *p) 
{
    if (at_end(p)) {
        return 1;
    }
    switch (token_type_parser(p)) {
        case OP_WORD:
            p->pos++; 
            return ARG(p);
        case OP_LT:
            p->pos++;
            return TGT(p);
        case OP_GT:
            p->pos++;
            return TGT(p);
        case OP_GTGT:
            p->pos++;
            return TGT(p);
        case OP_PIPE:
            p->pos++;
            return CMD(p);
        case OP_SEMI:
            p->pos++;
            return CMD(p);
        case OP_AMP:
            p->pos++;
            return BG(p);
        default:
            return 0;
    }
}

static int LINE(parser_t *p) 
{
    if (at_end(p)) 
    {
        return 1;
    }
    if (token_type_parser(p) != OP_WORD) 
    {
        return 0;
    }
    p->pos++;
    return ARG(p);
}

int parse_tokens(const token_list_t *list) {
    parser_t p = { .toks = list->tokens, .n = list->count, .pos = 0 };
    return LINE(&p);
}
