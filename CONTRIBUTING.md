# Contributing to Slabcache

Thank you for contributing to Slabcache! Slabcache is a high-performance C++20 hybrid memory allocator and in-memory key-value store speaking RESP3.

## Prerequisites

- **C++ Compiler**: GCC 14+ or Clang 18+ (C++20 support required)
- **CMake**: 3.23+
- **Build System**: Ninja (recommended)
- **Optional Dependencies**:
  - `libjemalloc-dev`
  - `libtcmalloc-minimal4`
  - `libgoogle-perftools-dev`
  - `libbenchmark-dev`
  - `redis-tools`
  - `valgrind`

On Ubuntu 24.04:
```bash
sudo apt-get update && sudo apt-get install -y \
    cmake ninja-build \
    libjemalloc-dev libtcmalloc-minimal4 libgoogle-perftools-dev \
    libbenchmark-dev redis-tools valgrind \
    clang-18 clang-tidy clang-format
```

## Building the Project

### Release Build
```bash
cmake --preset release
cmake --build --preset release
```

### Debug Build
```bash
cmake --preset debug
cmake --build --preset debug
```

### Running Tests
```bash
ctest --preset release --output-on-failure
```

### Running Allocator Fuzz Testing
```bash
./build/release/tests/test_fuzz_allocator --iterations=1000000
```

### Sanitizer Checks
Before submitting a PR, verify clean runs under AddressSanitizer and ThreadSanitizer:
```bash
# ASan + UBSan
cmake --preset asan
cmake --build --preset asan
ctest --preset asan --output-on-failure

# TSan
cmake --preset tsan
cmake --build --preset tsan
ctest --preset tsan --output-on-failure
```

## Benchmarks

To build and run Google Benchmark suites:
```bash
cmake --preset bench
cmake --build --preset bench

# Run allocation benchmarks
./build/bench/bench_alloc

# Run key-value throughput benchmarks
./build/bench/bench_kv

# Or use the comparison automation script
./benchmarks/compare_allocators.sh
```

## Code Style & Formatting

I adhere to Google C++ Style with 4-space indentation and a 100-character line limit.
Format all modified files before committing:
```bash
clang-format -i include/slabcache/*.hpp src/*.cpp tests/*.cpp benchmarks/*.cpp
```

## Commit Hygiene Rules

1. **One logical change per commit.** Do not mix allocator changes with protocol changes.
2. **Imperative mood.** `feat: add buddy coalescing` not `Added buddy coalescing`.
3. **Scope the commit.** `feat(slab): add per-size-class free list`, `feat(resp3): add % map type`.
4. **Include benchmark deltas** when performance changes: `perf(slab): 12ns/op — 3.7x faster than glibc malloc`.
5. **Never commit binaries** or build artifacts (`build/`, `.o`, `perf.data`).
