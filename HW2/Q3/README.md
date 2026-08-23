# Q3: Distributed Bitonic Sort using MPI

## Overview
This module implements a scalable distributed **Bitonic Sort** using MPI in C++17. 
Given a sequence of $N$ elements partitioned equally among $P$ processes ($N$ and $P$ are both powers of 2), the algorithm coordinates sorting using a parallel bitonic sorting network across a hypercube topology.

---

## Algorithm Architecture

1. **Scatter**: Rank 0 scatters equal chunks of size $K = N / P$ to all $P$ processes using `MPI_Scatter`.
2. **Initial Local Sort**:
   - Depending on whether the process block is configured for ascending or descending in Stage 1:
     - Rank with `(rank >> 1 & 1) == (rank & 1)` sorts locally in **Ascending** order.
     - Otherwise sorts locally in **Descending** order.
   - Pairs of processes now form bitonic sequences of size $2K$.
3. **Hypercube Bitonic Network Stages**:
   - For each outer stage $k = 1 \dots \log_2(P)$:
     - Determine outer stage direction `dir = ((rank >> k) & 1) == 0 ? ASCENDING : DESCENDING`.
     - For inner steps $j = k-1 \dots 0$:
       - Partner rank: `partner = rank ^ (1 << j)`.
       - Exchange local chunk with `partner` using `MPI_Sendrecv`.
       - Perform position-wise compare: $\text{low} = \min(\text{local}[i], \text{recv}[i])$ and $\text{high} = \max(\text{local}[i], \text{recv}[i])$.
       - Retain either `low` or `high` depending on rank relation (`rank < partner` vs `rank > partner`) and stage direction `dir`.
       - Re-sort local chunk in direction `dir` using `std::sort`.
4. **Gather**: `MPI_Gather` collects all sorted chunks back to Rank 0 into a globally sorted array.
5. **Verification**: Rank 0 verifies the result against sequential `std::sort` for 100% mathematical correctness.

---

## Compilation

```bash
mpicxx -O2 -std=c++17 -Wall -Wextra -pedantic bitonic.cpp -o build/bitonic
```

---

## Execution Instructions

### 1. From Input File:
```bash
mpirun -np <P> ./build/bitonic <input_file> [--print] [--benchmark]
```
Example:
```bash
mpirun -np 2 ./build/bitonic build/sample_n4.txt --print
# Output: 1 2 3 4
```

### 2. Direct In-Memory Generation (Fast for Large Benchmarks):
```bash
mpirun -np <P> ./build/bitonic <N> [seed] [--benchmark]
```
Example:
```bash
mpirun -np 4 ./build/bitonic 1048576 42 --benchmark
```

### 3. Slurm Execution on HPC Clusters:
```bash
srun -n 4 ./build/bitonic 16777216 42 --benchmark
```

---

## Automated Verification and Benchmarking

To run the complete automated test suite (checking PDF example cases, edge cases, and scaling from Small to Very Large inputs across $P \in \{1, 2, 4, 8\}$):

```bash
bash run.sh
```

Benchmark metrics (total time, computation time, communication time, speedup $S(P)$, and efficiency $E(P)$) are saved to `results/benchmark_<timestamp>.csv`.
