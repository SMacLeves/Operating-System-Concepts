# Operating Systems Concepts — Assignments

Three independent C++/POSIX assignments for the **Operating Systems Concepts** course at Radboud University: a Unix shell, thread synchronization primitives, and a memory/cache-locality optimization exercise.

Author: Levente Bódi (s1122062)

## Contents

| File | Assignment | Report |
|---|---|---|
| [`src/shell.cpp`](src/shell.cpp) | Shell | [reports/shell_report.pdf](reports/shell_report.pdf) |
| [`src/synchronization.cpp`](src/synchronization.cpp) | Synchronization | [reports/synchronization_report.pdf](reports/synchronization_report.pdf) |
| [`src/memory.cpp`](src/memory.cpp) | Memory / cache locality | [reports/memory_report.pdf](reports/memory_report.pdf) |

### 1. Shell (`src/shell.cpp`)

A Unix shell implementation supporting **pipelines**, **I/O redirection** (`<`, `>`), **background jobs** (`&`), and the built-in `cd`. It parses a command line into an `Expression` (a list of `Command`s plus redirection/background flags), then executes it by walking the pipeline left-to-right: creating a `pipe()` between each pair of adjacent commands, `fork()`-ing once per command, wiring up `dup2()` for stdin/stdout in the child, and `execvp()`-ing the program. The parent tracks child PIDs and `waitpid()`s on them unless the job is backgrounded.

> Note: the course-provided driver `main()` that calls `shell()` was not part of the submitted files, so a minimal one was added at the bottom of `shell.cpp` for this repo so it builds standalone.

Build & run:
```bash
g++ -std=c++17 -Wall -o shell src/shell.cpp
./shell
```
Try e.g. `date | tail -c 5`, `echo hi > out.txt`, `cat &`, `cd ..`. See the report for the full manual test matrix (built-ins, pipelines, redirection, background jobs, error handling).

### 2. Synchronization (`src/synchronization.cpp`)

A thread-safe **bounded/unbounded buffer** (`add()`/`remove()`/`set_bound()`/`make_unbounded()`) whose operations are protected by a per-buffer `std::mutex`, plus a shared **`Logger`** that multiple buffers write to concurrently, protected by a `std::shared_mutex` (exclusive for writes, shared for reads). Every buffer operation logs its outcome (success/failure) to the shared logger. The file is self-contained and runs its own test suite (`T1`–`T9`, covering single-threaded correctness, bounded-capacity enforcement, runtime policy switching, producer/consumer concurrency, and logger read/write concurrency) from `main()`.

Build & run:
```bash
g++ -std=c++17 -Wall -pthread -o synchronization src/synchronization.cpp
./synchronization
```
Expected output: `TOTAL: 36 passed, 0 failed`.

### 3. Memory / Cache Locality (`src/memory.cpp`)

A performance-tuning exercise motivated by Poul-Henning Kamp's *"You're Doing It Wrong"* (on B-heaps and virtual-memory-aware algorithm design). Applies a 3x3 box filter to a large synthetic image and iteratively optimizes it for cache/page locality: switching to **row-major** traversal, precomputing constants, using **row pointers** to avoid recomputing `row*SIZE` indices, and unrolling the 3x3 sum. Reports `rusage`-based metrics (user/system/wall time, page faults, context switches) before and after each change.

Build & run:
```bash
g++ -std=c++17 -O2 -o memory src/memory.cpp
./memory
```
Result: user time dropped from ~32.1s to ~2.0s (~16x) with no hard page faults, purely from access-pattern changes — no change to the algorithm's Big-O complexity.

## Requirements

This is a C++ project — see [`requirements.txt`](requirements.txt). You need a C++17 compiler (`g++`/`clang++`) and a POSIX environment (Linux, macOS, or WSL on Windows) for `shell.cpp`'s use of `fork()`/`pipe()`/`waitpid()`.

## License

MIT — see [LICENSE](LICENSE).
