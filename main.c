#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/errno.h>

#define TOKEN_BUF_SIZE 100
#define FILE_PATH_BUF_SIZE 100

char *paths[] = {
  "",
  "/bin/",
  NULL
};

char* search_paths(const char* command) {
  int buf_size = FILE_PATH_BUF_SIZE;
  char* buf = malloc(sizeof(char) * buf_size);

  for (int i = 0; paths[i] != NULL; i++) {
    strlcpy(buf, paths[i], buf_size);
    strlcat(buf, command, buf_size);
    int status = access(buf, X_OK);
    if (status == 0) {
      return buf;
    }
  }

  return NULL;
}

void execute(char* const args[]) {
  int pid = fork();
  if (pid < 0) {
    fprintf(stderr, "fork failed\n");
    exit(EXIT_FAILURE);
  } else if (pid == 0) {
    char* file_path = search_paths(args[0]);
    if (file_path == NULL) {
      fprintf(stderr, "command not found: %s\n", args[0]);
      exit(EXIT_FAILURE);
    }
    int rc_exec = execv(file_path, args);
    if (rc_exec == -1) {
      fprintf(stderr, "exec failed: %s\n", file_path);
      perror(NULL);
      exit(EXIT_FAILURE);
    }
    exit(0);
  } else {
    int rc_wait = wait(NULL);
    if (rc_wait == -1) {
      fprintf(stderr, "child failed:\n");
      perror(NULL);
    }
  }
}

int main(int argc, char* argv[]) {
  while (1) {
    char* line = NULL;
    size_t linecap = 0;
    ssize_t linelen;

    printf("> ");
    if (getline(&line, &linecap, stdin) < 0) exit(EXIT_FAILURE);
    if (strcmp(line, "exit\n") == 0) exit(0);

    int buf_size = TOKEN_BUF_SIZE;
    char** args = calloc(buf_size, sizeof(char*));
    int args_size = 0;
    char* token;
    while ((token = strsep(&line, " \t\n"))) {
      if (strlen(token) == 0) continue;

      args[args_size] = token;
      args_size++;
      if (args_size >= buf_size) {
        buf_size += TOKEN_BUF_SIZE;
        args = realloc(args, buf_size * sizeof(char*));
      }
      if (token == NULL) break;
    }

    execute(args);

    free(line);
  }

  return 0;

}
