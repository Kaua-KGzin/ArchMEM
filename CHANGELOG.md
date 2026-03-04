# Changelog

All notable changes to this project will be documented in this file.

The format is based on Keep a Changelog, and this project follows Semantic Versioning.

## [1.1.0] - 2026-03-04
### Added
- LRU page replacement when all physical frames are occupied.
- Byte-level `read_byte` and `write_byte` memory operations.
- Runtime metrics for faults, hits, replacements, reads, writes, and segment allocation results.
- CLI flags: `--help` and `--version`.
- Sanitizer build target in `Makefile`.

### Changed
- Paging model now separates virtual page count from physical frame count.
- Segment allocation now uses first-fit over contiguous free gaps.
- Documentation rewritten for GitHub release quality.

### Fixed
- Removed overlapping/unsafe segment placement strategy based on a fixed multiplier.
- Added stronger validation for page numbers, offsets, and segment bounds.

## [1.0.0] - 2026-02-03
### Added
- Initial academic simulator with page table, segment table, and memory usage visualization.
