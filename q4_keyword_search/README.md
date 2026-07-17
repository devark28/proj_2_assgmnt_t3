# Q4: Multithreaded Keyword Search

## What it does

`search.c` searches for a keyword across a list of text files using a
pool of worker threads and writes a per-file match count into a shared
output file, plus a final total.

```
./search keyword output.txt file1.txt file2.txt ... <number_of_threads>
```

`argv` is parsed positionally: `argv[1]` is the keyword, `argv[2]` is the
output path, everything between that and the last argument is a file to
search, and the last argument is the thread count.

## Work distribution: a shared queue, not a fixed assignment

Each thread processes one file, which is easy when thread count equals
file count, but the program also has to run sensibly at 2 threads
against 16 files. Rather than writing two code paths, `search.c` always
uses a shared work queue: an integer index `next_file` protected by
`queue_mutex`. Each thread loops: lock, claim the current index and
advance it, unlock, then process that file outside the
lock.

```c
pthread_mutex_lock(&job->queue_mutex);
if (job->next_file >= job->num_files) {
    pthread_mutex_unlock(&job->queue_mutex);
    break;
}
int index = job->next_file++;
pthread_mutex_unlock(&job->queue_mutex);
```

The lock is held only long enough to read and increment one integer, the
actual file reading and string search happen with no lock at all, since
each thread works on its own `content` buffer. When `num_threads >=
num_files` (the "maximum threads" case, one thread per file), each thread
claims exactly one index before the queue empties, so each thread really
does process exactly one file. When
`num_threads < num_files`, threads pull more than one file each,
correctly and safely, off the same queue.

## File handling and counting

Each file is read whole with `fopen`/`fseek`/`ftell`/`fread` into a
heap buffer, null-terminated, then scanned with a `strstr()` loop that
advances past each match (non-overlapping counting, the same convention
`grep -o` uses):

```c
while ((cursor = strstr(cursor, keyword)) != NULL) {
    count++;
    cursor += keyword_len;
}
```

Reading each file in one shot instead of streaming through fixed-size
chunks avoids a subtle bug: a keyword that straddles a chunk boundary
would otherwise be missed by a naive chunked search. Since the test files
here are tens of kilobytes, holding one at a time in memory per thread is
not a concern.

## Protecting the shared output file

A second mutex, `output_mutex`, guards both `fprintf()` into the shared
output file and the running `total_matches` accumulator:

```c
pthread_mutex_lock(&job->output_mutex);
fprintf(job->output_fp, "%s: %ld occurrence(s) of \"%s\"\n", path, matches, job->keyword);
job->total_matches += matches;
pthread_mutex_unlock(&job->output_mutex);
```

Two separate mutexes are used, one for the work queue and one for the
output file, rather than a single lock for both, because they protect
unrelated critical sections: a thread claiming its next file has nothing
to do with a (different) thread writing its previous result, and forcing
them through one lock would serialize work-claiming behind whatever
thread happens to be mid-write.

## Test data

`gen_test_data.py` builds 16 files of ~6,000 random dictionary words
each (`data/file01.txt` ... `data/file16.txt`) and seeds the keyword
`kernel` into a random number of positions per file (0 to 12), so file
sizes and match counts are realistic and uneven rather than uniform.
Words are wrapped 15 per line using list slicing, not raw character
slicing, so the keyword can never end up split across a line break.

```
python3 gen_test_data.py
```

## Build and run

```
gcc -Wall -O2 -o search search.c -lpthread
./search kernel outputs/results.txt data/*.txt 8
```

## Performance across thread counts

All three configurations were run against the same 16 files and produced
the same total, 109 matches, confirming the queue-based split does not
lose or double-count files at any thread count:

| Threads                 | Time     | Total matches |
|-------------------------|----------|----------------|
| 2                       | 0.0004 s | 109            |
| 8 (average core count)  | 0.0011 s | 109            |
| 16 (maximum, one/file)  | 0.0016 s | 109            |

This machine has 8 logical cores (`nproc`). Counterintuitively, more
threads is slightly slower here, not faster. The dataset is small (16
files, about 55 KB each), so the total work per thread is a handful of
`fread()` calls and a `strstr()` scan over ~55,000 bytes, work that
finishes in well under a millisecond. Against that, `pthread_create()`
and `pthread_join()` each carry real, fixed kernel overhead (roughly
tens of microseconds per thread). At 16 threads, thread setup cost
dominates the actual search time; at 2 threads, that overhead is paid
twice instead of sixteen times, so it wins. This is expected and
consistent with the same finding in Q3: threading pays off when the
per-thread workload is large enough to amortize its own setup cost, and
this program is correct at every thread count regardless of whether that
condition holds.
