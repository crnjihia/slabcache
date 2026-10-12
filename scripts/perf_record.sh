#!/usr/bin/env bash

# Record performance data for the kv benchmark.
# This script assumes the kv benchmark executable is built with the bench preset.

set -e

# Build benchmarks if needed.
if [ ! -f build/bench/bench_kv ]; then
    cmake --preset bench
    cmake --build --preset bench
fi

# Run perf record on the kv benchmark.
perf record -g -o perf.data ./build/bench/bench_kv

# Generate a flamegraph (requires flamegraph.pl in path).
if command -v stackcollapse-perf.pl >/dev/null && command -v flamegraph.pl >/dev/null; then
    perf script | stackcollapse-perf.pl | flamegraph.pl > perf_flamegraph.svg
    echo "Flamegraph generated: perf_flamegraph.svg"
else
    echo "stackcollapse-perf.pl or flamegraph.pl not found; skipping flamegraph generation."
fi
