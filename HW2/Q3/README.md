## Q3: Bitonic Sort

This solution sorts a sequence of $N$ integer elements across $P$ MPI processes using the Distributed Bitonic Sort algorithm. Each process receives an equal-sized local chunk of $N/P$ elements. The algorithm coordinates pairwise compare-exchange operations over hypercube network stages, followed by local sorting, to produce the globally sorted sequence.

### Input format

```text
N
x1 x2 x3 ... xN
```

$N$ and $P$ must both be powers of 2, with $N \ge P$. The input contains integer values.

### Compile and run

Build all Q3 programs with the Makefile:

```bash
make all
```

This creates `build/bitonic_seq`, `build/bitonic_mpi`, and `build/generate_data`.

Compile and run the sequential reference program with:

```bash
g++ -O2 -std=c++17 bitonic_seq.cpp -o build/bitonic_seq
./build/bitonic_seq example.txt
```

Compile and run the MPI program with:

```bash
mpicxx -O2 -std=c++17 bitonic_mpi.cpp -o build/bitonic_mpi
mpirun -np 4 ./build/bitonic_mpi example.txt
```

The program prints the space-separated sorted sequence to standard output. Detailed MPI phase timings (setup, scatter, initial local sort, stage communication, stage computation, gather, total computation, total communication, and overall algorithm time) are printed to standard error with the `MPI_PHASES` prefix.

### Reproducible tests and benchmarks

Run the benchmark suite from this directory:

```bash
bash run.sh
```

The script generates these fixed-seed cases:

| Case | Elements ($N$) | Description / Seed |
| --- | ---: | --- |
| PDF sample 1 | 4 | `4 1 3 2` (expected: `1 2 3 4`) |
| PDF sample 2 | 8 | `5 3 8 1 9 2 7 4` (expected: `1 2 3 4 5 7 8 9`) |
| Edge case | 8 | Seed 42 |
| Small 64 | 64 | Seed 43 |
| Medium 1024 | 1,024 | Seed 44 |
| Large 65K | 65,536 | Seed 45 |
| Large maximum | 1,048,576 | Seed 46 |
| Very large | 4,194,304 | Seed 47 |

Each case is verified against sequential sorting and run with MPI using $P \in \{1, 2, 4, 8\}$. Small cases use repeated runs to reduce measurement noise. Benchmark CSV files record high-resolution wall time, MPI algorithm time, individual phase breakdowns, wall/algorithm speedups, efficiencies, and correctness status (`PASS`).

### Analyze results and create plots

Run the Python analysis script after a benchmark:

```bash
python3 scripts/analyze_benchmark.py
```

The script reads the newest benchmark CSV in `results/`, validates that all process counts pass verification, and generates:

- `analysis_detail.csv` - comprehensive benchmark metrics table
- `analysis_summary.csv` - one summary row per dataset
- `analysis_report.txt` - textual performance breakdown
- `speedup_plot.svg` - wall-clock speedup plot
- `algo_speedup_plot.svg` - algorithm-only speedup plot
- `efficiency_plot.svg` - efficiency plot
- `mpi_runtime_plot.svg` - MPI runtime plot

The script uses only the Python standard library. The generated SVG plots can be viewed in any browser or directly embedded into markdown reports.

On the RCE cluster, load the OpenMPI module before building and running:

```bash
module load hpcx-2.7.0/hpcx-ompi
```
