#include <stdio.h>
#include <stdlib.h>

#include "mpc.h"

/* Win support */
#ifdef _WIN32

static char buffer[2048];

/* Fake readline */
char* readline(char *prompt)
{
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char *cpy = malloc(strlen(buffer) + 1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy)-1] = '\0';
  return cpy;
}

/* Fake add_history */
void add_history(char *unused) {}

/* Add Linux readline suuport */
#else
#include <readline/readline.h>
#include <readline/history.h>
#endif

int main(int argc, char** argv)
{
  /* Create parsers */
  mpc_parser_t *Number    = mpc_new("number");
  mpc_parser_t *Operator  = mpc_new("operator");
  mpc_parser_t *Expr      = mpc_new("expr");
  mpc_parser_t *Lezchty   = mpc_new("lezchty");

  /* Define language parser */
  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                       \
      number    :  /-?[0-9]+/ ;                             \
      operator  :  '+' | '-' | '*' | '/' ;                  \
      expr      : <number> | '(' <operator> <expr>+ ')' ;   \
      lezchty   : /^/ <operator> <expr>+ /$/ ;              \
    ",
    Number, Operator, Expr, Lezchty);


  /* Print version and Exit information */
  puts("Lezchty Version 0.0.2");
  puts("(C) 2015 Ernesto Celis");
  puts("Press Ctrl+c to Exit\n");

  /* In a never ending loop */
  while (1) {
    /* Output our prompt */
    char* input = readline("lez> ");

    /* Add input to history */
    add_history(input);

    /* Parse user input */
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lezchty, &r)) {
      mpc_ast_print(r.output);
      mpc_ast_delete(r.output);
    } else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    /* Free retrieved input */
    free(input);
  }

  /* Undefine and Delete Parsers */
  mpc_cleanup(4, Number, Operator, Expr, Lezchty);

  return 0;
}
