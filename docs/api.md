# ArchMEM engine API

One C entry point, used by the CLI and by the WebAssembly build:

```c
char *archmem_run(const char *request_json);  /* never NULL */
void  archmem_free(char *response);
```

Every request has an `"action"`. Failures come back as
`{"ok":false,"error":"<mensagem em português>"}`; nothing aborts or crashes.

| action | fields (defaults) | response |
|---|---|---|
| `run` | `config` {`page_size` 64, `frames` 3, `virtual_pages` 16, `tlb_entries` 0, `algorithm` "lru"}, `refs` (required) | `algorithm`, `config`, `totals`, `steps[]` |
| `compare` | `config`, `refs`, `algorithms` (all five) | `results[]` {`algorithm`, `totals`} in request order |
| `curve` | `config`, `refs`, `algorithms`, `max_frames` 8 | `frames[]`, `series[]` {`algorithm`, `faults[]`}, `belady[]` {`algorithm`, `at_frames`} |
| `gen` | `kind` uniform/locality/loop/sequential, `length` 20, `pages` 8, `seed` 1 (uint32), `write_ratio` 0 | `refs` |
| `seg` | `memory_size` 1024, `fit` first/best/worst, `ops` (required) | `fit`, `steps[]`, `totals` |
| `challenge` | `seed` 1 (uint32), `difficulty` 1..3 | `challenge` {`kind`, `difficulty`, `seed`, `params`, `scenario`, `answer`} |
| `version` | — | `version` |

`refs`: `7` read page 7, `7w` write, `@130` read virtual address 130, `@0x82w`
write; separators are spaces, commas or newlines; up to 10000 accesses.

Limits: page_size power of two 16..65536, frames 1..64, virtual_pages 1..4096,
tlb_entries 0..64, max_frames 1..64, memory_size 1..1048576, up to 1000 seg ops.

Each `steps[]` item of `run`:

```json
{"step":4,"op":"R","address":128,"page":2,"offset":0,"tlb":"off","result":"fault","frame":0,
 "victim":{"page":7,"frame":0,"dirty":false},
 "why":"fault_evict_lru",
 "why_args":{"victim":7,"loaded":1,"last":1,"count":1,"next_use":-1,"cleared":[]},
 "frames":[{"page":2,"ref":1,"count":1,"loaded":4,"last":4,"dirty":false},{"page":0,...},{"page":1,...}],
 "hand":0,"tlb_pages":[]}
```

`why` is one of `hit`, `hit_tlb`, `fault_free_frame`, `fault_evict_fifo`,
`fault_evict_lru`, `fault_evict_clock`, `fault_evict_lfu`, `fault_evict_opt`.
The UI turns it into a sentence (`web/src/i18n.ts`); `next_use` is only
meaningful for OPT (`-1` = never) and `cleared` for Clock (pages whose bit the
hand reset). `frames` is the state after the access (`null` = free frame).
