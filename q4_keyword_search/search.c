#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

typedef struct {
    const char *keyword;
    char **files;
    int num_files;
    int next_file;
    long total_matches;
    FILE *output_fp;
    pthread_mutex_t queue_mutex;
    pthread_mutex_t output_mutex;
} search_job_t;

static long count_occurrences(const char *text, const char *keyword) {
    long count = 0;
    size_t keyword_len = strlen(keyword);
    const char *cursor = text;

    while ((cursor = strstr(cursor, keyword)) != NULL) {
        count++;
        cursor += keyword_len;
    }

    return count;
}

static char *read_whole_file(const char *path, long *out_size) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        perror(path);
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);

    char *buffer = malloc(size + 1);
    if (!buffer) {
        fclose(fp);
        return NULL;
    }

    size_t bytes_read = fread(buffer, 1, size, fp);
    buffer[bytes_read] = '\0';
    fclose(fp);

    *out_size = (long)bytes_read;
    return buffer;
}

static void *worker(void *arg) {
    search_job_t *job = (search_job_t *)arg;

    while (1) {
        pthread_mutex_lock(&job->queue_mutex);
        if (job->next_file >= job->num_files) {
            pthread_mutex_unlock(&job->queue_mutex);
            break;
        }
        int index = job->next_file++;
        pthread_mutex_unlock(&job->queue_mutex);

        const char *path = job->files[index];
        long size = 0;
        char *content = read_whole_file(path, &size);
        long matches = content ? count_occurrences(content, job->keyword) : 0;
        free(content);

        pthread_mutex_lock(&job->output_mutex);
        fprintf(job->output_fp, "%s: %ld occurrence(s) of \"%s\"\n", path, matches, job->keyword);
        job->total_matches += matches;
        pthread_mutex_unlock(&job->output_mutex);
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "usage: %s keyword output.txt file1.txt [file2.txt ...] num_threads\n", argv[0]);
        return 1;
    }

    const char *keyword = argv[1];
    const char *output_path = argv[2];
    int num_files = argc - 4;
    char **files = &argv[3];
    int num_threads = atoi(argv[argc - 1]);

    if (num_threads < 1) {
        fprintf(stderr, "num_threads must be a positive integer\n");
        return 1;
    }
    if (num_threads > num_files) {
        num_threads = num_files;
    }

    FILE *output_fp = fopen(output_path, "w");
    if (!output_fp) {
        perror("fopen output");
        return 1;
    }

    search_job_t job = {
        .keyword = keyword,
        .files = files,
        .num_files = num_files,
        .next_file = 0,
        .total_matches = 0,
        .output_fp = output_fp,
    };
    pthread_mutex_init(&job.queue_mutex, NULL);
    pthread_mutex_init(&job.output_mutex, NULL);

    pthread_t *threads = malloc(num_threads * sizeof(pthread_t));

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, worker, &job);
    }
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double seconds = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    fprintf(output_fp, "TOTAL: %ld occurrence(s) of \"%s\" across %d file(s)\n",
            job.total_matches, keyword, num_files);

    fclose(output_fp);
    pthread_mutex_destroy(&job.queue_mutex);
    pthread_mutex_destroy(&job.output_mutex);
    free(threads);

    printf("search: %d thread(s) scanned %d file(s) in %.4f seconds, %ld total match(es)\n",
           num_threads, num_files, seconds, job.total_matches);

    return 0;
}
