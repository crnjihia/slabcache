# Changelog

## [v1.2.0] - 2026-10-15
### Added
- Automated GitHub Actions release publishing workflow (`release.yml`) on version tags.
- Execution timeout limits (`timeout-minutes`) across all CI matrix and coverage jobs to prevent runner hangs.

### Changed
- Standardized documentation and contributing conventions in `CONTRIBUTING.md`.

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
