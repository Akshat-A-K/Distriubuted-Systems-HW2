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
make all

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

if [[ -n "${SLURM_JOB_ID:-}" ]] && command -v srun >/dev/null 2>&1; then
	MPI_RUNNER=(mpirun)
	MPI_COUNT_FLAG="-np"
else
	MPI_RUNNER=(mpirun)
	MPI_COUNT_FLAG="-np"
fi

printf 'graph,vertices,edges,processes,sequential_seconds,mpi_seconds,speedup,efficiency,triangles\n' > "$RESULT_FILE"

benchmark() {
	local graph_name="$1"
	local input_file="$2"
	local expected_result="${3:-}"
	local vertices edges seq_seconds seq_result processes mpi_seconds mpi_result speedup efficiency
	local seq_output="$SCRIPT_DIR/build/seq_output.txt"
	local seq_time="$SCRIPT_DIR/build/seq_time.txt"
	local mpi_output="$SCRIPT_DIR/build/mpi_output.txt"
	local mpi_time="$SCRIPT_DIR/build/mpi_time.txt"
	read -r vertices edges < "$input_file"
	/usr/bin/time -f '%e' -o "$seq_time" "$SEQ_BIN" "$input_file" > "$seq_output"
	seq_seconds="$(< "$seq_time")"
	seq_result="$(< "$seq_output")"
	if [[ -n "$expected_result" && "$seq_result" != "$expected_result" ]]; then
		echo "Sequential correctness failed for $graph_name: expected $expected_result, got $seq_result." >&2
		exit 1
	fi
	for processes in 1 2 4 8; do
		/usr/bin/time -f '%e' -o "$mpi_time" "${MPI_RUNNER[@]}" "$MPI_COUNT_FLAG" "$processes" "$MPI_BIN" "$input_file" > "$mpi_output"
		mpi_seconds="$(< "$mpi_time")"
		mpi_result="$(< "$mpi_output")"
		if [[ "$mpi_result" != "$seq_result" ]]; then
			echo "Correctness failed for $graph_name at P=$processes." >&2
			exit 1
		fi
		speedup="$(awk "BEGIN { printf \"%.6f\", $seq_seconds / $mpi_seconds }")"
		efficiency="$(awk "BEGIN { printf \"%.6f\", $speedup / $processes }")"
		printf '%s,%s,%s,%s,%s,%s,%s,%s,%s\n' "$graph_name" "$vertices" "$edges" "$processes" "$seq_seconds" "$mpi_seconds" "$speedup" "$efficiency" "$mpi_result" >> "$RESULT_FILE"
	done
	echo "$graph_name: $vertices vertices, $edges edges, $seq_result triangles"
}

echo "Running PDF sample, edge case, K4, medium, and large benchmarks..."
benchmark pdf_example "$SCRIPT_DIR/build/sample_q4.txt" 2
benchmark edge_case "$SCRIPT_DIR/build/edge_case.txt" 1
benchmark small_k4 "$SCRIPT_DIR/build/small_k4.txt" 4
benchmark medium_500 "$SCRIPT_DIR/build/medium_500.txt"
benchmark large_max "$SCRIPT_DIR/build/large_max.txt"

echo "Results saved to: $RESULT_FILE"
