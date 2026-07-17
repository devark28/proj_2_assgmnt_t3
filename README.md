# Project overview

Four Linux systems programming exercises covering process/pipe IPC,
system-call-level file I/O, POSIX multithreading with synchronization,
and concurrent file processing. Each question lives in its own directory
with source code, a detailed README, and generated command output
(`outputs/`). All C code builds with a plain `gcc` command, no build
system is used.

## q1_pipeline_ipc

`pipeline.c` reimplements the shell pipeline `ps aux | grep root` using
`fork()`, `execvp()`, and `pipe()` directly instead of a shell: one child
execs `ps aux` with its stdout wired to the pipe's write end, a second
child execs `grep root` with its stdin wired to the pipe's read end and
its stdout redirected into a file, and the parent waits for both before
reading back and previewing the captured output.

```
gcc -Wall -o pipeline pipeline.c
./pipeline
strace -f -tt -o outputs/strace_trace.txt ./pipeline
```

In [q1_pipeline_ipc/README.md](q1_pipeline_ipc/README.md) there are
details of a full walkthrough of the strace output, including how
`execvp()` performs a `$PATH` search as a sequence of `execve()`
attempts, and why the two `wait4()` calls are what guarantee the output
file is fully written before the parent reads it back.

## q2_file_copy_io

Two file-copy utilities compared on a 150 MB file:

- `copy_syscall.c`, raw `open`/`read`/`write`/`close`
- `copy_stdio.c`, buffered `fopen`/`fread`/`fwrite`/`fclose`

```
gcc -Wall -O2 -o copy_syscall copy_syscall.c
gcc -Wall -O2 -o copy_stdio copy_stdio.c
dd if=/dev/urandom of=testfile.bin bs=1M count=150
./copy_syscall testfile.bin copy_syscall_out.bin
./copy_stdio testfile.bin copy_stdio_out.bin
strace -c -o outputs/strace_copy_syscall_summary.txt ./copy_syscall testfile.bin copy_syscall_out.bin
strace -c -o outputs/strace_copy_stdio_summary.txt ./copy_stdio testfile.bin copy_stdio_out.bin
```

In [q2_file_copy_io/README.md](q2_file_copy_io/README.md) there are
details of the full syscall-count comparison and timing results.

## q3_prime_counter

`prime_counter.c` counts the primes between 1 and 200,000 across 16
POSIX threads. The range is split into 16 equal segments up front (true
divide-and-conquer, not a shared queue); each thread counts its own
segment into a local variable and only takes `counter_mutex` once, at the
very end, to add its local count into the shared total. Primality testing
uses the 6k+1 trial-division optimization, which only tests divisors of
the form 6k+1 and 6k-1.

```
gcc -Wall -O2 -o prime_counter prime_counter.c -lpthread
./prime_counter
```

Output:

```
The synchronized total number of prime numbers between 1 and 200,000 is 17984
```

In [q3_prime_counter/README.md](q3_prime_counter/README.md) there are
details of the 6k+1 explanation and a measured comparison against naive
trial division (about 2.9x faster on a larger range).

## q4_keyword_search

`search.c` searches a keyword across multiple text files with a pool of
worker threads pulling from a shared, mutex-protected work queue (one
file per claim), and writes a per-file match count plus a grand total
into a shared output file, with a second mutex protecting those writes.

```
./search keyword output.txt file1.txt file2.txt ... <number_of_threads>
```

Test data (16 files seeded with a random number of keyword occurrences
each) is generated with `python3 gen_test_data.py`. Performance was
measured at 2 threads, 8 threads (this machine's core count), and 16
threads (one thread per file); all three produced the same 109-match
total, confirming correctness independent of thread count.

In [q4_keyword_search/README.md](q4_keyword_search/README.md) there are
details of the full design (queue-based work distribution, whole-file
reads to avoid boundary-split matches, and why more threads was not
faster on a dataset this small).
