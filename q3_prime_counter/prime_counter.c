#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>

#define RANGE_START 1
#define RANGE_END 200000
#define THREAD_COUNT 16

static long total_primes = 0;
static pthread_mutex_t counter_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    long start;
    long end;
} range_t;

static bool is_prime(long n) {
    if (n < 2) return false;
    if (n == 2 || n == 3) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;

    for (long i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0) return false;
    }
    return true;
}

static void *count_range(void *arg) {
    range_t *range = (range_t *)arg;
    long local_count = 0;

    for (long n = range->start; n <= range->end; n++) {
        if (is_prime(n)) local_count++;
    }

    pthread_mutex_lock(&counter_mutex);
    total_primes += local_count;
    pthread_mutex_unlock(&counter_mutex);

    return NULL;
}

int main(void) {
    pthread_t threads[THREAD_COUNT];
    range_t ranges[THREAD_COUNT];

    long span = RANGE_END - RANGE_START + 1;
    long chunk = span / THREAD_COUNT;

    for (int i = 0; i < THREAD_COUNT; i++) {
        ranges[i].start = RANGE_START + i * chunk;
        ranges[i].end = (i == THREAD_COUNT - 1) ? RANGE_END : ranges[i].start + chunk - 1;

        if (pthread_create(&threads[i], NULL, count_range, &ranges[i]) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_join(threads[i], NULL);
    }

    pthread_mutex_destroy(&counter_mutex);

    printf("The synchronized total number of prime numbers between 1 and 200,000 is %ld\n",
           total_primes);

    return 0;
}
