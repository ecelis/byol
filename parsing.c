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
enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR };

/* lval */
typedef struct lval {
  int type;
  long num;
  /* Error and Symbol have string data */
  char *err;
  char *sym;
  /* Count and Pointer to a list of lval */
  int count;
  struct lval **cell;
} lval;

/* Forward define functions */
void lval_println(lval *v);

/* lval Number Type */
lval *lval_num(long x)
{
  lval *v malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;
  return v;
}

/* lval Error Type */
lval *lval_err(char *m)
{
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_ERR;
  v->err = malloc(strlen(m) + 1);
  strcpy(v->err, m);
  return v;
}

/* lval Symbol */
lval *lval_sym(char *s)
{
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = malloc(strlen(s) + 1);
  strcpy(v->sym, s);
  return v;
}

/* Empty lval Sexpr */
lval *lval_sexpr(void)
{
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

void lval_del(lval *v)
{
  switch (v->type) {
    case LVAL_NUM:
      break;
    case LVAL_ERR:
      free(v->err);
      break;
    case LVAL_SYM:
      free(v->sym);
      break;
    case LVAL_SEXPR:
        for (int i = 0; i < v->count; i++) {
          lval_del(v->cell[i]);
        }
      free(v->cell);
      break;
  }
}

lval *lval_read_num(mpc_ast_t *t)
{
  errno = 0;
  long x = strtol(t->contents, NULL, 10);
  return errno != ERANGE ? lval_num(x) : lval_err("Invalid number");
}

lval *lval_read(mpc_ast_t *t)
{
  if (strstr(t->tag, "number")) { return lval_read_num(t); }
  if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

  /* If root (>) or sexpr create empty list */
  lval *x = NULL;
  if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
  if (strcmp(t->tag, "sexpr")) { x = lval_sexpr(); }

  /* Fill the list with any valid expression contained within */
  for (int i = 0; i < t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
    if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
    if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
    if (strcmp(t->children[i]->tag, "regex") == 0) { continue; }
    x = lval_add(x, lval_read(t->children[i]));
  }

  return x;
}

lval *lval_add(lval *v, lval *x)
{
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  v->cell[v->count-1] = x;
  return v;
}

void lval_expr_print(lval *v, char open, char close)
{
  putchar(open);
  for (int i = 0; i < v->count; i++) {
    lval_print(v->cell[i]);
    if(i != (v->count-1)) {
      putchar(' ');
    }
  }
  putchar(close);
}

void lval_print(lval *v)
{
  switch (v->type) {
    case LVAL_NUM:
      printf("%li", v->num);
      break;
    case LVAL_ERR:
      printf("Error: %s", v->err);
      break;
    case LVAL_SYM:
      printf("%s", v->sym);
      break;
    case LVAL_SEXPR:
      lval_expr_print(v, '(', ')');
      break;
  }
}

void lval_println(lval *v)
{
  lval_print(v);
  putchar('\n');
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
  mpc_parser_t *Symbol    = mpc_new("symbol");
  mpc_parser_t *Sexpr     = mpc_new("sexpr");
  mpc_parser_t *Expr      = mpc_new("expr");
  mpc_parser_t *Lezchty   = mpc_new("lezchty");

  /* Define language parser */
  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                                 \
      number    :  /-?[0-9]+/ ;                                       \
      symbol  :  '+' | '-' | '*' | '/' | '%' ;                      \
      sexpr    :  '(' <expr>* ')' ;                                       \
      expr      : <number> | <letter> | '(' <symbol> <expr>+ ')' ;  \
      lezchty   : /^/ <symbol> <expr>+ /$/ ;                        \
    ",
    Number, Symbol, Sexpr, Expr, Lezchty);


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
  mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Lezchty);

  return 0;
}
