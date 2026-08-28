# HW2 Report

## Q3: Distributed Bitonic Sort

### Problem and implementation

The Q3 program sorts an integer sequence with distributed bitonic sort. Rank 0 reads the input, and `MPI_Scatter` distributes equal-sized blocks. Each rank sorts its block, then ranks exchange blocks with hypercube partners using `MPI_Sendrecv`, compare-exchange the values, and gather the final sorted sequence. The implementation requires both `N` and `P` to be powers of two and supports `P=1,2,4,8`.

### Correctness

The benchmark CSV contains 31 rows covering eight datasets. Every row has `status=PASS`, and the MPI output exactly matched sequential `std::sort`. The PDF examples, generated edge case, and datasets up to 4,194,304 elements were tested. A development error that sorted after every inner merge step was fixed by sorting only after the final inner step; this was specifically tested at `N=16,P=4` and then across the full benchmark.

| Dataset | N | Process counts | Result |
| --- | ---: | --- | --- |
| pdf_example1 | 4 | 1, 2, 4 | PASS |
| pdf_example2 | 8 | 1, 2, 4, 8 | PASS |
| edge_case | 8 | 1, 2, 4, 8 | PASS |
| small_64 | 64 | 1, 2, 4, 8 | PASS |
| medium_1024 | 1,024 | 1, 2, 4, 8 | PASS |
| large_65k | 65,536 | 1, 2, 4, 8 | PASS |
| large_max | 1,048,576 | 1, 2, 4, 8 | PASS |
| very_large | 4,194,304 | 1, 2, 4, 8 | PASS |

### Wall-clock speed-up

Speed-up is $S(P)=T_1/T_P$. Values below 1 mean the MPI run was slower than the sequential baseline.

| N | P=1 | P=2 | P=4 | P=8 |
| ---: | ---: | ---: | ---: | ---: |
| 4 | 0.0115 | 0.0121 | 0.0107 | -- |
| 8, PDF | 0.0127 | 0.0117 | 0.0109 | 0.0082 |
| 8, edge | 0.0126 | 0.0118 | 0.0107 | 0.0084 |
| 64 | 0.0123 | 0.0120 | 0.0104 | 0.0084 |
| 1,024 | 0.0133 | 0.0133 | 0.0115 | 0.0090 |
| 65,536 | 0.0886 | 0.0757 | 0.0691 | 0.0591 |
| 1,048,576 | 0.5245 | 0.4154 | 0.3662 | 0.3251 |
| 4,194,304 | 0.7528 | 0.7901 | 0.5362 | 0.6511 |

The wall-clock speed-up stayed below 1 for every case because each MPI run includes process launch, MPI initialization, file parsing, communication, and finalization.

### Algorithm-only speed-up and efficiency

| N | P=1 | P=2 | P=4 | P=8 |
| ---: | ---: | ---: | ---: | ---: |
| 65,536 | 1.68 | 2.00 | 1.18 | 1.42 |
| 1,048,576 | 1.55 | 1.65 | 1.08 | 1.25 |
| 4,194,304 | 1.53 | 1.61 | 1.13 | 1.24 |

Algorithm-only efficiency $E(P)=S(P)/P$ for the same large cases was approximately 1.68/1.00/0.30/0.18, 1.55/0.83/0.27/0.16, and 1.53/0.81/0.28/0.16 for P=1/2/4/8 respectively. The best algorithm-only speed-up was about 2.00 at P=2 for 65K elements. The plots are in `Q3/results/`.

### Communication and computation

For the largest case at P=8, setup and input broadcast was the dominant phase at 0.454765 seconds. Rank 0 reads the complete file before scattering, creating a serial bottleneck. The hypercube compare-exchange adds communication at every bitonic stage, and process launch overhead further increases wall time. A future improvement would use parallel file-range reads or a binary input format so that input parsing also scales.

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

These are wall-clock speed-ups using the sequential program as $T_1$. The best wall result was 0.830 at P=2 for one million records, so MPI was still slower end to end. Algorithm-only speed-up for one million records was 0.956 at P=1, 0.969 at P=2, 0.945 at P=4, and 0.920 at P=8. Algorithm-only efficiency was 0.956, 0.484, 0.236, and 0.115 respectively.

### Communication and computation

For one million records at P=8, setup and header broadcast was the dominant measured phase at about 1.339 seconds. Rank 0 performs formatted input parsing before `MPI_Scatterv`, so the main data-loading work remains serial. Communication and final merge also increase with P, while the per-rank aggregation work becomes smaller. This explains why adding processes does not improve total runtime for the tested sizes.

### Reproducibility and PDF compliance

The generator uses `mt19937_64`, fixed seeds 42, 43, 44, and 45, and the documented ranges for timestamps, station IDs, temperature, humidity, pressure, rainfall, and wind speed. The benchmark covers the required process counts and multiple input sizes. The sample input format and all required output fields are implemented in both sequential and MPI programs. Build and benchmark instructions are provided in `Q8/README.md`; plots and CSV evidence are in `Q8/results/`.

## Conclusion

All three assigned questions include sequential and MPI implementations, Makefiles, README instructions, correctness checks, benchmark CSV files, analysis reports, and SVG plots. Q3 and Q8 are correct across all tested datasets and process counts. Q4 reproduces the PDF sample and keeps triangle counts consistent across all process counts. The measurements show that MPI startup, serial rank-0 input parsing, replicated data, and communication can dominate computation for small or moderate inputs. Larger computation-heavy inputs, parallel input distribution, and reduced replication would be needed to demonstrate stronger end-to-end speed-up.
