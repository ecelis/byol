#include <stdio.h>
#include <stdlib.h>

/* Win support */
#ifdef _WIN32

/* Fake readline */
char* readline(char* prompt)
{
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char* cpy = malloc(strlen(buffer) + 1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy)-1] = '\0';
  return cpy;
}

/* Fake add_history */
void add_history(char* unused) {}

/* Add Linux readline suuport */
#else
#include <readline/readline.h>
#include <readline/history.h>
#endif

int main(int argc, char** argv)
{
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

    /* Echo input back to user */
    printf("=>%s\n", input);

    /* Free retrieved input */
    free(input);
  }
  return 0;
}
