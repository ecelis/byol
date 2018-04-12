#include <stdio.h>
#include <stdlib.h>

#include "mpc.h"

/* Win support */
#ifdef _WIN32
#include <string.h>

static char buffer[2048];

/* Fake readline */
char* readline (char *prompt)
{
	fputs (prompt, stdout);
	fgets (buffer, 2048, stdin);
	char *cpy = malloc (strlen (buffer) + 1);
	strcpy (cpy, buffer);
	cpy[strlen (cpy) - 1] = '\0';
	return cpy;
}

/* Fake add_history */
void add_history (char *unused) {}

#else
/* Add Linux readline suuport */
#include <readline/readline.h>
#include <readline/history.h>
#endif

/* lval Types */
enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR };

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

/* lval Number Type */
lval *lval_num (long x)
{
	lval *v = malloc (sizeof(lval));
	v->type = LVAL_NUM;
	v->num = x;
	return v;
}

/* lval Error Type */
lval *lval_err(char *m)
{
	lval *v = malloc (sizeof(lval));
	v->type = LVAL_ERR;
	v->err = malloc (strlen (m) + 1);
	strcpy (v->err, m);
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

/* A pointer to new empty Qexpr lval */
lval *lval_qexpr(void)
{
	lval *v = malloc(sizeof(lval));
	v->type = LVAL_QEXPR;
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
		free (v->err);
		break;
	case LVAL_SYM:
		free (v->sym);
		break;
	case LVAL_SEXPR:
	case LVAL_QEXPR:
		for (int i = 0; i < v->count; i++) {
			lval_del (v->cell[i]);
		}
		free (v->cell);
		break;
	}
	/* Free the memory allocated for the lval struct itself */
	free (v);
}

lval *lval_add (lval *v, lval *x)
{
	v->count++;
	v->cell = realloc (v->cell, sizeof(lval*) * v->count);
	v->cell[v->count - 1] = x;
	return v;
}

lval *lval_pop (lval *v, int i)
{
	/* Find the item at i */
	lval *x = v->cell[i];

	/*Shift memory after the item at i over the top */
	memmove (&v->cell[i], &v->cell[i+1],
		sizeof(lval*) * (v->count - i - 1));

	/* Decrease the count of items in the list */
	v->count--;

	/* Decrease the memory used */
	v->cell = realloc (v->cell, sizeof(lval*) * v->count);
	return x;
}

lval *lval_conj (lval *x, lval *y)
{
	while (y->count) {
		x = lval_add (x, lval_pop (y, 0));
	}
	lval_del(y);
	return x;
}

lval *lval_take (lval *v, int i)
{
	lval *x = lval_pop (v, i);
	lval_del (v);
	return x;
}

/* Forward define functions */
void lval_print (lval *v);

void lval_expr_print (lval *v, char open, char close)
{
	putchar (open);
	for (int i = 0; i < v->count; i++) {
		/* Print value contained within */
		lval_print (v->cell[i]);
		/* Don't print trailing space */
		if(i != (v->count - 1)) {
			putchar (' ');
		}
	}
	putchar (close);
}

/* Print an lval */
void lval_print (lval *v)
{
	switch (v->type) {
	case LVAL_NUM:
		printf ("%li", v->num);
		break;
	case LVAL_ERR:
		printf ("Error: %s", v->err);
		break;
	case LVAL_SYM:
		printf ("%s", v->sym);
		break;
	case LVAL_SEXPR:
		lval_expr_print (v, '(', ')');
		break;
	case LVAL_QEXPR:
		lval_expr_print (v, '{', '}');
		break;
	}
}

void lval_println (lval *v)
{
	lval_print (v);
	putchar ('\n');
}

#define LASSERT(args, cond, err) \
	if (!(cond)) { lval_del (args); return lval_err (err); }

lval *lval_eval (lval *v);

/* Return S-Expression transformed to Q-expression (builds a list) */
lval *builtin_list (lval *a)
{
	a->type = LVAL_QEXPR;
	return a;
}

/* Return the head(first) element of the list CAR */
lval *builtin_first (lval *a)
{
	LASSERT (a, a->count == 1,
		 "Function 'first' passed too many arguments.");
	LASSERT (a, a->cell[0]->type == LVAL_QEXPR,
		 "Function 'first' passed incorrect type.");
	LASSERT (a, a->cell[0]->count != 0,
		 "Function 'first' passed {}.");

	lval *v = lval_take (a, 0);

	while (v->count > 1) {
		lval_del (lval_pop (v, 1));
	}

	return v;
}

/* Return the list minus the first element (tail) CDR */
lval *builtin_rest (lval *a)
{
	LASSERT (a, a->count == 1,
		 "Function 'rest' passed too many arguments.");
	LASSERT (a, a->cell[0]->type == LVAL_QEXPR,
		 "Function 'rest' passed incorrect type.");
	LASSERT (a, a->cell[0]->count != 0,
		 "Function 'rest' passed {}.");

	lval *v = lval_take (a, 0);
	lval_del (lval_pop (v, 0));
	return v;
}

/* Return a S-Expression from a Q-Expression and evaluates it */
lval *builtin_eval (lval *a)
{
	LASSERT (a, a->count == 1,
		 "Function 'eval' passed too many arguments.");
	LASSERT (a, a->cell[0]->type == LVAL_QEXPR,
		 "Function 'eval' passed incorrect type.");

	lval *x = lval_take(a, 0);
	x->type = LVAL_SEXPR;
	return lval_eval (x);
}

/* Return a joint Q-Expression from N Q-Expression in input */
lval *builtin_conj (lval *a)
{
	for (int i = 0; i < a->count; i++) {
		LASSERT (a, a->cell[i]->type == LVAL_QEXPR,
			 "Function 'conj' passed incorrect type.");
	}

	lval *x = lval_pop (a, 0);

  /* For each cell in y add it to x */
	while (a->count) {
		x = lval_conj (x, lval_pop (a, 0));
	}

	lval_del (a);

	return x;
}

lval *builtin_op (lval *a, char *op)
{
	/* Ensure all arguments are numbers */
	for (int i = 0; i < a->count; i++) {
		if (a->cell[i]->type != LVAL_NUM) {
			lval_del (a);
			return lval_err ("Cannot operate on non-number!");
		}
	}

	/* Pop the first element */
	lval *x = lval_pop (a, 0);

	/* If no argument and sub then perform unary negation */
	if ((strcmp (op, "-") == 0) && a->count == 0)
		x->num = -x->num;

	/* While there are still elements remaining */
	while (a->count > 0) {
		/* Pop the next element */
		lval *y = lval_pop (a, 0);

		/* Perform operation */
		if (strcmp (op, "+") == 0)
			x->num += y->num;

		if (strcmp (op, "-") == 0)
			x->num -= y->num;

		if (strcmp (op, "*") == 0)
			x->num *= y->num;

		if (strcmp (op, "/") == 0) {
			if (y->num == 0) {
				lval_del (x);
				lval_del (y);
				x = lval_err ("Division by zero.");
				break;
			}
			x->num /= y->num;
		}

		/* Delete element now finished with */
		lval_del (y);
	}

	/* Delete input expression and return result */
	lval_del (a);
	return x;
}

lval *builtin (lval *a, char *func)
{
	if (strcmp ("list", func) == 0)
		return builtin_list (a);

	if (strcmp ("first", func) == 0)
		return builtin_first (a);

	if (strcmp ("rest", func) == 0)
		return builtin_rest (a);

	if (strcmp ("conj", func) == 0)
		return builtin_conj (a);

	if (strcmp ("eval", func) == 0)
		return builtin_eval (a);

	if (strstr ("+-/*%", func))
		return builtin_op (a, func);

	lval_del (a);
	return lval_err ("Unknown function!");
}


lval *lval_eval_sexpr (lval *v)
{
	/* Evaluate children */
	for (int i = 0; i < v->count; i++)
		v->cell[i] = lval_eval (v->cell[i]);

	/* Error checking */
	for (int i = 0; i < v->count; i++) {
		if (v->cell[i]->type == LVAL_ERR)
			return lval_take (v, i);
	}

	/* Empty Expression */
	if (v->count == 0)
		return v;

	/* Single Expression */
	if (v->count == 1)
		return lval_take (v, 0);

	/* Ensure first Element is Symbol */
	lval *f = lval_pop (v, 0);
	if (f->type != LVAL_SYM) {
		lval_del (f);
		lval_del (v);
		return lval_err ("S-Expression does not start with symbol.");
	}

	/* Call builtin with operator */
	lval *result = builtin (v, f->sym);
	lval_del (f);
	return result;
}

lval *lval_eval(lval *v)
{
	/* Evaluate S-Expressions */
	if (v->type == LVAL_SEXPR)
		return lval_eval_sexpr (v);

	/* All other lvla types return the same */
	return v;
}


lval *lval_read_num(mpc_ast_t *t)
{
	errno = 0;
	long x = strtol(t->contents, NULL, 10);
	return errno != ERANGE ? lval_num(x) : lval_err("Invalid number");
}

lval *lval_read(mpc_ast_t *t)
{
  if (strstr(t->tag, "number"))
	  return lval_read_num(t);

  if (strstr(t->tag, "symbol"))
	  return lval_sym(t->contents);

  /* If root (>) or sexpr create empty list */
  lval *x = NULL;
  if (strcmp(t->tag, ">") == 0)
	  x = lval_sexpr();

  if (strcmp(t->tag, "sexpr"))
	  x = lval_sexpr();

  if (strstr(t->tag, "qexpr"))
	  x = lval_qexpr();

  /* Fill the list with any valid expression contained within */
  for (int i = 0; i < t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0)
	    continue;
    if (strcmp(t->children[i]->contents, ")") == 0)
	    continue;
    if (strcmp(t->children[i]->contents, "}") == 0)
	    continue;
    if (strcmp(t->children[i]->contents, "{") == 0)
	    continue;
    if (strcmp(t->children[i]->tag, "regex") == 0)
	    continue;
    x = lval_add(x, lval_read(t->children[i]));
  }

  return x;
}

int main(int argc, char** argv)
{
  /* Create parsers */
  // TODO define JSON grammar
  mpc_parser_t *Number    = mpc_new("number");
  mpc_parser_t *Symbol    = mpc_new("symbol");
  mpc_parser_t *Sexpr     = mpc_new("sexpr");
  mpc_parser_t *Qexpr	    = mpc_new("qexpr");
  mpc_parser_t *Expr      = mpc_new("expr");
  mpc_parser_t *Lezchty   = mpc_new("lezchty");

  /* Define language parser */
  // FIXME Add floating point numbers support.
  // FIXME Add missing and useful operators such as ^, %, min, max
  // FIXME Change - operator so that when it receives one argument negates it
  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                           \
      number    :  /-?[0-9]+/ ;                                 \
      symbol	:  \"list\" | \"first\" | \"rest\"		\
		| \"conj\" | \"eval\" | '+' | '-'		\
		| '*' | '/' | '%' ;				\
      sexpr    :  '(' <expr>* ')' ;                                       \
      qexpr	: '{' <expr>* '}' ;				\
      expr      : <number> | <symbol> | <sexpr> | <qexpr> ;  \
      lezchty   : /^/ <expr>* /$/ ;                        \
    ",
    Number, Symbol, Sexpr, Qexpr, Expr, Lezchty);


  /* Print version and Exit information */
  puts("Lezchty Version 0.0.4");
  puts("(C) 2014 Daniel Holden, 2015 modifications by Ernesto Celis");
  puts("Learn C and Build your own lisp at http://www.buildyourownlisp.com/");
  puts("Licensed under Creative Commons Attribution-NonCommercial-ShareAlike 3.0");
  puts("Press Ctrl+c to Exit\n");

  /* In a never ending loop */
  while (1) {
    /* Output our prompt */
    char* input = readline("lez> ");

    /* Add input to history */
    add_history(input);

    /* Parse user input */
    mpc_result_t r;
    if (mpc_parse ("<stdin>", input, Lezchty, &r)) {
      lval *x = lval_eval (lval_read (r.output));
      lval_println (x);
      lval_del (x);
      mpc_ast_delete (r.output);
    } else {
      mpc_err_print (r.error);
      mpc_err_delete (r.error);
    }

    /* Free retrieved input */
    free (input);
  }

  /* Undefine and Delete Parsers */
  mpc_cleanup (6, Number, Symbol, Sexpr, Qexpr, Expr, Lezchty);

  return 0;
}
