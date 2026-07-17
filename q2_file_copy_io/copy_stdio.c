#include <stdio.h>
#include <stdlib.h>
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

    FILE *src = fopen(argv[1], "rb");
    if (!src) {
        perror("fopen source");
        return 1;
    }

    FILE *dst = fopen(argv[2], "wb");
    if (!dst) {
        perror("fopen destination");
        fclose(src);
        return 1;
    }

    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    long total_bytes = 0;

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, src)) > 0) {
        if (fwrite(buffer, 1, bytes_read, dst) != bytes_read) {
            perror("fwrite");
            fclose(src);
            fclose(dst);
            return 1;
        }
        total_bytes += bytes_read;
    }

    if (ferror(src)) {
        perror("fread");
        fclose(src);
        fclose(dst);
        return 1;
    }

    fclose(src);
    fclose(dst);

    clock_gettime(CLOCK_MONOTONIC, &end);

    printf("copy_stdio: copied %ld bytes in %.4f seconds (buffer = %d bytes)\n",
           total_bytes, elapsed_seconds(start, end), BUFFER_SIZE);

    return 0;
}
