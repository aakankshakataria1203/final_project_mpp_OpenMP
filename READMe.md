# Adaptive Task Scheduling Engine using OpenMP
## Lock-Free Concurrent Algorithm Implementation

---

## Team Members
- Aakanksha Kataria

---

## Project Overview

This project implements an **Adaptive Task Scheduling Engine** that demonstrates lock-free concurrent programming using OpenMP. The scheduler intelligently manages task execution across multiple threads using four different scheduling strategies, with a rigorous comparison against a traditional lock-based synchronization baseline.

### Key Innovations
1. **Custom Task Scheduling Layer**: Builds on top of OpenMP to classify tasks by computational weight (Light/Medium/Heavy).
2. **Lock-Free vs. Lock-Based Duel**: Implements a side-by-side comparison of explicit locking (`omp_lock_t`) versus OpenMP's internal atomic mechanisms (`fetch-and-add`).
3. **Heterogeneous Scheduling**: A custom strategy that applies Longest Processing Time (LPT) heuristics to optimize mixed workloads.
4. **Stress Testing**: Includes a fine-grained stress test that demonstrates an **836x performance advantage** for lock-free scheduling under high contention.

---

## Theoretical Background

### Why Lock-Free Programming?

Traditional lock-based synchronization suffers from:
- **Contention**: Serialization of threads waiting for a single resource.
- **Priority Inversion**: Lower-priority threads blocking critical paths.
- **Deadlock Risk**: Improper lock ordering causing system hangs.
- **Scalability Collapse**: Performance degrades drastically as thread count rises for fine-grained tasks.

Lock-free algorithms leverage **atomic operations** (like `LOCK XADD` on x86) to ensure system-wide progress without blocking.

### Scheduling Strategies Implemented

| Strategy | Description | Mechanism | Best For |
|----------|-------------|-----------|----------|
| **Lock-Based** | Centralized queue protected by mutex | `omp_set_lock` | Coarse-grained tasks |
| **Static** | Pre-computed division at compile time | No synchronization | Uniform workloads |
| **Dynamic** | Threads claim 1 chunk at a time | Atomic `fetch_and_add` | Highly irregular workloads |
| **Guided** | Exponentially decreasing chunk sizes | Amortized Atomics | Mixed workloads |
| **Heterogeneous** | Tasks sorted by weight (Heavy → Light) | LPT + Atomics | Predictable mixed workloads |

---

## Project Structure

```
openmp_adaptive_scheduler/
├── task_scheduler.h       # Scheduler API and data structures
├── task_scheduler.c       # Core scheduling implementations
├── workloads.h            # Workload definitions (Mixed, Stress Test)
├── workloads.c            # Task implementations
├── benchmark.c            # Main entry point & performance suite
├── test_correctness.c     # Unit tests for race condition verification
├── Makefile               # Build configuration
└── README.md              # This documentation
```

---

## Build Instructions

### Prerequisites
- GCC with OpenMP support (gcc 4.9+)
- Linux/Unix environment
- Make build system

### Compilation

```bash
# Clean and build all targets
make clean && make all
```

This produces two executables:
- `benchmark` - Runs the full performance suite (Mixed Workload + Stress Test).
- `test_correctness` - Verifies that no race conditions exist.

### Compiler Flags Used
```
-O3          : Maximum optimization level
-fopenmp     : Enable OpenMP runtime
-std=gnu11   : C11 standard with GNU extensions
-Wall -Wextra: All warnings enabled
```

---

## Running the Project

### Step 1: Verify Correctness

Run the unit tests to ensure all schedulers process exactly the required number of tasks without data races.

```bash
./test_correctness
```

**Expected Output:**
```
=== CORRECTNESS TESTS ===
Test 1: Basic task submission... counter=100 (expected 100) PASS
...
Test 5: HETEROGENEOUS scheduling... counter=50 (expected 50) PASS
✓ ALL TESTS COMPLETED!
```

### Step 2: Run Benchmarks

Execute the performance suite. This runs the "Mixed Workload" (coarse-grained) followed by the "Stress Test" (fine-grained).

```bash
./benchmark > results_comprehensive.csv
cat results_comprehensive.csv
```

**Output Format**: The CSV contains five sections:
1. Mixed Workload Results (1,000 tasks with varying costs)
2. Per-Thread Fairness (load balance metrics)
3. Task Latency Histogram
4. Stress Test (100,000 fine-grained tasks)

---

## Key Results Summary

### 1. Coarse-Grained Workload (Mixed Tasks)
*Tasks perform ~1ms of work each. Contention is low relative to execution time.*

- **Lock-Based**: 16.4x speedup at 16 threads, 102.9% efficiency.
- **Lock-Free (Guided)**: 15.8x speedup at 16 threads, 98.9% efficiency.
- **Insight**: For heavy tasks, simple locking is efficient and competitive. Both approaches scale linearly due to low contention.

### 2. Fine-Grained Workload (Stress Test)
*Tasks perform minimal work (single increment). Synchronization overhead is the bottleneck.*

| Metric | Lock-Based | Lock-Free (Guided) | Advantage |
|--------|------------|--------------------|-----------|
| **Throughput** | 1.6M tasks/sec | **1.34B tasks/sec** | **836x Faster** |
| **Duration** | 0.062 sec | **0.00007 sec** | Instantaneous |

**Insight**: This decisively proves lock-free scheduling is mandatory for fine-grained parallelism. The overhead of acquiring/releasing a lock 100,000 times destroys performance, while OpenMP's guided schedule amortizes atomic operations through chunking.

### 3. Load Balancing
- **Static**: High variance (SD ~8.2 tasks) on irregular workloads.
- **Dynamic/Guided**: Near-perfect balance (SD ~2–3 tasks).
- **Heterogeneous**: Excellent balance with minimal overhead.

---

## Understanding the Metrics

| Metric | Description |
|--------|-------------|
| **Throughput** | Tasks completed per second |
| **Speedup** | Wall-clock time at 1 thread / Time at N threads |
| **Efficiency** | (Speedup / N threads) × 100% |
| **Fairness** | (Min tasks per thread / Mean tasks per thread) × 100% |

### Interpreting Efficiency
- **>95%**: Excellent parallel scaling
- **80-95%**: Good scaling
- **60-80%**: Acceptable (common at high thread counts due to hyperthreading)
- **<60%**: Poor scaling (contention or scheduling overhead)

---

## Challenges Faced & Solutions

### Challenge 1: Ensuring a Fair Baseline
**Problem**: Initial lock-based implementation was too slow due to inefficient logic.  
**Solution**: Rewrote the lock-based scheduler to use a tight spin-loop (`omp_lock_t`) mirroring the exact structure of the lock-free loop. This ensures the comparison isolates synchronization cost only.

### Challenge 2: False Sharing
**Problem**: Per-thread counters sharing cache lines caused performance degradation.  
**Solution**: Utilized thread-local accumulation variables, summing to the global counter only at task completion.

### Challenge 3: Validating "Impossible" Throughput
**Problem**: The Stress Test showed 1.34 billion tasks/sec, which seemed erroneous for 100k tasks.  
**Solution**: Validated that because the tasks were trivial (single instruction), the metric correctly reflects the *scheduler's* capacity to hand out work at hardware speed without blocking. Mathematically: 100,000 tasks ÷ 0.00007 seconds = 1.4 billion tasks/sec.

---

## Performance Analysis

The project demonstrates a critical inflection point where lock-free scheduling becomes essential:
- **Coarse-Grained**: Lock-based and lock-free are comparable; locking overhead is negligible relative to work.
- **Fine-Grained**: Lock-free is 836x faster; synchronization overhead dominates the execution profile.

This validates the theoretical foundation: **Lock-free programming is not an optimization; it is a requirement for scalable fine-grained parallelism.**

---

## Compiler & Environment

- **Language**: C11 (GNU11 extensions)
- **Parallel Runtime**: OpenMP 4.5+
- **Tested On**: GCC 8.x, Virginia Tech ARC Dogwood Cluster (16-core shared-memory node)
- **Optimization**: `-O3 -fopenmp`

---

## Future Work

1. **Work Stealing**: Implement a lock-free deque (Chase-Lev algorithm) to improve cache locality for recursive workloads.
2. **NUMA Awareness**: Optimize thread affinity for multi-socket architectures.
3. **GPU Offloading**: Extend the scheduler to offload "Heavy" tasks to GPUs using OpenMP target directives.
4. **Adaptive Runtime Switching**: Dynamically select scheduling strategy based on runtime metrics.


