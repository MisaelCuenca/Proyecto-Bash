#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#define MAXLINE 4096
#define MAXARGS 256

/* ========= PROTOTIPOS ========= */
void sigchld_handler(int sig);
char *read_line(void);
int parse_line(char *line, char **argv, int *background, char **infile, char **outfile);
int is_builtin(char *cmd);
int do_builtin(char **argv);
int builtin_cd(char **argv);
int builtin_pwd(char **argv);
int builtin_ls(char **argv);
int builtin_mkdir(char **argv);
int builtin_rm(char **argv);
int builtin_cp(char **argv);
int builtin_mv(char **argv);
int builtin_cat(char **argv);
void execute_command(char **argv, int background, char *infile, char *outfile);

/* ========= MAIN ========= */
int main(void) {
    /* Instalar handler para SIGCHLD (evitar procesos zombie) */
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    while (1) {
        /* Mostrar prompt con el directorio actual */
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("%s$ ", cwd);
        } else {
            printf("CuencaN-bash$ ");
        }
        fflush(stdout);

        char *line = read_line();
        if (!line) { // EOF (Ctrl+D)
            printf("\n");
            break;
        }

        /* ignorar líneas vacías */
        char *trim = line;
        while (*trim == ' ' || *trim == '\t' || *trim == '\n') trim++;
        if (*trim == '\0') { free(line); continue; }

        /* parsear línea */
        char *argv[MAXARGS];
        int background = 0;
        char *infile = NULL;
        char *outfile = NULL;
        int argc = parse_line(line, argv, &background, &infile, &outfile);
        if (argc == 0) { free(line); continue; }

        if (strcmp(argv[0], "exit") == 0) {
            free(line);
            break;
        }

        if (is_builtin(argv[0])) {
            /* aplicar redirección en el proceso actual para builtins */
            int saved_stdin = -1, saved_stdout = -1;
            if (infile) {
                saved_stdin = dup(STDIN_FILENO);
                int fd = open(infile, O_RDONLY);
                if (fd < 0) { perror("open infile"); }
                else { dup2(fd, STDIN_FILENO); close(fd); }
            }
            if (outfile) {
                saved_stdout = dup(STDOUT_FILENO);
                int fd = open(outfile, O_CREAT | O_TRUNC | O_WRONLY, 0644);
                if (fd < 0) { perror("open outfile"); }
                else { dup2(fd, STDOUT_FILENO); close(fd); }
            }

            do_builtin(argv);

            /* restaurar stdin/stdout */
            if (saved_stdin != -1) { dup2(saved_stdin, STDIN_FILENO); close(saved_stdin); }
            if (saved_stdout != -1) { dup2(saved_stdout, STDOUT_FILENO); close(saved_stdout); }
        } else {
            execute_command(argv, background, infile, outfile);
        }

        free(line);
    }

    return 0;
}

/* ========= HANDLER SIGCHLD ========= */
void sigchld_handler(int sig) {
    (void)sig;
    int saved_errno = errno;
    while (1) {
        int status;
        pid_t pid = waitpid(-1, &status, WNOHANG);
        if (pid <= 0) break;
        if (WIFEXITED(status)) {
            fprintf(stderr, "\n[child %d exited %d]\n", pid, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            fprintf(stderr, "\n[child %d killed by signal %d]\n", pid, WTERMSIG(status));
        }
    }
    errno = saved_errno;
}

/* ========= LEER LÍNEA ========= */
char *read_line(void) {
    char *buf = NULL;
    size_t cap = 0;
    ssize_t n = getline(&buf, &cap, stdin);
    if (n == -1) {
        free(buf);
        return NULL;
    }
    if (n > 0 && buf[n-1] == '\n') buf[n-1] = '\0';
    return buf;
}

/* ========= PARSEAR LÍNEA (tokens + <, >, &) ========= */
int parse_line(char *line, char **argv, int *background, char **infile, char **outfile) {
    *background = 0;
    *infile = NULL;
    *outfile = NULL;
    int argc = 0;

    char *p = line;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;

        /* redirección < */
        if (*p == '<') {
            p++;
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '\0') break;
            char *start = p;
            while (*p && *p != ' ' && *p != '\t') p++;
            if (*p) { *p = '\0'; p++; }
            *infile = start;
            continue;
        }

        /* redirección > */
        if (*p == '>') {
            p++;
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '\0') break;
            char *start = p;
            while (*p && *p != ' ' && *p != '\t') p++;
            if (*p) { *p = '\0'; p++; }
            *outfile = start;
            continue;
        }

        /* token normal */
        char *start = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '<' && *p != '>') p++;
        if (*p == '&') {
            *p = '\0';
            argv[argc++] = start;
            *background = 1;
            p++;
            while (*p == ' ' || *p == '\t') p++;
            break;
        }
        if (*p) { *p = '\0'; p++; }
        argv[argc++] = start;

        if (argc > 0 && strcmp(argv[argc-1], "&") == 0) {
            argv[argc-1] = NULL;
            argc--;
            *background = 1;
        }
        if (argc >= MAXARGS-1) break;
    }
    argv[argc] = NULL;
    return argc;
}

/* ========= BUILTINS ========= */
int is_builtin(char *cmd) {
    const char *builtins[] = {"cd","pwd","ls","mkdir","rm","cp","mv","cat", NULL};
    for (int i = 0; builtins[i]; ++i)
        if (strcmp(cmd, builtins[i]) == 0) return 1;
    return 0;
}

int do_builtin(char **argv) {
    if (strcmp(argv[0], "cd") == 0)     return builtin_cd(argv);
    if (strcmp(argv[0], "pwd") == 0)    return builtin_pwd(argv);
    if (strcmp(argv[0], "ls") == 0)     return builtin_ls(argv);
    if (strcmp(argv[0], "mkdir") == 0)  return builtin_mkdir(argv);
    if (strcmp(argv[0], "rm") == 0)     return builtin_rm(argv);
    if (strcmp(argv[0], "cp") == 0)     return builtin_cp(argv);
    if (strcmp(argv[0], "mv") == 0)     return builtin_mv(argv);
    if (strcmp(argv[0], "cat") == 0)    return builtin_cat(argv);
    fprintf(stderr, "builtin no implementado\n");
    return -1;
}

/* cd */
int builtin_cd(char **argv) {
    char *dir = argv[1];
    if (!dir || strlen(dir) == 0) {
        dir = getenv("HOME");
    }
    if (!dir) dir = "/";
    if (chdir(dir) != 0) {
        perror("cd");
        return -1;
    }
    return 0;
}

/* pwd */
int builtin_pwd(char **argv) {
    (void)argv;
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    } else {
        perror("pwd");
    }
    return 0;
}

/* ls sencillo: lista archivos no ocultos */
int builtin_ls(char **argv) {
    char *path = argv[1] ? argv[1] : ".";
    DIR *d = opendir(path);
    if (!d) {
        perror("ls");
        return -1;
    }
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;  // omitir ocultos
        printf("%s  ", entry->d_name);
    }
    printf("\n");
    closedir(d);
    return 0;
}

/* mkdir */
int builtin_mkdir(char **argv) {
    if (!argv[1]) {
        fprintf(stderr, "mkdir: falta operando\n");
        return -1;
    }
    if (mkdir(argv[1], 0755) != 0) {
        perror("mkdir");
        return -1;
    }
    return 0;
}

/* rm básico: intenta unlink y si no, rmdir (para dir vacíos) */
int builtin_rm(char **argv) {
    if (!argv[1]) {
        fprintf(stderr, "rm: falta operando\n");
        return -1;
    }
    if (unlink(argv[1]) != 0) {
        if (rmdir(argv[1]) != 0) {
            perror("rm");
            return -1;
        }
    }
    return 0;
}

/* función auxiliar para cp/mv */
int copy_file(const char *src, const char *dst) {
    int in = open(src, O_RDONLY);
    if (in < 0) {
        perror("cp: open src");
        return -1;
    }
    int out = open(dst, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (out < 0) {
        perror("cp: open dst");
        close(in);
        return -1;
    }
    char buf[4096];
    ssize_t r;
    while ((r = read(in, buf, sizeof(buf))) > 0) {
        ssize_t w = write(out, buf, r);
        if (w != r) {
            perror("cp: write");
            close(in);
            close(out);
            return -1;
        }
    }
    if (r < 0) perror("cp: read");
    close(in);
    close(out);
    return (r < 0) ? -1 : 0;
}

/* cp */
int builtin_cp(char **argv) {
    if (!argv[1] || !argv[2]) {
        fprintf(stderr, "cp: uso: cp origen destino\n");
        return -1;
    }
    return copy_file(argv[1], argv[2]);
}

/* mv */
int builtin_mv(char **argv) {
    if (!argv[1] || !argv[2]) {
        fprintf(stderr, "mv: uso: mv origen destino\n");
        return -1;
    }
    if (rename(argv[1], argv[2]) == 0) return 0;
    if (copy_file(argv[1], argv[2]) == 0) {
        if (unlink(argv[1]) != 0) {
            perror("mv: unlink");
            return -1;
        }
        return 0;
    }
    return -1;
}

/* cat */
int builtin_cat(char **argv) {
    if (!argv[1]) {
        char buf[4096];
        ssize_t r;
        while ((r = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
            if (write(STDOUT_FILENO, buf, r) != r) {
                perror("cat: write");
                return -1;
            }
        }
        if (r < 0) perror("cat: read");
        return (r < 0) ? -1 : 0;
    }
    for (int i = 1; argv[i]; ++i) {
        int fd = open(argv[i], O_RDONLY);
        if (fd < 0) {
            perror("cat");
            return -1;
        }
        char buf[4096];
        ssize_t r;
        while ((r = read(fd, buf, sizeof(buf))) > 0) {
            if (write(STDOUT_FILENO, buf, r) != r) {
                perror("cat: write");
                close(fd);
                return -1;
            }
        }
        if (r < 0) {
            perror("cat: read");
            close(fd);
            return -1;
        }
        close(fd);
    }
    return 0;
}

/* ========= EJECUCIÓN DE COMANDOS EXTERNOS ========= */
void execute_command(char **argv, int background, char *infile, char *outfile) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return;
    }
    if (pid == 0) {
        /* hijo */

        /* redirección entrada */
        if (infile) {
            int fd = open(infile, O_RDONLY);
            if (fd < 0) {
                perror("open infile");
                exit(EXIT_FAILURE);
            }
            if (dup2(fd, STDIN_FILENO) < 0) {
                perror("dup2 infile");
                exit(EXIT_FAILURE);
            }
            close(fd);
        }
        /* redirección salida */
        if (outfile) {
            int fd = open(outfile, O_CREAT | O_TRUNC | O_WRONLY, 0644);
            if (fd < 0) {
                perror("open outfile");
                exit(EXIT_FAILURE);
            }
            if (dup2(fd, STDOUT_FILENO) < 0) {
                perror("dup2 outfile");
                exit(EXIT_FAILURE);
            }
            close(fd);
        }

        execvp(argv[0], argv);
        perror("execvp");
        exit(EXIT_FAILURE);
    } else {
        /* padre */
        if (background) {
            printf("[started %d]\n", pid);
            /* no esperamos: lo recoge el handler SIGCHLD */
        } else {
            int status;
            if (waitpid(pid, &status, 0) < 0) perror("waitpid");
        }
    }
}

