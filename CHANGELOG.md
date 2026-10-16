# Changelog

## [v1.2.0] - 2026-10-15
### Added
- Automated GitHub Actions release publishing workflow (`release.yml`) on version tags.
- Execution timeout limits (`timeout-minutes`) across all CI matrix and coverage jobs to prevent runner hangs.

### Changed
- Standardized documentation and contributing conventions in `CONTRIBUTING.md`.

### Fixed
- Fixed condition variable notification race in `TTLManager::stop` causing lost wakeups and thread deadlocks during rapid shutdown.
- Fixed `test_aof_replay` evaluating requests inside `assert()`, which stripped test execution in Release builds (`-DNDEBUG`).
- Ensured `AOFLogger` flushes and closes streams safely during replay and teardown.
- Added 60s per-test execution timeout and `-UNDEBUG` for test binaries in CMake.

## [v1.1.0] - 2026-10-13
### Added
- Dockerfile for containerized deployment.
- GitHub Actions job to build and push Docker image.
- README Docker usage instructions.

## [v1.0.0] - 2026-10-10
### Added
- Complete hybrid allocator (slab, arena, buddy).
- Thread‑local cache, LRU, TTL manager.
- RESP3 protocol parser and command dispatcher.
- Append‑Only File (AOF) logging.
- Benchmarks, CI workflow, coverage, documentation.
