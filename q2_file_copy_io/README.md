# Q2: System Calls vs Standard I/O for Large File Copy

## What it does

Two file-copy utilities, built from the same 1024-byte buffer size and
the same copy loop shape, differing only in which I/O layer they call:

- `copy_syscall.c`, raw `open()`/`read()`/`write()`/`close()`
- `copy_stdio.c`, buffered `fopen()`/`fread()`/`fwrite()`/`fclose()`

Both take `<source> <destination>`, copy the file, and print the byte
count and elapsed time (measured with `clock_gettime(CLOCK_MONOTONIC)`
around the whole copy loop, including the final `close()`/`fclose()`).

## Test file and build

```
gcc -Wall -O2 -o copy_syscall copy_syscall.c
gcc -Wall -O2 -o copy_stdio copy_stdio.c
dd if=/dev/urandom of=testfile.bin bs=1M count=150
./copy_syscall testfile.bin copy_syscall_out.bin
./copy_stdio testfile.bin copy_stdio_out.bin
cmp testfile.bin copy_syscall_out.bin
cmp testfile.bin copy_stdio_out.bin
```

150 MB satisfies the "at least 100 MB" requirement. `cmp` confirms both
copies are byte-identical to the source.

## Why the buffer is 1024 bytes on both sides

Both programs read and write in 1024-byte chunks on purpose, not a large
one like 64 KB. glibc's `stdio` layer keeps its own internal buffer per
`FILE*`, sized `BUFSIZ`, 4096 bytes on this system. When a `fread()`
request is smaller than that internal buffer, `stdio` doesn't issue one
`read()` syscall per `fread()` call: it fills its 4096-byte internal
buffer with a single `read()`, then serves up to four 1024-byte
`fread()` calls out of that buffer before it needs another `read()`. A
1024-byte request is exactly the size needed to make that 4-to-1
amortization visible in a syscall count comparison; a buffer already as
large as or larger than `BUFSIZ` would make `stdio` bypass its internal
buffer entirely and the two versions would look nearly identical under
strace.

## Counting system calls with strace -c

```
strace -c -o outputs/strace_copy_syscall_summary.txt ./copy_syscall testfile.bin copy_syscall_out.bin
strace -c -o outputs/strace_copy_stdio_summary.txt ./copy_stdio testfile.bin copy_stdio_out.bin
```

| Syscall   | copy_syscall calls | copy_stdio calls | Ratio |
|-----------|--------------------:|------------------:|------:|
| `read`    | 153,602             | 38,402             | 4.00x |
| `write`   | 153,601             | 38,401             | 4.00x |
| `close`   | 4                   | 4                  | 1x    |
| `openat`  | 4                   | 4                  | 1x    |
| **Total** | **307,239**         | **76,841**         | **4.0x** |

This is exactly the 4x amortization the 1024-vs-4096 buffer mismatch
predicts: `copy_stdio` issues almost precisely a quarter of the `read`
and `write` syscalls that `copy_syscall` does, for the same 150 MB of
data. The `openat`/`close` counts are identical since both programs open
and close exactly one source and one destination file, `stdio` doesn't
add any extra file-level syscalls here, its savings are entirely in
collapsing many small `read`/`write` calls into fewer, larger ones under
the hood.

This is the concrete cost of the raw syscall version's design: every
single 1024-byte `read()`/`write()` call the program makes is a real
user-to-kernel context switch, cheap per call (this system: roughly 2
microseconds average per `strace -c`'s own CPU-time accounting) but not
free, and it adds up linearly with call count. `stdio` pays that same
per-syscall cost only a quarter as often, at the expense of one extra
in-process `memcpy()` per `fread()`/`fwrite()` call to move bytes between
its internal buffer and the caller's buffer, overhead that never leaves
the CPU and never touches the kernel.

## Timing results

Clean single runs on freshly written data:

| Version        | Time     |
|----------------|----------|
| `copy_syscall` | 0.1444 s |
| `copy_stdio`   | 0.0978 s |

`copy_stdio` was faster here, consistent with issuing 4x fewer syscalls
for the same amount of data copied.

The **syscall count comparison above is the more reliable evidence** of
the two: it depends only on buffer sizes and glibc's documented
buffering behavior, so it holds regardless of whatever else the machine
happens to be doing at the moment a given copy runs, while a single
wall-clock timing sample can shift run to run.
