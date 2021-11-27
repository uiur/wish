#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/errno.h>
#include <fcntl.h>

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

void exec_command(char* const args[], const char* output) {
  if (output != NULL) {
    int fd = open(output, O_RDWR|O_CREAT|O_TRUNC, 0644);
    if (fd == -1) {
      perror(NULL);
      exit(EXIT_FAILURE);
    }

    close(1);
    dup(fd);
  }

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
}

void launch_process(char* const args[], const char* output) {
  int pid = fork();
  if (pid < 0) {
    fprintf(stderr, "fork failed\n");
    exit(EXIT_FAILURE);
  } else if (pid == 0) {
    exec_command(args, output);
  } else {
    int rc_wait = waitpid(pid, NULL, WUNTRACED);
    if (rc_wait == -1) {
      fprintf(stderr, "child failed:\n");
      perror(NULL);
    }
  }
}

void execute(char* args[]) {
  int args_size = 0;
  while (args[args_size] != NULL) args_size++;

  if (args[0] == NULL) return;
  if (strcmp(args[0], "exit") == 0) {
    exit(EXIT_SUCCESS);
  }

  if (strcmp(args[0], "cd") == 0) {
    if (args_size == 2) {
      if (chdir(args[1]) == -1) {
        perror(NULL);
      }
    } else {
      fprintf(stderr, "cd: expects 2 args, but got %d", args_size);
    }
    return;
  }

  char* redirect_output = NULL;
  for (int i = 0; args[i] != NULL; i++) {
    if (strcmp(args[i], ">") == 0) {
      if (args[i + 1] == NULL) {
        fprintf(stderr, "parse error after >\n");
        return;
      }
      redirect_output = args[i + 1];
      args[i] = NULL;
    }
  }

  launch_process(args, redirect_output);
}

char* read_line() {
  char* line = NULL;
  size_t linecap = 0;
  ssize_t linelen;

  if (getline(&line, &linecap, stdin) < 0) exit(EXIT_FAILURE);

  return line;
}

char** parse_args(char* line) {
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

  return args;
}

int main(int argc, char* argv[]) {
  while (1) {

    printf("> ");
    char* line = read_line();
    char** args = parse_args(line);

    execute(args);

    free(line);
    free(args);
  }

  return 0;

}
