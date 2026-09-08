# Coverage-Guided Fuzzer

A high-performance, multi-process coverage-guided fuzzer written in C, designed to automate software vulnerability detection. By leveraging LLVM/Clang sanitizers and a custom mutation engine, this testing tool tracks program execution paths to systematically generate inputs that trigger edge-case crashes and memory bugs.

## System Architecture

The fuzzer operates using a concurrent architecture to maximize testing throughput and safely isolate target program crashes:
* **Main Fuzzer & Input Queue:** Manages a dual-priority queue system to store and schedule inputs based on the novelty of their coverage data.
* **Runner Jobs:** Utilizes POSIX system calls (`fork`, `execvp`) to concurrently execute the target program in isolated child processes.
* **Inter-Process Communication (IPC):** Bidirectional communication is handled via Unix pipes for control flow, and memory-mapped shared memory (`mmap`, `shm_open`) for real-time coverage feedback.

## Core Features

* **Advanced Mutation Engine:** Implements a 37-strategy mutation cycle—including bit flipping, string injection, and truncation—to programmatically generate novel test cases from seed inputs.
* **Coverage-Feedback Tracking:** Employs a custom map utilizing both a bitmap (to track control-flow graph edges) and a hashset (to track unique execution paths) to filter out duplicate program behaviors.
* **Sanitizer Integration:** Built to interface directly with LLVM/Clang's AddressSanitizer (ASan), MemorySanitizer (MSan), and UndefinedBehaviorSanitizer (UBSan) to catch use-after-free errors, out-of-bounds accesses, and uninitialized variables.
* **Robust Process Control:** Features strict timeout enforcement via `SIGALRM` and comprehensive signal handling (`SIGCHLD`, `SIGTERM`) to gracefully manage infinite loops and prevent zombie processes.

## 🛠️ Build and Run

**Prerequisites**
Ensure you have `clang` and `libclang-rt-18-dev` installed, as the fuzzer requires LLVM's C compiler and coverage sanitizer headers. The target `PROGRAM` you intend to test must also be compiled and linked with the coverage sanitizer (`-fsanitize-coverage=trace-pc-guard`) and any other desired sanitizers (e.g., ASan, MSan).

**Compilation**
Compile the fuzzer executable utilizing the provided Makefile:
```bash
make
```
To compile with debugging symbols, use:
```bash
make debug
```

**Execution Format**
Start the fuzzer using the following command structure:
```bash
./bin/fuzzer [options] PROGRAM ARGS...
```
*Important: You must include the exact string `@@` as one of the arguments within `ARGS...`. This acts as a placeholder that the fuzzer will replace with the generated test inputs before execution.*

**Configuration Options**
Argument parsing processes options until an unrecognized argument is found, at which point all subsequent arguments are treated as the target program and its arguments.
* `-s <seed_file>`: **(Required)** Specifies the input file containing newline-separated initial seed inputs for the fuzzer.
* `-j <jobs>`: Sets the number of concurrent runner jobs to use (Default: 4).
* `-n <inputs>`: Sets the total number of mutated inputs to attempt, excluding the initial seeds (Default: 32).
* `-t <time_limit>`: Sets the time limit for the target program in seconds before it is forcefully terminated (Default: 5).
* `-h`: Prints the help message and exits with `EXIT_SUCCESS`.

**Execution Examples**
Run the fuzzer on a target program (`cascade`) using a required seed file (`inputs.txt`) with all default settings:
```bash
./bin/fuzzer -s inputs.txt ./programs/bin/cascade @@
```

Run the fuzzer with a custom configuration: 10 concurrent jobs, 100 total generated inputs, and a 50-second timeout for the target program:
```bash
./bin/fuzzer -j 10 -n 100 -s hello ./programs/bin/my_program -t 50 @@
```
*(Note: In the example above, the `-t 50` is placed after the target program, meaning it is passed as an argument to `my_program`, not parsed as the fuzzer's timeout option.)*
