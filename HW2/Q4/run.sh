#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

SEQ_BIN="$SCRIPT_DIR/build/triangle_seq"
MPI_BIN="$SCRIPT_DIR/build/triangle_mpi"
GEN_BIN="$SCRIPT_DIR/build/generate_graph"
RESULTS_DIR="$SCRIPT_DIR/results"
TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
RESULT_FILE="$RESULTS_DIR/benchmark_$TIMESTAMP.csv"

mkdir -p build "$RESULTS_DIR"

echo "Compiling..."
if ! command -v make >/dev/null 2>&1; then
	echo "Error: make is required to build Q4." >&2
	exit 1
fi
make -B all

cat > "$SCRIPT_DIR/build/sample_q4.txt" <<'EOF'
4 5
0 1
1 2
2 0
2 3
3 0
EOF
"$GEN_BIN" 3 3 42 > "$SCRIPT_DIR/build/edge_case.txt"
"$GEN_BIN" 4 6 43 > "$SCRIPT_DIR/build/small_k4.txt"
"$GEN_BIN" 500 5000 44 > "$SCRIPT_DIR/build/medium_500.txt"
"$GEN_BIN" 100000 1000000 45 > "$SCRIPT_DIR/build/large_max.txt"
"$GEN_BIN" 1500 1000000 46 > "$SCRIPT_DIR/build/dense_1500.txt"

if [[ -n "${SLURM_JOB_ID:-}" ]] && command -v srun >/dev/null 2>&1; then
	MPI_RUNNER=(mpirun --bind-to none --oversubscribe)
	MPI_COUNT_FLAG="-np"
else
	MPI_RUNNER=(mpirun --bind-to none --oversubscribe)
	MPI_COUNT_FLAG="-np"
fi

printf 'graph,vertices,edges,processes,sequential_seconds,wall_seconds,algo_seconds,setup_seconds,degree_seconds,adjacency_seconds,scatter_seconds,compute_seconds,reduce_seconds,wall_speedup,wall_efficiency,algo_speedup,algo_efficiency,triangles\n' > "$RESULT_FILE"

benchmark() {
	local graph_name="$1"
	local input_file="$2"
	local expected_result="${3:-}"
	local vertices edges seq_seconds seq_result processes wall_seconds algo_seconds setup_seconds degree_seconds adjacency_seconds scatter_seconds compute_seconds reduce_seconds
	local mpi_result wall_speedup wall_efficiency algo_speedup algo_efficiency phase_line
	local seq_output="$SCRIPT_DIR/build/seq_output.txt"
	local mpi_output="$SCRIPT_DIR/build/mpi_output.txt"
	local mpi_log="$SCRIPT_DIR/build/mpi_log.txt"
	local repetitions=1
	read -r vertices edges < "$input_file"
	if (( edges < 10000 )); then repetitions=10; fi
	seq_seconds="$(for ((repeat = 0; repeat < repetitions; ++repeat)); do
		start="$(date +%s%N)"
		"$SEQ_BIN" "$input_file" > "$seq_output"
		end="$(date +%s%N)"
		printf '%s\n' "$((end - start))"
	done | awk '{ total += $1 } END { printf "%.9f", total / NR / 1000000000 }')"
	seq_result="$(< "$seq_output")"
	if [[ -n "$expected_result" && "$seq_result" != "$expected_result" ]]; then
		echo "Sequential correctness failed for $graph_name: expected $expected_result, got $seq_result." >&2
		exit 1
	fi
	for processes in 1 2 4 8; do
		wall_seconds="$(for ((repeat = 0; repeat < repetitions; ++repeat)); do
			start="$(date +%s%N)"
			"${MPI_RUNNER[@]}" "$MPI_COUNT_FLAG" "$processes" "$MPI_BIN" "$input_file" > "$mpi_output" 2> "$mpi_log"
			end="$(date +%s%N)"
			printf '%s\n' "$((end - start))"
		done | awk '{ total += $1 } END { printf "%.9f", total / NR / 1000000000 }')"
		phase_line="$(grep 'MPI_PHASES ' "$mpi_log" | tail -n 1)"
		read -r setup_seconds degree_seconds adjacency_seconds scatter_seconds compute_seconds reduce_seconds algo_seconds <<< "$(printf '%s\n' "$phase_line" | awk '{ for (i = 2; i <= NF; i++) { split($i, value, "="); metrics[value[1]] = value[2] } printf "%s %s %s %s %s %s %s", metrics["setup"], metrics["degree"], metrics["adjacency"], metrics["scatter"], metrics["compute"], metrics["reduce"], metrics["algo"] }')"
		if [[ -z "$algo_seconds" ]]; then
			echo "MPI phase timing output missing for $graph_name at P=$processes." >&2
			echo "Expected a line beginning with MPI_PHASES in $mpi_log." >&2
			exit 1
		fi
		mpi_result="$(< "$mpi_output")"
		if [[ "$mpi_result" != "$seq_result" ]]; then
			echo "Correctness failed for $graph_name at P=$processes." >&2
			exit 1
		fi
		wall_speedup="$(awk "BEGIN { printf \"%.6f\", $seq_seconds / $wall_seconds }")"
		wall_efficiency="$(awk "BEGIN { printf \"%.6f\", $wall_speedup / $processes }")"
		algo_speedup="$(awk "BEGIN { printf \"%.6f\", $seq_seconds / $algo_seconds }")"
		algo_efficiency="$(awk "BEGIN { printf \"%.6f\", $algo_speedup / $processes }")"
		printf '%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n' "$graph_name" "$vertices" "$edges" "$processes" "$seq_seconds" "$wall_seconds" "$algo_seconds" "$setup_seconds" "$degree_seconds" "$adjacency_seconds" "$scatter_seconds" "$compute_seconds" "$reduce_seconds" "$wall_speedup" "$wall_efficiency" "$algo_speedup" "$algo_efficiency" "$mpi_result" >> "$RESULT_FILE"
	done
	echo "$graph_name: $vertices vertices, $edges edges, $seq_result triangles"
}

echo "Running PDF sample, edge case, K4, medium, large, and dense benchmarks..."
benchmark pdf_example "$SCRIPT_DIR/build/sample_q4.txt" 2
benchmark edge_case "$SCRIPT_DIR/build/edge_case.txt" 1
benchmark small_k4 "$SCRIPT_DIR/build/small_k4.txt" 4
benchmark medium_500 "$SCRIPT_DIR/build/medium_500.txt"
benchmark large_max "$SCRIPT_DIR/build/large_max.txt"
benchmark dense_1500 "$SCRIPT_DIR/build/dense_1500.txt"

echo "Results saved to: $RESULT_FILE"
