// Types of the engine's JSON contract (see docs/api.md).

export type Algorithm = "fifo" | "lru" | "clock" | "lfu" | "opt";
export const ALGORITHMS: Algorithm[] = ["fifo", "lru", "clock", "lfu", "opt"];

export interface PagingConfig {
  page_size: number;
  frames: number;
  virtual_pages: number;
  tlb_entries: number;
  algorithm: Algorithm;
}

export interface Scenario {
  config: PagingConfig;
  refs: string;
}

export interface Totals {
  accesses: number;
  faults: number;
  hits: number;
  hit_rate: number;
  replacements: number;
  writebacks: number;
  tlb_hits: number;
  tlb_misses: number;
}

export interface FrameState {
  page: number;
  ref: number;
  count: number;
  loaded: number;
  last: number;
  dirty: boolean;
}

export interface WhyArgs {
  victim?: number;
  loaded?: number;
  last?: number;
  count?: number;
  next_use?: number;
  cleared?: number[];
}

export interface Step {
  step: number;
  op: "R" | "W";
  address: number;
  page: number;
  offset: number;
  tlb: "off" | "hit" | "miss";
  result: "hit" | "fault";
  frame: number;
  victim: { page: number; frame: number; dirty: boolean } | null;
  why: string;
  why_args: WhyArgs;
  frames: (FrameState | null)[];
  hand: number;
  tlb_pages: number[];
}

export interface Failure {
  ok: false;
  error: string;
}

export interface RunResponse {
  ok: true;
  algorithm: Algorithm;
  config: PagingConfig;
  totals: Totals;
  steps: Step[];
}

export interface CompareResponse {
  ok: true;
  results: { algorithm: Algorithm; totals: Totals }[];
}

export interface CurveResponse {
  ok: true;
  frames: number[];
  series: { algorithm: Algorithm; faults: number[] }[];
  belady: { algorithm: Algorithm; at_frames: number }[];
}

export interface SegRegion {
  base: number;
  size: number;
  name: string | null;
}

export interface SegStep {
  step: number;
  op: "alloc" | "free";
  name: string;
  size: number;
  result: "ok" | "fail";
  base: number;
  chosen_hole: { base: number; size: number } | null;
  candidates: { base: number; size: number }[];
  why: string;
  map: SegRegion[];
}

export interface SegResponse {
  ok: true;
  fit: "first" | "best" | "worst";
  steps: SegStep[];
  totals: {
    memory_size: number;
    free: number;
    largest_hole: number;
    holes: number;
    failures: number;
    external_fragmentation: number;
  };
}

export interface Challenge {
  kind: "faults" | "victim" | "best_algorithm";
  difficulty: number;
  seed: number;
  params: { algorithm?: Algorithm; frames: number; step?: number; candidates?: Algorithm[] };
  scenario: Scenario;
  answer: number | string;
}

export interface ChallengeResponse {
  ok: true;
  challenge: Challenge;
}

export interface GenResponse {
  ok: true;
  refs: string;
}

export type Response<T> = T | Failure;
