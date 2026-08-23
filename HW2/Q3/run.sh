#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

MPI_BIN="build/bitonic"
RESULTS_DIR="results"
TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
RESULT_FILE="$RESULTS_DIR/benchmark_$TIMESTAMP.csv"

mkdir -p build "$RESULTS_DIR"

echo "=========================================================="
echo "Compiling Bitonic Sort MPI Program..."
echo "=========================================================="
if ! command -v mpicxx >/dev/null 2>&1; then
    echo "Error: mpicxx compiler is required." >&2
    exit 1
fi
mpicxx -O2 -std=c++17 -Wall -Wextra -pedantic bitonic.cpp -o "$MPI_BIN"
echo "Compilation successful: $MPI_BIN"
echo ""

# Create sample input files from Home_Work_2.pdf
cat > build/sample_n4.txt <<'EOF'
4
4 1 3 2
EOF

cat > build/sample_n8.txt <<'EOF'
8
5 3 8 1 9 2 7 4
EOF

# Detect runner (SLURM srun vs mpirun)
if [[ -n "${SLURM_JOB_ID:-}" ]] && command -v srun >/dev/null 2>&1; then
    MPI_RUNNER=(srun)
    MPI_COUNT_FLAG="-n"
else
    MPI_RUNNER=(mpirun)
    MPI_COUNT_FLAG="-np"
fi

echo "=========================================================="
echo "Running Correctness Tests on PDF Sample Cases..."
echo "=========================================================="

# Test Example 1 (N=4)
echo "Testing Example 1 (N=4, P=2)..."
RES_N4_P1="$("${MPI_RUNNER[@]}" "$MPI_COUNT_FLAG" 1 "$MPI_BIN" build/sample_n4.txt --print)"
RES_N4_P2="$("${MPI_RUNNER[@]}" "$MPI_COUNT_FLAG" 2 "$MPI_BIN" build/sample_n4.txt --print)"

if [[ "$RES_N4_P1" != "1 2 3 4" || "$RES_N4_P2" != "1 2 3 4" ]]; then
    echo "FAILED: Example 1 output mismatch! Expected '1 2 3 4', got P1='$RES_N4_P1', P2='$RES_N4_P2'" >&2
    exit 1
fi
echo "PASSED: Example 1 (N=4) -> $RES_N4_P2"

# Test Example 2 (N=8)
echo "Testing Example 2 (N=8, P=1, 2, 4, 8)..."
EXPECTED_N8="1 2 3 4 5 7 8 9"
for p in 1 2 4 8; do
    RES_N8="$("${MPI_RUNNER[@]}" "$MPI_COUNT_FLAG" "$p" "$MPI_BIN" build/sample_n8.txt --print)"
    if [[ "$RES_N8" != "$EXPECTED_N8" ]]; then
        echo "FAILED: Example 2 output mismatch for P=$p! Expected '$EXPECTED_N8', got '$RES_N8'" >&2
        exit 1
    fi
done
echo "PASSED: Example 2 (N=8) -> $EXPECTED_N8 across all P in {1, 2, 4, 8}"
echo ""

echo "=========================================================="
echo "Running Benchmark Suite..."
echo "=========================================================="

printf 'input_category,N,processes,seq_time_sec,total_time_sec,comp_time_sec,comm_time_sec,comm_pct,speedup,efficiency,correct\n' > "$RESULT_FILE"

# List of input sizes: small, medium, large, very large
SIZES=(
    "Small:8"
    "Small:64"
    "Small:1024"
    "Medium:65536"
    "Medium:262144"
    "Large:1048576"
    "Large:4194304"
    "VeryLarge:16777216"
)

for entry in "${SIZES[@]}"; do
    CATEGORY="${entry%%:*}"
    N="${entry##*:}"
    echo "----------------------------------------------------------"
    echo "Benchmarking Category: $CATEGORY, N = $N"
    echo "----------------------------------------------------------"

    T1=""
    for P in 1 2 4 8; do
        if [ "$N" -lt "$P" ]; then
            continue
        fi

        # Run benchmark mode
        OUTPUT="$("${MPI_RUNNER[@]}" "$MPI_COUNT_FLAG" "$P" "$MPI_BIN" "$N" 42 --benchmark)"
        
        # Extract fields from output
        # Output format: N=.. P=.. SeqTime=.. TotalTime=.. CompTime=.. (X%) CommTime=.. (Y%) Correct=YES
        SEQ_TIME=$(echo "$OUTPUT" | sed -n 's/.*SeqTime=\([0-9.]*\).*/\1/p')
        TOTAL_TIME=$(echo "$OUTPUT" | sed -n 's/.*TotalTime=\([0-9.]*\).*/\1/p')
        COMP_TIME=$(echo "$OUTPUT" | sed -n 's/.*CompTime=\([0-9.]*\).*/\1/p')
        COMM_TIME=$(echo "$OUTPUT" | sed -n 's/.*CommTime=\([0-9.]*\).*/\1/p')
        CORRECT=$(echo "$OUTPUT" | sed -n 's/.*Correct=\([A-Z]*\).*/\1/p')

        if [ "$P" -eq 1 ]; then
            T1="$TOTAL_TIME"
        fi

        # Calculate speedup and efficiency
        SPEEDUP="$(awk "BEGIN { if ($TOTAL_TIME > 0) printf \"%.4f\", $T1 / $TOTAL_TIME; else printf \"1.0000\" }")"
        EFFICIENCY="$(awk "BEGIN { printf \"%.4f\", $SPEEDUP / $P }")"
        COMM_PCT="$(awk "BEGIN { if ($TOTAL_TIME > 0) printf \"%.2f\", ($COMM_TIME / $TOTAL_TIME) * 100; else printf \"0.00\" }")"

        printf '%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n' \
            "$CATEGORY" "$N" "$P" "$SEQ_TIME" "$TOTAL_TIME" "$COMP_TIME" "$COMM_TIME" "$COMM_PCT" "$SPEEDUP" "$EFFICIENCY" "$CORRECT" >> "$RESULT_FILE"

        printf "  P=%d | Total: %8.6fs | Comp: %8.6fs | Comm: %8.6fs (%5.1f%%) | Speedup: %6.2fx | Eff: %6.2f | Verified: %s\n" \
            "$P" "$TOTAL_TIME" "$COMP_TIME" "$COMM_TIME" "$COMM_PCT" "$SPEEDUP" "$EFFICIENCY" "$CORRECT"
    done
done

echo ""
echo "=========================================================="
echo "Benchmarks Complete! Results saved to: $RESULT_FILE"
echo "=========================================================="
