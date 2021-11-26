#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char* argv[]) {
  while (1) {
    char* line = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    printf("wish> ");
    linelen = getline(&line, &linecap, stdin);
    if (linelen <= 0) break;
    if (strcmp(line, "exit\n") == 0) break;

    free(line);
  }

  return 0;

}
