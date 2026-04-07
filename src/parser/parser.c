#include <unistd.h>
#include "../../include/core/shell.h"
#include "../../include/parser/parser.h"
#include "../../include/parser/scanner.h"
#include "../../include/parser/node.h"
#include "../../include/core/source.h"

struct node_s *parse_simple_command(struct token_s *tok)
{
    if(!tok)
    {
        return NULL;
    }
    
    struct node_s *cmd = new_node(NODE_COMMAND);
    if(!cmd)
    {
        free_token(tok);
        return NULL;
    }
    
    struct source_s *src = tok->src;
    
    do
    {
        if(tok->text[0] == '\n')
        {
            free_token(tok);
            break;
        }

        struct node_s *word = new_node(NODE_VAR);
        if(!word)
        {
            free_node_tree(cmd);
            free_token(tok);
            return NULL;
        }
        set_node_val_str(word, tok->text);
        add_child_node(cmd, word);

        free_token(tok);

    } while((tok = tokenize(src)) != &eof_token);

    return cmd;
}
