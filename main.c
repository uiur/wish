#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char *paths[] = {
  "",
  "/bin/",
  NULL
};

char* search_paths(const char* command) {
  char* buf = malloc(sizeof(char) * 100);

  for (int i = 0; paths[i] != NULL; i++) {
    strlcpy(buf, paths[i], sizeof(buf));
    strlcat(buf, command, sizeof(buf));
    int status = access(buf, X_OK);
    if (status == 0) {
      return buf;
    }
  }

  return NULL;
}

int main(int argc, char* argv[]) {
  while (1) {
    char* line = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    printf("wish> ");
    linelen = getline(&line, &linecap, stdin);
    if (linelen <= 0) exit(0);
    line[linelen - 1] = '\0';

    if (strcmp(line, "exit") == 0) exit(0);

    char* str = strdup(line);
    char* to_free = str;

    char** args = calloc(0, sizeof(char*));
    int args_size = 0;
    char* token;
    while ((token = strsep(&str, " ")) != NULL) {
      args_size++;
      args = realloc(args, args_size * sizeof(char*));
      args[args_size - 1] = token;
    }

    int rc = fork();
    if (rc < 0) {
      fprintf(stderr, "fork failed\n");
      exit(1);
    } else if (rc == 0) {
      // child
      char* file_path = search_paths(args[0]);
      if (file_path == NULL) {
        fprintf(stderr, "command not found: %s\n", args[0]);
        exit(1);
      }
      int rc_exec = execv(file_path, args);
      if (rc_exec == -1) {
        fprintf(stderr, "exec failed\n");
      }
      exit(0);
    } else {
      // parent
      int rc_wait = wait(NULL);
    }

    free(to_free);

    free(line);
  }

  return 0;

}
