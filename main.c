#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/errno.h>
#include <fcntl.h>

#define TOKEN_BUF_SIZE 100
#define FILE_PATH_BUF_SIZE 100

typedef char* command_token;

char *paths[] = {
  "",
  "/bin/",
  "/usr/bin/",
  NULL
};

void show_command(char* args[]) {
  for (int i = 0; args[i] != NULL; i++) {
    printf(" %s", args[i]);
  }
  puts("");
}

int fork_or_die() {
  int pid = fork();
  if (pid < 0) {
    fprintf(stderr, "fork failed\n");
    perror(NULL);
    exit(EXIT_FAILURE);
  }

  return pid;
}

char* search_paths(const command_token command) {
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

void exec_shell_command(command_token args[], int in_fd, int out_fd) {
  if (in_fd != 0) {
    close(0);
    dup(in_fd);
  }

  if (out_fd != 1) {
    close(1);
    dup(out_fd);
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

command_token** parse_shell_statement(command_token args[]) {
  int size = 1;
  while (args[size - 1] != NULL) size++;

  char** current_args = calloc(size, sizeof(char*));
  int current_args_index = 0;
  char*** commands = calloc(size, sizeof(char**));
  int commands_index = 0;

  for (int i = 0; args[i] != NULL; i++) {
    if (strcmp(args[i], "|") == 0) {
      current_args[current_args_index] = NULL;

      commands[commands_index] = current_args;
      commands_index++;

      current_args = calloc(size, sizeof(char*));
      current_args_index = 0;
      continue;
    }

    current_args[current_args_index] = args[i];
    current_args_index++;
  }

  current_args[current_args_index] = NULL;
  commands[commands_index] = current_args;
  commands_index++;
  commands[commands_index] = NULL;


  return commands;
}

char* extract_redirection(command_token args[]) {
  for (int i = 0; args[i] != NULL; i++) {
    if (strcmp(args[i], ">") == 0) {
      if (args[i + 1] == NULL) {
        fprintf(stderr, "parse error after >\n");
        exit(EXIT_FAILURE);
      }
      args[i] = NULL;
      return args[i + 1];
    }
  }

  return NULL;
}

void exec_shell_statement(command_token args[]) {
  int size = 1;
  while (args[size - 1] != NULL) size++;
  char* output = extract_redirection(args);

  command_token** commands = parse_shell_statement(args);

  int dest_fd = 1;
  if (output != NULL) {
    dest_fd = open(output, O_RDWR|O_CREAT|O_TRUNC, 0644);
    if (dest_fd == -1) {
      perror(NULL);
      exit(EXIT_FAILURE);
    }
  }

  int* pids = calloc(size, sizeof(int));

  int current_pipe_fds[2];
  current_pipe_fds[0] = 0;
  current_pipe_fds[1] = 0;
  for (int i = 0; commands[i] != NULL; i++) {
    int in_fd = 0;
    if (i > 0) {
      in_fd = current_pipe_fds[0];
      close(current_pipe_fds[1]);
    }

    int out_fd;
    if (commands[i+1] != NULL) {
      if (pipe(current_pipe_fds) == -1) {
        perror(NULL);
        exit(EXIT_FAILURE);
      }
      out_fd = current_pipe_fds[1];
    } else {
      out_fd = dest_fd;
    }

    int pid = fork_or_die();
    if (pid == 0) {
      if (out_fd != dest_fd) close(current_pipe_fds[0]);
      exec_shell_command(commands[i], in_fd, out_fd);
      exit(EXIT_SUCCESS);
    }

    // printf("%s: %d %d - %d\n", commands[i][0], pid, in_fd, out_fd);
    pids[i] = pid;
  }

  for (int i = 0; commands[i] != NULL; i++) {
    int pid = pids[i];
    int rc_wait = waitpid(pid, NULL, WUNTRACED);
    if (rc_wait == -1) {
      fprintf(stderr, "child failed:\n");
      perror(NULL);
    }
  }
}

int run_shell_statement(command_token args[]) {
  int pid = fork_or_die();

  if (pid == 0) {
    exec_shell_statement(args);
    exit(EXIT_SUCCESS);
  }

  return pid;
}

void launch_process(command_token args[]) {
  int* pids = calloc(TOKEN_BUF_SIZE, sizeof(int));

  int args_index = 0;
  char** current_args = calloc(TOKEN_BUF_SIZE, sizeof(char*));
  int current_args_index = 0;
  char*** statements = calloc(TOKEN_BUF_SIZE, sizeof(char**));
  int parallel_command_index = 0;

  while (1) {
    if (args[args_index] == NULL || strcmp(args[args_index], "&") == 0)  {
      current_args[current_args_index] = NULL;
      statements[parallel_command_index] = current_args;
      parallel_command_index++;
      statements[parallel_command_index] = NULL;

      if (args[args_index] == NULL) break;

      current_args = calloc(TOKEN_BUF_SIZE, sizeof(char*));
      current_args_index = 0;
    } else {
      current_args[current_args_index] = args[args_index];
      current_args_index++;
    }

    args_index++;
  }

  for (int i = 0; statements[i] != NULL; i++) {
    int pid = run_shell_statement(statements[i]);
    pids[i] = pid;
  }

  for (int i = 0; statements[i] != NULL; i++) {
    int pid = pids[i];
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

  launch_process(args);
}

char* read_line() {
  char* line = NULL;
  size_t linecap = 0;
  ssize_t linelen;

  if (getline(&line, &linecap, stdin) < 0) exit(EXIT_FAILURE);

  return line;
}

command_token* parse_args(char* line) {
  int buf_size = TOKEN_BUF_SIZE;
  command_token* args = calloc(buf_size, sizeof(char*));
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
    command_token* args = parse_args(line);

    execute(args);

    free(line);
    free(args);
  }

  return 0;

}
