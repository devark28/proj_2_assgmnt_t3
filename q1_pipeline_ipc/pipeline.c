#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#define OUTPUT_FILE "outputs/pipeline_output.txt"
#define PREVIEW_LINES 5

int main(void) {
    int fd[2];
    if (pipe(fd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid_t producer = fork();
    if (producer == -1) {
        perror("fork producer");
        exit(EXIT_FAILURE);
    }

    if (producer == 0) {
        close(fd[0]);
        dup2(fd[1], STDOUT_FILENO);
        close(fd[1]);

        char *args[] = {"ps", "aux", NULL};
        execvp(args[0], args);
        perror("execvp ps");
        exit(EXIT_FAILURE);
    }

    pid_t consumer = fork();
    if (consumer == -1) {
        perror("fork consumer");
        exit(EXIT_FAILURE);
    }

    if (consumer == 0) {
        close(fd[1]);
        dup2(fd[0], STDIN_FILENO);
        close(fd[0]);

        int out_fd = open(OUTPUT_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (out_fd == -1) {
            perror("open output file");
            exit(EXIT_FAILURE);
        }
        dup2(out_fd, STDOUT_FILENO);
        close(out_fd);

        char *args[] = {"grep", "root", NULL};
        execvp(args[0], args);
        perror("execvp grep");
        exit(EXIT_FAILURE);
    }

    close(fd[0]);
    close(fd[1]);

    waitpid(producer, NULL, 0);
    waitpid(consumer, NULL, 0);

    printf("Pipeline finished, output captured in %s\n\n", OUTPUT_FILE);

    FILE *fp = fopen(OUTPUT_FILE, "r");
    if (!fp) {
        perror("fopen output file");
        exit(EXIT_FAILURE);
    }

    printf("--- first %d lines of captured output ---\n", PREVIEW_LINES);
    char line[512];
    int shown = 0;
    while (shown < PREVIEW_LINES && fgets(line, sizeof(line), fp)) {
        fputs(line, stdout);
        shown++;
    }
    fclose(fp);

    return 0;
}
