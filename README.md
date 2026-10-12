# Slabcache

A production‑grade hybrid memory allocator (slab + arena + buddy) embedded inside a RESP3 key‑value store.

This repository currently contains the initial scaffolding:
- CMake build system (`CMakeLists.txt`, `CMakePresets.json`).
- Stub implementations for the slab, arena, and buddy allocators.
- Basic `.gitignore`, clang‑format, and clang‑tidy configuration.
- Placeholder CI workflow (`.github/workflows/ci.yml`).
- MIT license.

Further development will flesh out the allocators, thread‑local caches, RESP3 server, benchmarks, and documentation as outlined in the project roadmap.
