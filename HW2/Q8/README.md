## Q8: Large-Scale Weather and Environmental Data Analytics

This solution processes large-scale weather and environmental sensor logs using both a sequential baseline and a distributed MPI implementation.

### Input format

```text
N K S
timestamp station_id temperature humidity pressure rainfall wind_speed
...
```

- `N`: Total number of sensor measurement records.
- `K`: Number of top stations to report.
- `S`: Total number of unique weather stations.
- Each of the subsequent $N$ lines contains:
  `timestamp station_id temperature humidity pressure rainfall wind_speed`

### Compile and run

Build all Q8 binaries using the Makefile:

```bash
make all
```

This compiles:
- `build/weather_seq` (Sequential reference implementation)
- `build/weather_mpi` (MPI distributed analytics)
- `build/generate` (Dataset generator)

Compile and run sequential program:
```bash
g++ -O2 -std=c++17 weather_seq.cpp -o build/weather_seq
./build/weather_seq data.txt
```

Compile and run distributed MPI program:
```bash
mpicxx -O2 -std=c++17 weather_mpi.cpp -o build/weather_mpi
mpirun -np 4 ./build/weather_mpi data.txt
```

### Reproducible tests and benchmarks

Run the automated test and benchmark suite:

```bash
bash run.sh
```

The script generates test datasets and verifies MPI output against sequential output across $P \in \{1, 2, 4, 8\}$:
- Sample test case ($N=5$)
- Small dataset ($N=100$)
- Medium dataset ($N=10,000$)
- Large dataset ($N=100,000$)
- 1M dataset ($N=1,000,000$)

The generated datasets use seeds 42, 43, 44, and 45 with station counts 10, 50, 100, and 200 respectively. The generator uses `mt19937_64`, timestamps in `1700000000..1700086400`, temperatures in `[-10,50]`, humidity in `[10,100]`, pressure in `[950,1050]`, rainfall in `[0,50]`, and wind speed in `[0,120]`. MPI output comparison allows only a 0.011 numeric tolerance for values printed to two decimals, because compensated reductions can round a displayed average differently while representing the same result.

All performance metrics and timing phases (`setup`, `scatter`, `compute`, `comm`, `merge`, `algo`) are saved to `results/benchmark_<timestamp>.csv`.

### Analyze results and create plots

Generate summary tables and standalone SVG visualization charts:

```bash
python3 scripts/analyze_benchmark.py
```

Outputs in `results/`:
- `analysis_detail.csv`
- `analysis_summary.csv`
- `analysis_report.txt`
- `speedup_plot.svg`
- `algo_speedup_plot.svg`
- `efficiency_plot.svg`
- `mpi_runtime_plot.svg`

On the RCE cluster, load the OpenMPI module before building and running:

```bash
module load hpcx-2.7.0/hpcx-ompi
```
