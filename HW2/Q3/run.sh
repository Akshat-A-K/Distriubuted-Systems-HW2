#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

SEQ_BIN="$SCRIPT_DIR/build/bitonic_seq"
MPI_BIN="$SCRIPT_DIR/build/bitonic_mpi"
GEN_BIN="$SCRIPT_DIR/build/generate_data"
RESULTS_DIR="$SCRIPT_DIR/results"
TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
RESULT_FILE="$RESULTS_DIR/benchmark_$TIMESTAMP.csv"

mkdir -p build "$RESULTS_DIR"

echo "Compiling..."
if ! command -v make >/dev/null 2>&1; then
	echo "Error: make is required to build Q3." >&2
	exit 1
fi
make -B all

# 1. Generate test cases
cat > "$SCRIPT_DIR/build/sample_n4.txt" <<'EOF'
4
4 1 3 2
EOF

cat > "$SCRIPT_DIR/build/sample_n8.txt" <<'EOF'
8
5 3 8 1 9 2 7 4
EOF

cat > "$SCRIPT_DIR/build/sample_n16.txt" <<'EOF'
16
15 3 8 1 12 7 4 10 6 14 2 11 5 16 9 13
EOF

"$GEN_BIN" 8 42 > "$SCRIPT_DIR/build/edge_case.txt"
"$GEN_BIN" 64 43 > "$SCRIPT_DIR/build/small_64.txt"
"$GEN_BIN" 1024 44 > "$SCRIPT_DIR/build/medium_1024.txt"
"$GEN_BIN" 65536 45 > "$SCRIPT_DIR/build/large_65k.txt"
"$GEN_BIN" 1048576 46 > "$SCRIPT_DIR/build/large_max.txt"
"$GEN_BIN" 4194304 47 > "$SCRIPT_DIR/build/very_large.txt"

# On this cluster (RCE), srun's PMI handoff does not match the loaded
# OpenMPI build (produces a PMI error). mpirun launched from inside an
# active salloc/sbatch allocation reads SLURM's environment directly and
# works correctly -- this is a standard, supported launch mode, not a
# workaround. Confirmed empirically on job 79811 (8-task allocation).
MPI_RUNNER=(mpirun --bind-to none --oversubscribe)
MPI_COUNT_FLAG="-np"

printf 'dataset,elements,processes,sequential_seconds,wall_seconds,compute_seconds,comm_seconds,speedup,efficiency,status\n' > "$RESULT_FILE"

benchmark() {
	local dataset_name="$1"
	local input_file="$2"
	local expected_result="${3:-}"
	local elements seq_seconds seq_result processes wall_seconds compute_seconds comm_seconds
	local mpi_result speedup efficiency phase_line
	local seq_output="$SCRIPT_DIR/build/seq_output.txt"
	local mpi_output="$SCRIPT_DIR/build/mpi_output.txt"
	local mpi_log="$SCRIPT_DIR/build/mpi_log.txt"
	local repetitions=1

	read -r elements < "$input_file"
	if (( elements < 10000 )); then repetitions=10; fi

	seq_seconds="$(for ((repeat = 0; repeat < repetitions; ++repeat)); do
		start="$(date +%s%N)"
		"$SEQ_BIN" "$input_file" > "$seq_output"
		end="$(date +%s%N)"
		printf '%s\n' "$((end - start))"
	done | awk '{ total += $1 } END { printf "%.9f", total / NR / 1000000000 }')"

	seq_result="$(< "$seq_output")"
	if [[ -n "$expected_result" && "$seq_result" != "$expected_result" ]]; then
		echo "Sequential correctness failed for $dataset_name: expected '$expected_result', got '$seq_result'." >&2
		exit 1
	fi

	for processes in 1 2 4 8; do
		if (( elements < processes )); then
			continue
		fi

		wall_seconds="$(for ((repeat = 0; repeat < repetitions; ++repeat)); do
			start="$(date +%s%N)"
			"${MPI_RUNNER[@]}" "$MPI_COUNT_FLAG" "$processes" "$MPI_BIN" "$input_file" > "$mpi_output" 2> "$mpi_log"
			end="$(date +%s%N)"
			printf '%s\n' "$((end - start))"
		done | awk '{ total += $1 } END { printf "%.9f", total / NR / 1000000000 }')"

		phase_line="$(grep 'MPI_PHASES ' "$mpi_log" | tail -n 1)"
		read -r compute_seconds comm_seconds <<< "$(printf '%s\n' "$phase_line" | awk '{ for (i = 2; i <= NF; i++) { split($i, value, "="); metrics[value[1]] = value[2] } printf "%s %s", metrics["compute"], metrics["comm"] }')"

		if [[ -z "$compute_seconds" ]]; then
			echo "MPI phase timing output missing for $dataset_name at P=$processes." >&2
			echo "Expected a line beginning with MPI_PHASES in $mpi_log." >&2
			exit 1
		fi

		mpi_result="$(< "$mpi_output")"
		if [[ "$mpi_result" != "$seq_result" ]]; then
			echo "Correctness failed for $dataset_name at P=$processes." >&2
			exit 1
		fi

		speedup="$(awk "BEGIN { printf \"%.6f\", $seq_seconds / $wall_seconds }")"
		efficiency="$(awk "BEGIN { printf \"%.6f\", $speedup / $processes }")"

		printf '%s,%s,%s,%s,%s,%s,%s,%s,%s,PASS\n' \
			"$dataset_name" "$elements" "$processes" "$seq_seconds" "$wall_seconds" \
			"$compute_seconds" "$comm_seconds" "$speedup" "$efficiency" >> "$RESULT_FILE"
	done
	echo "$dataset_name: $elements elements sorted and verified across all P"
}

echo "Running PDF sample, edge case, small, medium, large, and very large benchmarks..."
benchmark pdf_example1 "$SCRIPT_DIR/build/sample_n4.txt" "1 2 3 4"
benchmark pdf_example2 "$SCRIPT_DIR/build/sample_n8.txt" "1 2 3 4 5 7 8 9"
benchmark sample_n16 "$SCRIPT_DIR/build/sample_n16.txt" "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16"
benchmark edge_case "$SCRIPT_DIR/build/edge_case.txt"
benchmark small_64 "$SCRIPT_DIR/build/small_64.txt"
benchmark medium_1024 "$SCRIPT_DIR/build/medium_1024.txt"
benchmark large_65k "$SCRIPT_DIR/build/large_65k.txt"
benchmark large_max "$SCRIPT_DIR/build/large_max.txt"
benchmark very_large "$SCRIPT_DIR/build/very_large.txt"

echo "Results saved to: $RESULT_FILE"

if command -v python3 >/dev/null 2>&1 && [[ -f "$SCRIPT_DIR/scripts/analyze_benchmark.py" ]]; then
	echo "Running benchmark analysis..."
	python3 "$SCRIPT_DIR/scripts/analyze_benchmark.py" "$RESULT_FILE"
fi
