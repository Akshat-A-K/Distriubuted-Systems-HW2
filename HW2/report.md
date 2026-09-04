# HW2 Report

## Q3: Distributed Bitonic Sort

### Problem and implementation

The Q3 program sorts an integer sequence with distributed bitonic sort. Rank 0 reads the input, and `MPI_Scatter` distributes equal-sized blocks. Each rank sorts its block, then ranks exchange blocks with hypercube partners using `MPI_Sendrecv`, compare-exchange the values, and gather the final sorted sequence. The implementation requires both `N` and `P` to be powers of two and supports `P=1,2,4,8`.

### Correctness

The benchmark CSV contains 35 rows covering nine datasets. Every row has `status=PASS`, and the MPI output exactly matched sequential `std::sort`. The PDF examples, the `N=16` manual-verification case, a generated edge case, and datasets up to 4,194,304 elements were tested. A development error that sorted after every inner merge step was fixed by sorting only after the final inner step.

| Dataset | N | Process counts | Result |
| --- | ---: | --- | --- |
| pdf_example1 | 4 | 1, 2, 4 | PASS |
| pdf_example2 | 8 | 1, 2, 4, 8 | PASS |
| sample_n16 | 16 | 1, 2, 4, 8 | PASS |
| edge_case | 8 | 1, 2, 4, 8 | PASS |
| small_64 | 64 | 1, 2, 4, 8 | PASS |
| medium_1024 | 1,024 | 1, 2, 4, 8 | PASS |
| large_65k | 65,536 | 1, 2, 4, 8 | PASS |
| large_max | 1,048,576 | 1, 2, 4, 8 | PASS |
| very_large | 4,194,304 | 1, 2, 4, 8 | PASS |

### Speed-up

Speed-up is $S(P)=T_1/T_P$, using wall-clock time (process launch through exit) for both the sequential and MPI runs. Values below 1 mean the MPI run was slower end to end than the sequential baseline.

| N | P=1 | P=2 | P=4 | P=8 |
| ---: | ---: | ---: | ---: | ---: |
| 4 | 0.0118 | 0.0117 | 0.0106 | -- |
| 8, PDF | 0.0122 | 0.0120 | 0.0105 | 0.0084 |
| 16 | 0.0125 | 0.0120 | 0.0106 | 0.0086 |
| 8, edge | 0.0125 | 0.0121 | 0.0106 | 0.0084 |
| 64 | 0.0127 | 0.0122 | 0.0106 | 0.0084 |
| 1,024 | 0.0140 | 0.0133 | 0.0118 | 0.0092 |
| 65,536 | 0.0853 | 0.0811 | 0.0733 | 0.0644 |
| 1,048,576 | 0.5248 | 0.3768 | 0.4234 | 0.3908 |
| 4,194,304 | 0.7701 | 0.7854 | 0.7019 | 0.6186 |

The speed-up stayed below 1 for every case because each MPI run includes process launch, MPI initialization, file parsing, communication, and finalization, which the sequential baseline does not pay. `speedup_plot.svg` and `efficiency_plot.svg` in `Q3/results/` plot these two tables directly.

### Communication and computation

The MPI program separately times local computation (initial sort plus the compare-exchange/re-sort work at each bitonic stage) and communication (the `MPI_Sendrecv` chunk exchanges), reported as `compute_seconds`/`comm_seconds` in the benchmark CSV. For the largest case (4,194,304 elements), these do not move monotonically with P: `compute_seconds` is 0.300s at P=1, rises to 0.306s at P=2 and 0.327s at P=4, then drops to 0.216s at P=8; `comm_seconds` is 0.007s at P=1, rises to 0.012s at P=2, spikes to 0.118s at P=4 — nearly 3x higher than at P=8 (0.040s) — then falls back down. This is not a clean "compute shrinks / communication grows" trend as P increases.

`Q3/submit_bitonic.sh` requests `--nodes=1`, and `run.sh` launches every P value with `mpirun --bind-to none --oversubscribe` inside that single-node allocation, so the P=1, 2, 4, and 8 runs all contend for the same physical cores rather than gaining more hardware as P increases. The non-monotonic swings above are more consistent with core-contention/scheduling noise from that oversubscription than with an algorithmic effect. This measured compute/comm split also excludes process launch and MPI initialization, a separate, fixed per-run cost that is the main reason wall-clock speed-up stays below 1 regardless of this noise. A future improvement would use parallel file-range reads or a binary input format so that rank 0's serial file parsing also scales with P, and running across multiple physical nodes would let P actually add hardware parallelism instead of oversubscribing one node.

### Reproducibility

The benchmark uses `generate_data.cpp` with fixed seeds 42 through 47 for powers-of-two sizes. It is compiled with `mpicxx -O2 -std=c++17 -Wall -Wextra -pedantic`, and run with `mpirun --bind-to none --oversubscribe -np P`.

## Q4: Triangle Counting in an Undirected Graph

### Problem and implementation

Q4 counts triangles without double counting. The sequential and MPI programs orient every edge from the lower-degree endpoint to the higher-degree endpoint, breaking degree ties by vertex ID. Triangle counts are obtained by intersecting oriented neighbor lists. MPI broadcasts the degree and oriented adjacency data, scatters distinct edges across ranks, computes local intersections, and sums the local counts with `MPI_Reduce`.

### Correctness and results

The benchmark contains 24 rows for six graphs, with consistent triangle counts across P=1,2,4,8. The PDF sample output is reproduced exactly.

| Graph | V | E | Triangles |
| --- | ---: | ---: | ---: |
| PDF sample | 4 | 5 | 2 |
| Edge case | 3 | 3 | 1 |
| Small K4 | 4 | 6 | 4 |
| Medium | 500 | 5,000 | 1,284 |
| Large maximum | 100,000 | 1,000,000 | 1,300 |
| Dense | 1,500 | 1,000,000 | 395,056,429 |

| Graph | Sequential (s) | Best wall MPI (P, s) | Wall speed-up | Best algorithm speed-up |
| --- | ---: | ---: | ---: | ---: |
| PDF sample | 0.015746 | 1, 0.332328 | 0.047 | 4.448 |
| Edge case | 0.013517 | 1, 0.333538 | 0.041 | 14.119 |
| Small K4 | 0.015532 | 1, 0.335448 | 0.046 | 5.769 |
| Medium | 0.016707 | 1, 0.328635 | 0.051 | 5.158 |
| Large maximum | 0.579336 | 2, 0.770264 | 0.752 | 1.373 |
| Dense | 3.059672 | 8, 1.365551 | 2.241 | 3.057 |

### Communication and scalability

Small graphs are dominated by MPI startup, so their wall speed-up is below 1. The dense graph benefits from P=8 because its local intersection work is large enough to offset communication, reaching 2.241 wall speed-up and 3.057 algorithm-only speed-up. For `large_max`, the P=8 run spent about 0.510 seconds in setup/I/O and initial broadcast, while local compute was only about 0.0005 seconds. The current full adjacency broadcast simplifies intersections but uses more memory and communication than a design that exchanges only required neighbor data.

### Reproducibility

`generate_graph.cpp` generates distinct undirected edges with fixed seeds 42, 43, 44, 45, and 46. The programs use `mpicxx -O2 -std=c++17 -Wall -Wextra -pedantic` and `mpirun --bind-to none --oversubscribe -np P`. The benchmark records setup, degree broadcast, adjacency broadcast, edge scatter, local compute, reduction, wall time, speed-up, efficiency, and triangle count. Plots are in `Q4/results/`.

## Q8: Large-Scale Weather and Environmental Data Analytics

### Problem and implementation

Q8 processes records with the fields `timestamp station_id temperature humidity pressure rainfall wind_speed`. Both versions compute the required global statistics, extreme measurements, busiest 60-second interval, extreme-temperature count, and top-K station summaries. MPI rank 0 reads the records, distributes them with `MPI_Scatterv`, each rank aggregates its local records, and rank 0 merges the partial statistics and station results.

### Correctness

The benchmark contains 20 rows covering five datasets and P=1,2,4,8. All rows report `status=PASS`, including the five-record sample from the PDF format and the 1,000,000-record dataset. MPI output was compared with sequential output; values printed to two decimal places allow a maximum 0.011 numerical difference for equivalent floating-point reductions.

| Dataset | Records | Process counts | Result |
| --- | ---: | --- | --- |
| sample_q8 | 5 | 1, 2, 4, 8 | PASS |
| small_100 | 100 | 1, 2, 4, 8 | PASS |
| medium_10k | 10,000 | 1, 2, 4, 8 | PASS |
| large_100k | 100,000 | 1, 2, 4, 8 | PASS |
| large_1m | 1,000,000 | 1, 2, 4, 8 | PASS |

### Speed-up and efficiency

| Records | P=1 | P=2 | P=4 | P=8 |
| ---: | ---: | ---: | ---: | ---: |
| 5 | 0.0166 | 0.0152 | 0.0115 | 0.0079 |
| 100 | 0.0183 | 0.0159 | 0.0123 | 0.0085 |
| 10,000 | 0.0968 | 0.0910 | 0.0665 | 0.0459 |
| 100,000 | 0.4459 | 0.4397 | 0.3459 | 0.2471 |
| 1,000,000 | 0.8284 | 0.8303 | 0.7963 | 0.7324 |

These are speed-ups $S(P)=T_1/T_P$ using wall-clock time (process launch through exit) with the sequential program as $T_1$. The best result was 0.830 at P=2 for one million records, so MPI was still slower end to end for every tested size; each MPI run pays process launch, initialization, and teardown overhead that the sequential baseline does not. `speedup_plot.svg` and `efficiency_plot.svg` in `Q8/results/` plot these values.

### Communication and computation

The MPI program separately times local per-rank aggregation (`compute_seconds`) and the gather/reduce/`Gatherv` merge of partial statistics (`comm_seconds`). For the largest dataset (1,000,000 records) at P=8, communication (0.009855s) slightly exceeded local computation (0.006249s): as P grows, each rank's share of records shrinks, so per-rank aggregation gets cheaper while more partial results must be merged across more ranks. This split excludes rank 0's serial, formatted input parsing before `MPI_Scatterv`, which is a fixed per-run cost independent of P and is the main reason wall-clock speed-up stays below 1 for the tested sizes.

### Reproducibility and PDF compliance

The generator uses `mt19937_64`, fixed seeds 42, 43, 44, and 45, and the documented ranges for timestamps, station IDs, temperature, humidity, pressure, rainfall, and wind speed. The benchmark covers the required process counts and multiple input sizes. The sample input format and all required output fields are implemented in both sequential and MPI programs. Build and benchmark instructions are provided in `Q8/README.md`; plots and CSV evidence are in `Q8/results/`.

## Conclusion

All three assigned questions include sequential and MPI implementations, Makefiles, README instructions, correctness checks, benchmark CSV files, analysis reports, and SVG plots. Q3 and Q8 are correct across all tested datasets and process counts. Q4 reproduces the PDF sample and keeps triangle counts consistent across all process counts. The measurements show that MPI startup, serial rank-0 input parsing, replicated data, and communication can dominate computation for small or moderate inputs. Larger computation-heavy inputs, parallel input distribution, and reduced replication would be needed to demonstrate stronger end-to-end speed-up.
