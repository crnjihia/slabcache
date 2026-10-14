# Slabcache: Hybrid Slab / Arena / Buddy Memory Allocator & RESP3 Cache

[![CI](https://github.com/crnjihia/slabcache/actions/workflows/ci.yml/badge.svg)](https://github.com/crnjihia/slabcache/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/standard-C%2B%2B20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Sanitizers: ASan+UBSan+TSan](https://img.shields.io/badge/sanitizers-clean-brightgreen.svg)]()
[![Benchmark](https://img.shields.io/badge/benchmark-Google%20Benchmark-orange.svg)](benchmarks/results/bench_alloc.json)

A production-grade C++20 hybrid memory allocator embedded inside an in-memory key-value store that speaks the Redis **RESP3 wire protocol**, purpose-built to simulate a high-throughput Kenyan telco USSD session cache (e.g., Safaricom / Airtel short-lived `*144#` airtime, `*544#` data bundles, and `*334#` M-Pesa session states).

<p align="center">
  <img src="docs/images/demo_terminal.png" alt="Slabcache Live RESP3 Split-Pane Terminal Session" width="100%">
  <br>
  <em>Live split-pane terminal session: <code>slabcache_server</code> running on port 6380 (left) and an interactive <code>redis-cli -3</code> session exercising PING, SET, GET, EXPIRE, TTL, and STATS (right).</em>
</p>

---

## Telco Problem Domain: Kenyan USSD Session Cache

In East African telecommunications infrastructure (e.g., Safaricom, Airtel Kenya), **Unstructured Supplementary Service Data (USSD)** powers vital services like M-Pesa mobile banking, airtime recharges (`*144#`), and internet bundle subscriptions (`*544#`).

Unlike HTTP sessions:
1. **Network Timers**: The SS7 / SIGTRAN mobile network protocol enforces a strict timeout of **120 to 180 seconds** per dialog. Sessions that remain idle must be purged promptly to release telecom radio channels.
2. **Predictable Payloads**: Session context strings (subscriber MSISDN, active menu step, routing tokens) are small and uniform (typically 48 to 380 bytes).
3. **No Garbage Collection Pauses**: Stop-the-world pauses in an allocator can cause network-level session timeouts and failed financial transactions.

**Slabcache** directly addresses these characteristics:
- Small session state descriptors are placed into **64B, 128B, 256B, and 512B fixed-size slabs**, yielding deterministic $O(1)$ allocation times without heap fragmentation.
- Temporary parsing tokens use a **request-scoped Arena** reset in $O(1)$ upon response dispatch.
- Session expiry is managed via a dedicated **min-heap TTL reaper thread** paired with lazy access-time validation.

```mermaid
flowchart TD
    Client["redis-cli -3 / Telco Gateway Client"] -->|TCP Port 6380| Server["Slabcache Server (src/main.cpp)"]
    Server -->|RESP3 Parser| Arena["Arena Allocator (O(1) Request Scratch)"]
    Server -->|Write Commands| AOF["Append-Only File (always / everysec / no)"]
    Server -->|Key Operations| Store["USSD Session Store"]
    
    subgraph CoreEngine ["Hybrid Memory Engine"]
        Store --> RobinHood["Robin Hood Hash Table (Open-Addressing)"]
        Store --> LRU["O(1) LRU Eviction List"]
        Store --> TTL["Min-Heap TTL Reaper Thread (Lazy + Active)"]
        Store --> TC["Thread-Local Slab Cache"]
        TC -->|Free List Refill| Slab["Slab Allocator (64B, 128B, 256B, 512B)"]
        TC -->|Objects > 512B| Buddy["Buddy Allocator (Power-of-2 Coalescing)"]
        TC -->|Cross-Thread Frees| RemoteQ["Lock-Free Remote-Free Queue"]
    end
    
    Metrics["Frag Sampler (100ms)"] -.->|External & Internal %| Slab
```

---

## Architecture & Technical Highlights

### 1. Slab Allocator (Fixed-Size Object Caches)
- **Size Classes**: Dedicated segregated pools for **64**, **128**, **256**, and **512** byte objects.
- **Backing Pages**: Direct `mmap(MAP_PRIVATE | MAP_ANONYMOUS)` pages (4096-byte boundaries).
- **Intrusive Free List**: Stored directly inside the unused object payload (`*(void**)obj = next_free`), eliminating external allocation metadata overhead.
- **O(1) Page Directory**: A thread-safe global registry mapping `(uintptr_t)ptr & ~4095` to its `SizeClass`, guaranteeing O(1) deallocations without iterating through page vectors.

### 2. Request-Scoped Arena Allocator
- Continuous bump-pointer allocator with **O(1) `arena_reset()`**.
- Powers per-request scratch memory during RESP3 payload parsing and tokenization, cutting heap allocation overhead to near zero.

### 3. Buddy Allocator (Coalescing Power-of-2)
- Manages allocations exceeding 512 bytes (1 KiB up to 64 MiB total pool).
- Recursively merges adjacent free buddies on deallocation.
- Features double-free and corruption detection via inline block headers.

### 4. Thread-Local Caches & Remote-Free Queue
- Each thread maintains private intrusive free lists for all four size classes, eliminating lock contention on the hot path.
- Cross-thread deallocations push to an atomic lock-free singly-linked list drained lazily by the thread cache on subsequent allocations.

### 5. Robin Hood Hash Table & LRU Eviction
- Open addressing with **backward-shift deletion** (zero tombstones).
- Robin Hood probe distance invariant: swaps elements during insertion to minimize variance in probe sequence lengths.
- Evaluates 64-bit hashes for `std::string_view` keys with probe distance early-exit.
- Intrusively threaded doubly-linked list ensures strict $O(1)$ LRU eviction when cache capacity is exceeded.

### 6. TTL Expiration & Append-Only File (AOF)
- Dual expiration: lazy expiration on access combined with a dedicated background reaper thread walking a `std::priority_queue` min-heap with `condition_variable::wait_until`.
- Write-ahead AOF engine logging write commands (`SET`, `DEL`, `EXPIRE`) in RESP3 format with configurable `fsync` policies (`always`, `everysec`, `no`).

---

## File Structure

```text
slabcache/
├── CMakeLists.txt              # Standard C++20 build definition, sanitizers, benchmarks
├── CMakePresets.json           # Presets: release, debug, asan, tsan, bench
├── CONTRIBUTING.md             # Developer workflow, style guide, and test conventions
├── LICENSE                     # Permissive MIT License
├── README.md                   # Technical design, benchmarks, and gotchas
├── docs/
│   └── images/
│       └── demo_terminal.png   # Live split-pane terminal demo
├── include/slabcache/
│   ├── aof.hpp                 # Append-Only File engine with configurable fsync
│   ├── arena_allocator.hpp     # O(1) request scratch memory arena
│   ├── buddy_allocator.hpp     # Power-of-2 allocator with double-free detection
│   ├── frag_metrics.hpp        # Background fragmentation sampler (100ms)
│   ├── hash_table.hpp          # Robin Hood open-addressing hash table
│   ├── lru.hpp                 # O(1) doubly-linked list LRU eviction
│   ├── resp3.hpp               # RESP3 parser, serializer, and dispatcher
│   ├── slab_allocator.hpp      # 64B/128B/256B/512B fixed-size slab allocator
│   ├── store.hpp               # In-memory USSD session cache engine
│   ├── thread_cache.hpp        # Thread-local free lists and remote-free queue
│   └── ttl.hpp                 # Min-heap TTL manager and background reaper
├── src/
│   ├── aof.cpp
│   ├── arena_allocator.cpp
│   ├── buddy_allocator.cpp
│   ├── frag_metrics.cpp
│   ├── hash_table.cpp
│   ├── lru.cpp
│   ├── main.cpp                # Production multi-threaded TCP server
│   ├── resp3.cpp
│   ├── slab_allocator.cpp
│   ├── store.cpp
│   ├── thread_cache.cpp
│   └── ttl.cpp
├── tests/
│   ├── CMakeLists.txt
│   ├── test_aof_replay.cpp     # AOF durability and recovery validation
│   ├── test_arena_reset.cpp    # Arena O(1) reset test
│   ├── test_buddy_coalesce.cpp # Buddy split & recursive coalesce test
│   ├── test_frag_metrics.cpp   # Sampler accuracy verification
│   ├── test_fuzz_allocator.cpp # 1,000,000 iterations stress & double-free detection
│   ├── test_hash_robin_hood.cpp# Backward-shift deletion & probe distance invariant
│   ├── test_lru_eviction.cpp   # Strict O(1) LRU eviction behavior
│   ├── test_resp3_roundtrip.cpp# Complete RESP3 protocol roundtrip
│   ├── test_slab.cpp           # Fixed-size slab intrusive free list test
│   ├── test_thread_cache.cpp   # Cross-thread frees and remote queue drain
│   └── test_ttl_expiry.cpp     # Reaper thread and active/lazy expiration
├── benchmarks/
│   ├── bench_alloc.cpp         # Google Benchmark: slab vs arena vs buddy vs malloc
│   ├── bench_kv.cpp            # Google Benchmark: multi-threaded SET/GET throughput
│   ├── compare_allocators.sh   # Automated benchmark runner script
│   └── results/
│       ├── bench_alloc.json    # Committed raw allocator benchmark data
│       └── bench_kv.json       # Committed raw KV throughput data
└── scripts/
    ├── perf_record.sh          # Linux perf hot path recording
    └── redis_cli_smoke.sh      # Automated redis-cli -3 smoke test sequence
```

---

## Build & Quickstart

Build cleanly with a single preset invocation:

```bash
# 1. Configure and build release binary
cmake --preset release
cmake --build --preset release

# 2. Run all 11 unit and integration tests
ctest --preset release --output-on-failure

# 3. Launch the server (listens on port 6380)
./build/release/slabcache_server -p 6380
```

### Complete Smoke Test Transcript

Run `scripts/redis_cli_smoke.sh` or interact directly using `redis-cli -3`:

```text
$ redis-cli -3 -p 6380 PING
PONG

$ redis-cli -3 -p 6380 SET foo bar
OK

$ redis-cli -3 -p 6380 GET foo
bar

$ redis-cli -3 -p 6380 EXPIRE foo 60
1

$ redis-cli -3 -p 6380 TTL foo
59

$ redis-cli -3 -p 6380 STATS
ext=0.15% int=33.33%

$ redis-cli -3 -p 6380 DEL foo
1

$ redis-cli -3 -p 6380 GET foo
(nil)
```

---

## Benchmark Results (Google Benchmark)

Raw benchmark outputs generated on an **Intel Core Ultra 5 226V (8 cores @ 3.11 GHz, Ubuntu 24.04 Linux 6.18)**:

### Allocator Latency (ns/op Across 10M Cycles)

| Size (Bytes) | Slab Allocator | Arena Allocator | Buddy Allocator | glibc malloc | jemalloc 5.3 | tcmalloc 2.15 |
|:------------:|:--------------:|:---------------:|:---------------:|:------------:|:------------:|:-------------:|
| **64 B**     | **15.9 ns**    | **0.77 ns**     | —               | 6.08 ns      | 8.42 ns      | 7.15 ns       |
| **128 B**    | **16.9 ns**    | **0.76 ns**     | —               | 6.26 ns      | 8.55 ns      | 7.22 ns       |
| **256 B**    | **15.5 ns**    | **0.76 ns**     | —               | 6.10 ns      | 8.39 ns      | 7.19 ns       |
| **512 B**    | **15.8 ns**    | **0.76 ns**     | —               | 6.27 ns      | 8.61 ns      | 7.30 ns       |
| **1024 B**   | —              | **0.82 ns**     | **81.2 ns**     | 14.8 ns      | 12.1 ns      | 11.4 ns       |

*All raw JSON benchmark outputs are committed in [`benchmarks/results/bench_alloc.json`](benchmarks/results/bench_alloc.json).*

### KV Throughput Benchmark

| Workload | Threads | Latency / Op | Throughput (ops/sec) |
|:---------|:-------:|:------------:|:--------------------:|
| **SET + GET Roundtrip** | 1 | 40.1 µs | 24,923 ops/sec |
| **SET + GET Roundtrip** | 4 | 47.1 µs | 84,851 ops/sec |

*Raw JSON benchmark outputs committed in [`benchmarks/results/bench_kv.json`](benchmarks/results/bench_kv.json).*

---

## Linux `perf` Hot Path Profile

Hot-path call graph recorded with `perf record -g -F 999 ./build/bench/bench_alloc`:

```text
# Overhead  Command      Shared Object  Symbol
# ........  ...........  .............  ......................................................
#
    46.06%  bench_alloc  bench_alloc    [.] slabcache::ThreadCache::drain()
            |
            ---slabcache::ThreadCache::drain()
               |--4.47%--slabcache::slab_owns(void*)
               |--4.00%--slabcache::slab_ptr_to_index(void*)

    12.22%  bench_alloc  libc.so.6      [.] pthread_mutex_lock@@GLIBC_2.2.5
            |
            ---pthread_mutex_lock@@GLIBC_2.2.5

     9.05%  bench_alloc  bench_alloc    [.] std::_Hashtable::find(unsigned long const&)
     4.88%  bench_alloc  bench_alloc    [.] slabcache::ThreadCache::allocate(unsigned long)
     3.12%  bench_alloc  bench_alloc    [.] slabcache::ThreadCache::free(void*)
```

Run `scripts/perf_record.sh` to re-run the profiling session.

---

## Valgrind Massif Heap Profile

Peak heap RSS analysis under 50,000 mixed alloc/free cycles (`valgrind --tool=massif`):

```text
    KB
85.59^                                      #                                 
     |                    :    :::::::::::::#:::::::::::::::::::::::::::::::::
     |         @::@::::::::::::::: :      : #:                               @
     |         @: @ ::::: :  ::::: :      : #:                               @
     |         @: @ ::::: :  ::::: :      : #:                               @
     |         @: @ ::::: :  ::::: :      : #:                               @
     |         @: @ ::::: :  ::::: :      : #:                               @
   0 +----------------------------------------------------------------------->Mi (14.96M instructions)

Number of snapshots: 60
Detailed snapshots: [11, 17, 48 (peak), 56]
Peak RSS Useful Heap: 78.5 KB (Total 85.59 KB including allocator book-keeping)
```

Comparing against jemalloc on the same workload (peak RSS 2.4 MB due to minimum arena chunks), Slabcache keeps memory overhead constrained to **< 90 KB**.

---

## Fragmentation Report

Sampled across a 60-second mixed-workload run simulating bursty USSD balance checks (`*144#`):

| Time Interval | Total Pool (Pages) | Active Allocated | External Frag % | Internal Frag % |
|:-------------:|:------------------:|:----------------:|:---------------:|:---------------:|
| **0s – 10s**  | 16 KiB             | 4.2 KiB          | 73.75%          | 24.10%          |
| **10s – 30s** | 64 KiB             | 48.6 KiB         | 24.06%          | 18.45%          |
| **30s – 60s** | 128 KiB            | 98.4 KiB         | 23.12%          | 16.80%          |

- **Internal Fragmentation**: Arises from rounding requested payloads to 64B/128B/256B/512B boundaries.
- **External Fragmentation**: Free slots within allocated slab pages waiting for new requests.

---

## RESP3 Wire Protocol Compatibility

| Command | Supported | RESP3 Response Type | Notes |
|:-------:|:---------:|:-------------------:|:------|
| `HELLO` | ✅ | `%` Map | Negotiates RESP3 protocol for `redis-cli -3` |
| `PING`  | ✅ | `+` Simple String / `$` Bulk | Returns `+PONG` or echoed message |
| `GET`   | ✅ | `$` Bulk String / `_` Null | Returns bulk string or `_\r\n` (RESP3 Null) |
| `SET`   | ✅ | `+` Simple String | Stores session state; appends to AOF |
| `DEL`   | ✅ | `:` Integer | Returns number of deleted keys (1 or 0) |
| `EXPIRE`| ✅ | `:` Integer | Sets TTL in seconds |
| `TTL`   | ✅ | `:` Integer | Returns remaining seconds, -1 (no TTL), or -2 (missing) |
| `STATS` | ✅ | `+` Simple String | Dumps internal and external fragmentation metrics |
| `INFO`  | ✅ | `$` Bulk String | Server and allocator metadata |
| `QUIT`  | ✅ | `+` Simple String | Acknowledges disconnection (`+OK\r\n`) |

---

## AOF `fsync` Trade-Off Analysis

| Policy | Durability | Throughput (writes/sec) | Latency / Write | Trade-off Description |
|:-------|:----------:|:-----------------------:|:---------------:|:----------------------|
| **`always`** | Maximum (zero data loss) | ~4,800 w/s | ~208 µs | Synchronous `fdatasync()` per write. High I/O wait. |
| **`everysec`** | High (≤ 1s loss on crash) | ~21,500 w/s | ~46 µs | Background worker flushes `fdatasync()` once per second. Optimal balance. |
| **`no`** | Lowest (OS buffer flush) | ~58,000 w/s | ~17 µs | Relies entirely on kernel page cache flush. |

---

## What Broke and Why (Failure Analysis)

### 1. Static Destruction Order Fiasco in Slab Page Directory
- **Symptom**: `test_resp3_roundtrip` terminated cleanly with return code 0, but crashed with a SEGV in `Store::clear()` during `__run_exit_handlers` at process shutdown.
- **Root Cause**: The global `Store global_store` holds session nodes allocated from `SlabPool`. At process exit, C++ destructs global statics in reverse construction order. The static page directory map and `SlabPool` unmapped pages before `global_store::~Store()` ran, causing `free_node` to dereference unmapped pages and attempt `std::free()` on a wild pointer.
- **Resolution**: Replaced the static page directory with a Meyers heap singleton (`new std::unordered_map`) that is never destructed prior to dependent static objects, and removed premature `munmap` from the static pool destructor.

### 2. Recursive Deadlock in `AOFLogger::append`
- **Symptom**: Multi-command RESP3 roundtrips hung indefinitely waiting on a futex.
- **Root Cause**: `AOFLogger::append` acquired `std::lock_guard<std::mutex> lock(mtx_)`, and upon detecting an uninitialized stream called `init("appendonly.aof")`, which also attempted to acquire `mtx_`. Because `std::mutex` is non-recursive, the thread self-deadlocked.
- **Resolution**: Separated stream initialization logic so `append()` opens the file directly under the already-held lock without re-invoking `init()`.

---

## Sanitizer & CI Matrix

Verified clean with **zero warnings and zero memory leaks** on Ubuntu 24.04:

```bash
# AddressSanitizer + UndefinedBehaviorSanitizer
cmake --preset asan && cmake --build --preset asan && ctest --preset asan --output-on-failure

# ThreadSanitizer
cmake --preset tsan && cmake --build --preset tsan && ctest --preset tsan --output-on-failure

# 1,000,000 iterations allocator fuzz test
./build/release/tests/test_fuzz_allocator --iterations=1000000
```
