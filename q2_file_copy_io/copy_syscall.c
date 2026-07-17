#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#define BUFFER_SIZE 1024

static double elapsed_seconds(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <source> <destination>\n", argv[0]);
        return 1;
    }

    int src = open(argv[1], O_RDONLY);
    if (src == -1) {
        perror("open source");
        return 1;
    }

    int dst = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dst == -1) {
        perror("open destination");
        close(src);
        return 1;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    long total_bytes = 0;

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    while ((bytes_read = read(src, buffer, BUFFER_SIZE)) > 0) {
        ssize_t bytes_written = 0;
        while (bytes_written < bytes_read) {
            ssize_t n = write(dst, buffer + bytes_written, bytes_read - bytes_written);
            if (n == -1) {
                perror("write");
                close(src);
                close(dst);
                return 1;
            }
            bytes_written += n;
        }
        total_bytes += bytes_read;
    }

    if (bytes_read == -1) {
        perror("read");
        close(src);
        close(dst);
        return 1;
    }

    close(src);
    close(dst);

    clock_gettime(CLOCK_MONOTONIC, &end);

    printf("copy_syscall: copied %ld bytes in %.4f seconds (buffer = %d bytes)\n",
           total_bytes, elapsed_seconds(start, end), BUFFER_SIZE);

    return 0;
}
