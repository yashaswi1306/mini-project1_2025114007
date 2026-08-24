#ifndef EXECUTE_H
#define EXECUTE_H

#include "lexer.h"

void execute_cmd(const token_list_t *list);

void execute_pipeline(const token_list_t *list);

#endif