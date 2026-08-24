## Q4: Triangle Counting

This solution counts triangles in an undirected graph. Each MPI process receives a distinct subset of edges. The graph orientation is based on vertex degree and vertex ID, so every triangle is counted exactly once.

### Input format

```text
V E
u1 v1
u2 v2
...
```

Vertices are 0-indexed. The input must contain `3 <= V <= 100000` and `3 <= E <= 1000000` distinct undirected edges.

### Compile and run

Build all Q4 programs with the Makefile:

```bash
make all
```

This creates `build/triangle_seq`, `build/triangle_mpi`, and `build/generate_graph`.

Compile the sequential program with:

```bash
g++ -O2 -std=c++17 triangle_seq.cpp -o triangle_seq
./triangle_seq example.txt
```

Compile and run the MPI program with:

```bash
mpicxx -O2 -std=c++17 triangle_mpi.cpp -o triangle_mpi
mpirun -np 4 ./triangle_mpi example.txt
```

The program prints only the total triangle count to standard output. MPI phase timings are printed to standard error with the `MPI_PHASES` prefix.

### Reproducible tests and benchmarks

Run the benchmark script from this directory:

```bash
bash run.sh
```

The script generates these fixed-seed cases:

| Case | Vertices | Edges | Seed |
| --- | ---: | ---: | ---: |
| PDF sample | 4 | 5 | fixed sample |
| Edge case | 3 | 3 | 42 |
| Small K4 | 4 | 6 | 43 |
| Medium | 500 | 5000 | 44 |
| Large maximum | 100000 | 1000000 | 45 |

Each case is checked with the sequential program and MPI using `P=1,2,4,8`. Tiny cases use repeated runs. Benchmark CSV files contain high-resolution wall time, MPI algorithm time, setup/broadcast/scatter/compute/reduce phases, both speedup types, efficiency, and triangle count. The suite also includes a dense `V=1500`, `E=1000000` case, using about 89% of the possible edges, to exercise the worst-case behavior of the algorithm.

The timing is end-to-end program runtime measured by the shell script. Correctness output remains a single integer as required by the assignment.

### Analyze results and create plots

Run the Python script after a benchmark:

```bash
python3 scripts/analyze_benchmark.py
```

The script automatically reads the newest CSV in `results/`. It checks that triangle counts are equal for all process counts and writes these files:

- `analysis_detail.csv` - cleaned benchmark data for Excel
- `analysis_summary.csv` - one summary row per graph
- `analysis_report.txt` - short text summary
- `speedup_plot.svg` - speedup plot
- `efficiency_plot.svg` - efficiency plot
- `mpi_runtime_plot.svg` - MPI runtime plot

The script uses only the Python standard library. SVG plots can be opened in a browser or inserted into the report.
