#ifndef EXECUTE_H
#define EXECUTE_H

#include "lexer.h"

int execute_cmd(const token_list_t *list);
int execute_pipeline(const token_list_t *list);
int execute_pipeline_fg(const token_list_t *list, const char *full_cmd_str);
int execute_pipeline_bg(const token_list_t *list, int job_id, const char *full_cmd_str);
int execute_is_executable(const char *filepath);


#endif