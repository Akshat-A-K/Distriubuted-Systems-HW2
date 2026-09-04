#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

SEQ_BIN="$SCRIPT_DIR/build/weather_seq"
MPI_BIN="$SCRIPT_DIR/build/weather_mpi"
GEN_BIN="$SCRIPT_DIR/build/generate"
RESULTS_DIR="$SCRIPT_DIR/results"
TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
RESULT_FILE="$RESULTS_DIR/benchmark_$TIMESTAMP.csv"

mkdir -p build "$RESULTS_DIR"

echo "Compiling..."
if ! command -v make >/dev/null 2>&1; then
	echo "Error: make is required to build Q8." >&2
	exit 1
fi
make -B all

# Generate test cases
cat > "$SCRIPT_DIR/build/sample_q8.txt" <<'EOF'
5 2 3
1700000000 1 25.5 60.0 1013.2 0.0 12.5
1700000030 2 41.2 55.0 1010.5 5.2 22.0
1700000060 1 -2.5 80.0 1015.0 12.0 5.0
1700000090 3 30.0 45.0 1008.0 0.0 35.5
1700000120 2 18.0 70.0 1012.0 2.0 15.0
EOF

"$GEN_BIN" 100 5 10 42 > "$SCRIPT_DIR/build/small_100.txt"
"$GEN_BIN" 10000 5 50 43 > "$SCRIPT_DIR/build/medium_10k.txt"
"$GEN_BIN" 100000 10 100 44 > "$SCRIPT_DIR/build/large_100k.txt"
"$GEN_BIN" 1000000 10 200 45 > "$SCRIPT_DIR/build/large_1m.txt"

# On this cluster (RCE), mpirun with oversubscribe handles local and SLURM allocations
MPI_RUNNER=(mpirun --bind-to none --oversubscribe)
MPI_COUNT_FLAG="-np"

outputs_match() {
	awk '
	NR == FNR { expected[FNR] = $0; expected_lines = FNR; next }
	{
		if (FNR > expected_lines) exit 1
		left_count = split(expected[FNR], left)
		right_count = split($0, right)
		if (left_count != right_count) exit 1
		for (i = 1; i <= left_count; ++i) {
			if (left[i] ~ /^-?[0-9]+([.][0-9]+)?$/ && right[i] ~ /^-?[0-9]+([.][0-9]+)?$/) {
				if (sqrt((left[i] - right[i]) ^ 2) > 0.011) exit 1
			} else if (left[i] != right[i]) {
				exit 1
			}
		}
	}
	END { if (FNR != expected_lines) exit 1 }
	' "$seq_output" "$mpi_output"
}

printf 'dataset,records,processes,sequential_seconds,wall_seconds,compute_seconds,comm_seconds,speedup,efficiency,status\n' > "$RESULT_FILE"

benchmark() {
	local dataset_name="$1"
	local input_file="$2"
	local records k s seq_seconds seq_result processes wall_seconds compute_seconds comm_seconds
	local mpi_result speedup efficiency phase_line
	local seq_output="$SCRIPT_DIR/build/seq_output.txt"
	local mpi_output="$SCRIPT_DIR/build/mpi_output.txt"
	local mpi_log="$SCRIPT_DIR/build/mpi_log.txt"
	local repetitions=1

	read -r records k s < "$input_file"
	if (( records < 10000 )); then repetitions=10; fi

	seq_seconds="$(for ((repeat = 0; repeat < repetitions; ++repeat)); do
		start="$(date +%s%N)"
		"$SEQ_BIN" "$input_file" > "$seq_output"
		end="$(date +%s%N)"
		printf '%s\n' "$((end - start))"
	done | awk '{ total += $1 } END { printf "%.9f", total / NR / 1000000000 }')"

	seq_result="$(< "$seq_output")"

	for processes in 1 2 4 8; do
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
		if ! outputs_match; then
			echo "Correctness failed for $dataset_name at P=$processes." >&2
			echo "Diff:" >&2
			diff -u "$seq_output" "$mpi_output" >&2 || true
			exit 1
		fi

		speedup="$(awk "BEGIN { printf \"%.6f\", $seq_seconds / $wall_seconds }")"
		efficiency="$(awk "BEGIN { printf \"%.6f\", $speedup / $processes }")"

		printf '%s,%s,%s,%s,%s,%s,%s,%s,%s,PASS\n' \
			"$dataset_name" "$records" "$processes" "$seq_seconds" "$wall_seconds" \
			"$compute_seconds" "$comm_seconds" "$speedup" "$efficiency" >> "$RESULT_FILE"
	done
	echo "$dataset_name: $records records verified across all P"
}

echo "Running sample, small, medium, large, and 1M benchmarks..."
benchmark sample_q8 "$SCRIPT_DIR/build/sample_q8.txt"
benchmark small_100 "$SCRIPT_DIR/build/small_100.txt"
benchmark medium_10k "$SCRIPT_DIR/build/medium_10k.txt"
benchmark large_100k "$SCRIPT_DIR/build/large_100k.txt"
benchmark large_1m "$SCRIPT_DIR/build/large_1m.txt"

echo "Results saved to: $RESULT_FILE"

if command -v python3 >/dev/null 2>&1 && [[ -f "$SCRIPT_DIR/scripts/analyze_benchmark.py" ]]; then
	echo "Running benchmark analysis..."
	python3 "$SCRIPT_DIR/scripts/analyze_benchmark.py" "$RESULT_FILE"
fi
