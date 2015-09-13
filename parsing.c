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

/* Error Types */
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

/* lval Types */
enum { LVAL_NUM, LVAL_ERR };

/* lval */
typedef struct {
  int type;
  long num;
  int err;
} lval;

/* lval Number Type */
lval lval_num(long x)
{
  lval v;
  v.type = LVAL_NUM;
  v.num = x;
  return v;
}

/* lval Error Type */
lval lval_err(int x)
{
  lval v;
  v.type = LVAL_ERR;
  v.err = x;
  return v;
}

/* Print an lval */
void lval_print(lval v)
{
  switch (v.type) {
    case LVAL_NUM:
      printf("%li", v.num);
      break;
    case LVAL_ERR:
      if (v.err == LERR_DIV_ZERO) {
        printf("Error: Division by zero");
      }
      if (v.err == LERR_BAD_OP) {
        printf("Error: Invalid operator");
      }
      if (v.err == LERR_BAD_NUM) {
        printf("Error: Invalid number");
      }
      break;
  }
}

/* Print lvla followed by new line */
void lval_println(lval v)
{
  lval_print(v);
  putchar('\n');
}

/* Use operator string to see which operation perform */
lval eval_op(lval x, char *op, lval y)
{
  if (x.type == LVAL_ERR) { return x; }
  if (y.type == LVAL_ERR) { return y; }

  if (strcmp(op, "+") == 0) { return lval_num(x.num + y.num); }
  if (strcmp(op, "-") == 0) { return lval_num(x.num - y.num); }
  if (strcmp(op, "*") == 0) { return lval_num(x.num * y.num); }
  if (strcmp(op, "/") == 0) {
    return y.num == 0 ? lval_err(LERR_DIV_ZERO) : lval_num(x.num / y.num);
  }
  return lval_err(LERR_BAD_OP);
}


lval eval(mpc_ast_t *t)
{
  if (strstr(t->tag, "number")) {
    /* Check for error in conversion */
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
  }

  /* Operator is always second child */
  char *op = t->children[1]->contents;

  /* Store third child in `x` */
  lval x = eval(t->children[2]);

  /* Iterate remaining childre and combining */
  int i = 3;
  while (strstr(t->children[i]->tag, "expr")) {
    x = eval_op(x, op, eval(t->children[i]));
    i++;
  }

  return x;
}

int main(int argc, char** argv)
{
  /* Create parsers */
  mpc_parser_t *Number    = mpc_new("number");
  mpc_parser_t *Letter    = mpc_new("letter");
  mpc_parser_t *Operator  = mpc_new("operator");
  mpc_parser_t *Expr      = mpc_new("expr");
  mpc_parser_t *Lezchty   = mpc_new("lezchty");

  /* Define language parser */
  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                                 \
      number    :  /-?[0-9]+/ ;                                       \
      letter    :  'a' | 'b'  ;                                       \
      operator  :  '+' | '-' | '*' | '/' | '%' ;                      \
      expr      : <number> | <letter> | '(' <operator> <expr>+ ')' ;  \
      lezchty   : /^/ <operator> <expr>+ /$/ ;                        \
    ",
    Number, Letter, Operator, Expr, Lezchty);


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
      lval result = eval(r.output);
      lval_println(result);
      mpc_ast_delete(r.output);
    } else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    /* Free retrieved input */
    free(input);
  }

  /* Undefine and Delete Parsers */
  mpc_cleanup(5, Number, Letter, Operator, Expr, Lezchty);

  return 0;
}
