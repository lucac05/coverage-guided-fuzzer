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

## Usage & Configuration

The fuzzer accepts an initial seed file and executes a specified number of mutated inputs across concurrent jobs.

* **Basic Execution:** Run the fuzzer with a seed file, specifying the target program and using `@@` as the input placeholder.
* **Configuration Flags:** Customize the execution using `-j` for concurrent jobs, `-n` for the total number of inputs to generate, and `-t` for the target program timeout limit in seconds.
