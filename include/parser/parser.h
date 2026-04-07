#ifndef PARSER_H
#define PARSER_H

#include "../core/shell.h"
#include "scanner.h"

struct node_s * parse_simple_command(struct token_s *tok);

#endif