# Q3: Multithreaded Prime Counter

## What it does

`prime_counter.c` counts the primes in `[1, 200000]` using 16 POSIX
threads. The workload is split with a divide-and-conquer partitioning
strategy rather than a shared work queue: the range is cut into 16 equal,
non-overlapping segments ahead of time, and each thread owns one segment
outright. A thread never touches another thread's numbers, so there is no
contention while the actual counting happens, only at the very end when
results are combined.

## Primality test: the 6k+1 optimization

Every integer greater than 3 can be written as `6k`, `6k+1`, `6k+2`,
`6k+3`, `6k+4`, or `6k+5`. Four of those six forms are always divisible by
2 or 3 (`6k`, `6k+2`, `6k+4` are even; `6k+3` is divisible by 3), so no
prime greater than 3 can have that form. Only `6k+1` and `6k+5` (which is
the same as `6k-1`) are ever prime. `is_prime()` uses this to skip
straight from one candidate divisor to the next:

```c
for (long i = 5; i * i <= n; i += 6) {
    if (n % i == 0 || n % (i + 2) == 0) return false;
}
```

Starting at `i = 5`, this checks `5, 7, 11, 13, 17, 19, ...`, i.e. exactly
the numbers of the form `6k+1` and `6k-1`, after a cheap upfront check
rules out even numbers and multiples of 3. Compared to naive trial
division (checking every integer from 2 to sqrt(n)), this cuts the number
of divisions by roughly two thirds, since only 2 out of every 6
candidates are tested instead of all 6.

To confirm the effect is real and not just theoretical, both versions
were built and timed single-threaded over a larger range (1 to
2,000,000, both agree on the count: 148,933 primes):

| Version               | Time    |
|-----------------------|---------|
| Naive trial division  | 0.429 s |
| 6k+1 optimization     | 0.149 s |

About a 2.9x speedup from the algorithmic change alone, before any
threading is involved.

## Divide-and-conquer workload split

```c
long span = RANGE_END - RANGE_START + 1;
long chunk = span / THREAD_COUNT;

ranges[i].start = RANGE_START + i * chunk;
ranges[i].end = (i == THREAD_COUNT - 1) ? RANGE_END : ranges[i].start + chunk - 1;
```

`200000 / 16 = 12500` exactly, so all 16 threads get an identical
12,500-number segment; the last thread's `end` is pinned to `RANGE_END`
to absorb any remainder if the range didn't divide evenly.

Each thread runs `count_range()`, which counts primes in its own segment
into a plain `long local_count` with no locking at all, since that
variable is private to the thread's stack. Only after the loop finishes
does the thread touch shared state:

```c
pthread_mutex_lock(&counter_mutex);
total_primes += local_count;
pthread_mutex_unlock(&counter_mutex);
```

This is the key design point: the mutex is acquired exactly once per
thread, for a single addition, instead of once per number tested. Locking
around every `total_primes++` (the naive approach) would force all 16
threads to serialize on the same lock 200,000 times combined, turning most
of the work back into a critical section. Locking once per thread turns
16 threads worth of contention into 16 uncontended lock/unlock pairs, so
the mutex overhead is negligible next to the actual prime-checking work.

## Build and run

```
gcc -Wall -O2 -o prime_counter prime_counter.c -lpthread
./prime_counter
```

## Result

```
The synchronized total number of prime numbers between 1 and 200,000 is 17984
```

17,984 is the correct value of pi(200000) (the prime-counting function
evaluated at 200,000), which confirms both the primality test and the
range partitioning are correct: no numbers were double-counted or
skipped across segment boundaries.

A note on timing: on modern hardware, counting primes up to 200,000 is
fast enough (a few milliseconds single-threaded) that 16-way parallelism
does not produce a visible wall-clock speedup here, thread creation and
join overhead is comparable to the work itself at this problem size. The
code's value at this size is in demonstrating correct workload division
and synchronized accumulation; the 6k+1 algorithmic optimization is what
does the actual, measurable work of making prime checking efficient (as
shown in the table above), and the same threading structure would show a
real speedup on a larger range where each thread's segment takes long
enough to dwarf its setup cost.
