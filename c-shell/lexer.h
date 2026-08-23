#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>

typedef enum {
    OP_PIPE, // same functions as in  my previous architecture (removed OP_EOF since it was useless)
    OP_AMP,
    OP_SEMI,
    OP_LT,
    OP_GT,
    OP_GTGT,
    OP_WORD
}token_category;

typedef struct {
    token_category type;
    char *text; //used char* instead of char tyoe[len] as thsi is better for dynamic mem alloc
}token_t;

typedef struct {
    token_t *tokens; // ai usage here: asked ai how to refine my present architecture. it said make a list of tokens that way u can jst use .count for len and indices for individual tokens.
    int count;
}token_list_t;

token_list_t* lex_line(const char *line, int *error); 

void free_token_list(token_list_t* list); // code is ai modified, always free ur pointers (cpro trauma :()

#endif
