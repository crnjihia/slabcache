#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

echo "=== Building benchmark suite ==="
cd "${ROOT_DIR}"
cmake --preset bench
cmake --build --preset bench

mkdir -p "${SCRIPT_DIR}/results"

echo "=== Running Google Benchmark: Allocators ==="
"${ROOT_DIR}/build/bench/bench_alloc" \
    --benchmark_out="${SCRIPT_DIR}/results/bench_alloc.json" \
    --benchmark_out_format=json

echo "=== Running Google Benchmark: Key-Value Store ==="
"${ROOT_DIR}/build/bench/bench_kv" \
    --benchmark_out="${SCRIPT_DIR}/results/bench_kv.json" \
    --benchmark_out_format=json

echo "=== Benchmark run complete! Results saved in benchmarks/results/ ==="
