#ifndef ML_H
#define ML_H

#define TOKEN_LEN 300

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
    OP_WORD,
    OP_EOF
}token_category;

typedef struct
{
    token_category type;
    char value[TOKEN_LEN];
}token_def;

int do_stuff(const char *line,int *cursor, token_def *token_type);

#endif 