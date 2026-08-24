#ifndef REVEAL_H
#define REVEAL_H

#include "lexer.h"

void reveal_init(const char *home_dir);
void reveal(const token_list_t *list);

#endif
