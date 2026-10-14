#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${ROOT_DIR}"

if [ ! -f build/bench/bench_alloc ]; then
    cmake --preset bench
    cmake --build --preset bench
fi

echo "=== Recording perf profile on slab allocator hot path ==="
perf record -g -F 999 -o /tmp/perf.data ./build/bench/bench_alloc --benchmark_filter=BM_SlabAlloc64
cp -f /tmp/perf.data ./perf.data || true

echo "=== Top Hot Paths in Allocator ==="
perf report -i /tmp/perf.data --stdio --no-children | head -n 35

if command -v stackcollapse-perf.pl >/dev/null && command -v flamegraph.pl >/dev/null; then
    perf script -i /tmp/perf.data | stackcollapse-perf.pl | flamegraph.pl > perf_flamegraph.svg
    echo "Flamegraph generated: perf_flamegraph.svg"
else
    echo "Flamegraph utilities (stackcollapse-perf.pl / flamegraph.pl) not in PATH; perf.data recorded."
fi
