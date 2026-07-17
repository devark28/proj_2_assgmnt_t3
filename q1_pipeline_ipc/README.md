# Q1: Process Pipeline with fork, execvp and pipe

## What it does

`pipeline.c` rebuilds the shell pipeline `ps aux | grep root` at the system
call level instead of letting a shell do it. It:

1. Creates one `pipe()` with a read end and a write end.
2. `fork()`s a producer child that redirects its stdout onto the pipe's
   write end and `execvp()`s into `ps aux`.
3. `fork()`s a consumer child that redirects its stdin from the pipe's read
   end, redirects its stdout to `outputs/pipeline_output.txt`, and
   `execvp()`s into `grep root`.
4. The parent closes both ends of the pipe (it never touches the data
   itself), waits for both children with `waitpid()`, then opens the
   output file it just watched get created and prints the first 5 lines
   to the terminal.

No shell is involved, the parent process wires the file descriptors
directly with `dup2()`, which is exactly what a shell does internally when
it sets up a pipeline.

## Build and run

```
gcc -Wall -o pipeline pipeline.c
./pipeline
```

Run it from inside `q1_pipeline_ipc/` so the relative `outputs/` path
resolves. The program prints where the full output landed and previews
the first 5 lines.

## Tracing with strace

```
strace -f -tt -o outputs/strace_trace.txt ./pipeline
```

`-f` follows child processes across `fork()`, which is required here since
the interesting work happens in the two children, not the parent. `-tt`
adds microsecond timestamps so the interleaving between the three
processes is visible. The full trace is about 6,300 lines because it also
covers dynamic linker activity (`mmap`, `openat` on shared libraries) for
all three processes; `outputs/strace_filtered.txt` strips that down to the
syscalls that matter for this exercise: `pipe2`, `clone`, `dup2`,
`execve`, `wait4`, and the exit/`SIGCHLD` notifications.

## Reading the syscall sequence

The filtered trace tells a clear story, in order:

**1. Pipe creation.** `pipe2([3, 4], 0)` in the parent creates file
descriptors 3 (read end) and 4 (write end) before either child exists, so
both children inherit copies of both ends across `fork()`.

**2. Two clones.** Linux implements `fork()` as `clone()` with
`SIGCHLD` as the exit signal. The trace shows two `clone()` calls from the
parent (PID 35080), producing the producer (35081) and consumer (35082).

**3. Descriptor wiring happens before exec.** Each child calls `dup2()`
to overwrite its own stdin/stdout with its half of the pipe *before*
calling `execvp()`. This is the mechanism that makes the redirection
survive the exec: file descriptors are per-process kernel state and
`execvp()` replaces the program image but not the open file table.

**4. execvp does a PATH search.** The trace shows the consumer trying
`execve()` against `/home/pc/Android/Sdk/platform-tools/grep`,
`~/.local/share/pnpm/grep`, `~/.cargo/bin/grep`, and several more
candidates before it succeeds on `/usr/bin/grep`. This is `execvp()`'s
documented behavior: because the command name has no `/` in it, libc walks
every directory in `$PATH` and issues one `execve()` per candidate until
one doesn't fail with `ENOENT`. `ps` in the producer shows the same
pattern. This is worth noting in a report: `execvp()` is not a single
syscall, it is a libc loop around the `execve()` syscall.

**5. Output file creation.** The consumer's `openat(AT_FDCWD,
"outputs/pipeline_output.txt", O_WRONLY|O_CREAT|O_TRUNC, 0644)` happens
after its `dup2()` calls but before its `execvp()` loop resolves, since
the program sets up all its redirections first. This is the "capture
output into a file" requirement: the file descriptor returned here gets
`dup2()`'d onto the consumer's stdout, so everything `grep` prints goes
straight to disk instead of the terminal.

**6. Synchronization.** The parent calls `wait4()` twice, once per child,
and each call blocks until the corresponding `+++ exited with 0 +++` /
`SIGCHLD` pair appears in the trace. This is what guarantees the parent
does not try to read `pipeline_output.txt` before `grep` has finished
writing it. Without these two `wait4()` calls the parent could race the
consumer and read a partially written or empty file.

**7. Final read.** Only after both children have exited does the parent
issue its own `openat(..., O_RDONLY)` on the same file, confirming the
read-back happens strictly after the write is complete.

## Result

`ps aux | grep root` on this machine matched 208 lines, all captured in
`outputs/pipeline_output.txt`. The terminal preview (first 5 lines) shows
`init` and several kernel worker threads owned by `root`, which is
expected since `ps aux` lists every process on the system and `root` owns
the bulk of the kernel/service processes.
