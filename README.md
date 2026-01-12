# Multithreaded Word Count (mwc)

`mwc` is a POSIX-based **multithreaded word count program written in C**. It is designed as a learning-focused systems project that explores:

* File I/O using `pread`
* Correct word counting across chunk boundaries
* Thread-level parallelism with `pthreads`
* Avoiding shared mutable state
* Benchmarking and performance considerations

The project is conceptually similar to `wc -w`, but is intentionally implemented from first principles to understand *how* such tools work internally.

---

## Problem Statement

Given a large text file, count the total number of words using multiple threads.

A **word** is defined as a contiguous sequence of non-whitespace characters. Any whitespace character (space, newline, tab, etc.) is treated as a delimiter.

The key challenge is:

> How do you split a file across multiple threads **without double-counting or missing words** at chunk boundaries?

---

## Design Overview

### High-level approach

1. The file is split into **byte-range chunks** based on the number of available CPU cores.
2. Each chunk is processed by a separate worker thread.
3. Threads read their assigned file regions independently using `pread`.
4. Each thread produces a local word count.
5. The main thread joins all workers and aggregates the results.

No thread shares mutable state with another thread.

---

## Word Counting Strategy

Instead of trying to detect word *ends*, this project counts **word starts**.

### Core rule

> A word is counted **only when its first character is encountered**.

This is implemented using the following logic:

* A character starts a word if:

    * It is **not whitespace**, and
    * The previous character **is whitespace** (or the character is at file offset 0)

### Why this works

* Each word has exactly one starting character
* Words spanning multiple buffers or chunks are counted exactly once
* No need to peek into the next chunk

This approach completely avoids complex boundary-adjustment logic.

---

## Handling Chunk Boundaries

Each worker thread is given:

* `offset`: starting byte offset in the file
* `size`: number of bytes it is responsible for

To correctly handle the first word in a chunk:

* If `offset == 0`, the thread may count the first word
* Otherwise, the thread inspects the **previous byte** (`offset - 1`) to determine whether the current position starts a new word or is a continuation

This guarantees:

* No double-counting
* No missing words

---

## Concurrency Model

* Threads are created using `pthread_create`
* Each thread:

    * Reads from the file using `pread`
    * Maintains its own local buffer
    * Produces a local word count

### Why `pread`?

* `pread` reads from an explicit file offset
* It does **not** modify the file descriptor's shared offset
* Multiple threads can safely read from the same file descriptor concurrently

No locks or synchronization primitives are required.

---

## File I/O

* File size is determined using `stat`
* The file is logically divided into chunks based on CPU core count
* Each thread reads its chunk incrementally using a fixed-size buffer (`READ_BUFFER_SIZE`)

The program **never loads the entire file into memory**. It streams data in small buffers, making it suitable for very large files.

---

## Building

Compile with a POSIX-compliant compiler:

```bash
gcc -O2 -pthread mwc.c -o mwc
```

Optimization (`-O2`) is recommended for realistic performance measurements.

---

## Usage

```c
long count = mwc("large_file.txt");
```

The `mwc` function:

* Opens the file
* Spawns worker threads
* Waits for completion
* Returns the total word count

---

## Learning Goals

This project is intended to explore:

* Correct parallelization of sequential data
* Thread-safe file I/O in POSIX systems
* Boundary conditions in text processing

---

## License

This project is provided for educational purposes. Use and modify freely.
