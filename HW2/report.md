# HW2 Report

## Q3: Bitonic Sort

- Problem description
- Sequential vs. parallel timing results
- Speed-up table
- Efficiency and observations

## Q4: Triangle Counting

### Problem

The Q4 program counts triangles in an undirected graph. The graph edges are divided between MPI processes. The program uses vertex degree and vertex ID to orient the edges, so one triangle is counted only once.

### Results

The benchmark was run with 1, 2, 4, and 8 MPI processes on five graph sizes. The result file contains 20 rows. All process counts gave the same answer for each graph, so the parallel result is correct.

| Graph | Vertices | Edges | Triangles |
| --- | ---: | ---: | ---: |
| PDF sample | 4 | 5 | 2 |
| Edge case | 3 | 3 | 1 |
| Small K4 | 4 | 6 | 4 |
| Medium | 500 | 5,000 | 1,284 |
| Large maximum | 100,000 | 1,000,000 | 1,300 |

![MPI speedup](Q4/results/speedup_plot.svg)

![MPI efficiency](Q4/results/efficiency_plot.svg)

![MPI runtime](Q4/results/mpi_runtime_plot.svg)

### Observations

For the first four small and medium graphs, the sequential time is shown as 0.00 seconds. This is because the work is smaller than the timer resolution. Therefore, speedup for these cases is not useful for comparison.

For the largest graph, the sequential time was 0.35 seconds. The MPI time was 0.59 seconds with 1 process, 0.80 seconds with 2 processes, and 0.85 seconds with 4 and 8 processes. The best measured speedup was 0.593 with 1 MPI process. In this test, adding more processes did not improve the time because MPI startup, communication, and data distribution took more time than the parallel computation.

The triangle counts were 2, 1, 4, 1,284, and 1,300 for the five graphs. These values were unchanged for all process counts. The complete data and analysis files are in `HW2/Q4/results/`.

## Q8: Weather Data Parallelization

- Problem description
- Sequential vs. MPI timing results
- Speed-up table
- Efficiency and observations
- Correctness checks

## Conclusion

The Q4 correctness tests passed for all graph sizes and process counts. The benchmark also shows that parallel execution is not always faster. For this input size, communication and MPI startup overhead are important bottlenecks. A larger graph or a more computation-heavy triangle counting method may be needed to see better scaling.
