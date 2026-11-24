# Changelog

All notable changes to this project will be documented in this file.

The format is based on Keep a Changelog, and this project follows Semantic Versioning.

## [2.0.0] - 2026-11-24
O Simuler-Mem virou **ArchMEM**, o laboratório de memória virtual da linha Arch.

### Added
- Web app (WebAssembly) com paginação passo a passo, comparação de algoritmos,
  curva de faults com anomalia de Belady, segmentação e desafios.
- Algoritmos FIFO, LRU, Clock, LFU e OPT; TLB; páginas sujas e write-back;
  endereços virtuais decompostos em página e deslocamento.
- Segmentação com first-, best- e worst-fit e fragmentação externa.
- Geradores de sequência (aleatória, localidade, laço, varredura) com semente.
- CLI `archmem` (`run`, `compare`, `curve`, `gen`, `seg`, `challenge`).
- API JSON única (`archmem_run`) compartilhada por CLI e WebAssembly, com teste
  de paridade byte a byte.
- Testes com resultados de livro e um oráculo independente para todos os
  algoritmos; CI em Linux, macOS e Windows com sanitizers.
- `arch.json`, `/health` e deploy com arch-deploy.

### Removed
- O demo de roteiro fixo `memory_sim.c` (virou exemplos de cenário).

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
