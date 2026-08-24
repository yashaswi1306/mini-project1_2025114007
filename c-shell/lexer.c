#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"

#define INITIAL_WORD_CAP 32
#define INITIAL_TOKEN_CAP 8

static int is_special(char c) 
{
    return c=='<'||c=='>'||c == ';'||c=='|'||c =='&'; //not in word special char
}

static int is_space_char(char c) 
{
    return c==' '||c=='\t'||c=='\n';
}

//ai used to change the array code to vector (better for dynamic mem alloc)
typedef struct {
    token_t *items; //pointer to array
    size_t count; //no of items stored
    size_t cap; //capacity total
}token_vec_t;

//initialise the vec
static void tv_init(token_vec_t *v) 
{
    v->cap = INITIAL_TOKEN_CAP;
    v->count = 0;
    v->items = malloc(sizeof(token_t) * v->cap); //allocated memory to vec
    if (v->items == NULL) 
    {
        fprintf(stderr, "cshell: out of memory\n"); //mem not allocated properly
        exit(1);
    }
}

static void tv_push(token_vec_t *v, token_category type, char *text) {
    if (v->count == v->cap) 
    {
        v->cap *= 2; //if array full, double capacity
        token_t *grown = realloc(v->items, sizeof(token_t) * v->cap); //reallocate upon full memory
        if (grown == NULL) //not reallocated properly
        {
            fprintf(stderr, "cshell: out of memory\n");
            exit(1);
        }
        v->items = grown; //vec points to the new reallocated tokens
    }

    //store new value after old array ended , so after its len (count)
    v->items[v->count].type = type; 
    v->items[v->count].text = text;
    v->count++;
}

static void tv_free_contents(token_vec_t *v) 
{
    for (size_t i = 0; i < v->count; i++) 
    {
        free(v->items[i].text); //free individual tokens
    }
    free(v->items); //free artay
}

//use growable char buffer instead of array to bild word (again my code modified by ai to change it from arr)
typedef struct {
    char *data; //pointer to array
    int len; //len of char array
    int cap; //capacity of char array
} word_t;

//same as token mem allication function
static void word_init(word_t *b) 
{
    b->cap = INITIAL_WORD_CAP; 
    b->len = 0;
    b->data = malloc(b->cap);
    if (b->data == NULL) 
    {
        fprintf(stderr, "cshell: out of memory\n");
        exit(1);
    }
    b->data[0] = '\0'; // null terminator for end of word
}

static void word_push(word_t *b, char c) {
    if (b->len + 2 > b->cap) //+2 cux null terminator bhi hai naa (so new char + \0)
    { 
        b->cap *= 2;
        char *grown = realloc(b->data, b->cap);
        if (grown == NULL) {
            fprintf(stderr, "cshell: out of memory\n");
            exit(1);
        }
        b->data = grown;
    }
    b->data[b->len++] = c; //added char
    b->data[b->len] = '\0'; //null terminator
}


static char *lex_word(const char *line, size_t len, size_t *pos, int *error) 
{
    //tokenizationnnnnn 
    word_t word;
    word_init(&word); //mem alloc for word

    while (*pos<len) 
    {
        char c = line[*pos]; //read char at position
        if (is_space_char(c)||is_special(c)) 
        {
            break; //means that word has ended.
        }

        if (c=='\\') 
        {
            if (*pos+1>=len) 
            {
                //if lst char is escape, lex error
                *error=1;
                free(word.data);
                return NULL;
            }
            // otherwise ignore the /.
            word_push(&word, line[*pos + 1]);
            *pos += 2;
            continue;
        }

        if (c == '"') 
        { 
            //open double quotes
            (*pos)++;
            int closed = 0;
            while (*pos<len) {
                char dc=line[*pos];
                if (dc == '"') {
                    closed = 1; //find closing quotes
                    (*pos)++;
                    break;
                }
                if (dc == '\\') 
                {
                    if (*pos + 1 >= len) 
                    { // again if ended with /
                        *error = 1;
                        free(word.data);
                        return NULL;
                    }
                    // inside "", \" and \\ are special d_body
                    char nc = line[*pos + 1];

                    if (nc == '"' || nc == '\\') 
                    {
                        word_push(&word, nc); //push taht special char
                    } 
                    else 
                    {
                        word_push(&word, '\\'); // keep the / for chars like /n, /t etc.
                        word_push(&word, nc);
                    }
                    *pos += 2;
                    continue;
                }
                word_push(&word, dc);
                (*pos)++;
            }
            if (!closed)
            { //not closed
                *error = 1;
                free(word.data);
                return NULL;
            }
            continue;
        }

        if (c == '\'') 
        { //same logic as ""
            (*pos)++;
            int closed = 0;
            while (*pos < len) {
                char sc = line[*pos];
                if (sc == '\'') 
                {
                    closed = 1;
                    (*pos)++;
                    break;
                }
                word_push(&word, sc);
                (*pos)++;
            }
            if (!closed) \
            {
                *error = 1;
                free(word.data);
                return NULL;
            }
            continue;
        }

        /* ordinary character */
        word_push(&word, c);
        (*pos)++;
    }

    *error = 0; // if not break beofre, then no error
    return word.data;
}

token_list_t *lex_line(const char *line, int *error) //lexer for one inpput line
{
    token_vec_t vec; 
    tv_init(&vec);
    
    size_t len = strlen(line);
    
    size_t pos = 0;
    *error = 0;

    while (pos < len) 
    {
        char c = line[pos];

        if (is_space_char(c)) 
        {
            pos++; // move to next char 
            continue;
        }

        if (c == '|') 
        {
            tv_push(&vec, OP_PIPE, NULL); 
            //per char type, puh teh token categry as well as the text (null since | already shown with op_pipe)
            pos++;
            continue;
        }
        if (c == '&') 
        {
            tv_push(&vec, OP_AMP, NULL);
            pos++;
            continue;
        }
        if (c == ';') 
        {
            tv_push(&vec, OP_SEMI, NULL);
            pos++;
            continue;
        }
        if (c == '<') 
        {
            tv_push(&vec, OP_LT, NULL);
            pos++;
            continue;
        }
        if (c == '>') 
        {
            // if its >>, then by maximal munch
            if (pos + 1 < len && line[pos + 1] == '>') 
            {
                tv_push(&vec, OP_GTGT, NULL);
                pos += 2;
            } 
            else 
            {
                tv_push(&vec, OP_GT, NULL);
                pos++;
            }
            continue;
        }


        int w_error = 0;
        char *text = lex_word(line, len, &pos, &w_error); // otherwise uses lex word to deal with word lexing
        if (w_error) 
        {
            tv_free_contents(&vec); //free vector 
            *error = 1;
            return NULL;
        }
        tv_push(&vec, OP_WORD, text); 
    }

    token_list_t *list = malloc(sizeof(token_list_t)); //turn token_vec into the token_list
    
    if (list == NULL) 
    {
        fprintf(stderr, "cshell: out of memory\n");
        exit(1);
    }
    
    list->tokens = vec.items;
    list->count = vec.count;
    return list;
}

//frees token list
void free_token_list(token_list_t *list) {
    if (list == NULL) {
        return;
    }
    for (size_t i = 0; i < list->count; i++) {
        free(list->tokens[i].text);
    }
    free(list->tokens);
    free(list);
}
