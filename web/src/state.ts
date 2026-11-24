// Scenarios, examples and the shareable-link encoding.
import { ALGORITHMS, type Scenario } from "./types";

export const SILBERSCHATZ = "7 0 1 2 0 3 0 4 2 3 0 3 2 1 2 0 1 7 0 1";
export const BELADY = "1 2 3 4 1 2 5 1 2 3 4 5";

export const DEFAULT_SCENARIO: Scenario = {
  config: { page_size: 64, frames: 3, virtual_pages: 16, tlb_entries: 0, algorithm: "lru" },
  refs: SILBERSCHATZ,
};

export interface Example {
  id: string;
  title: string;
  hint: string;
  scenario: Scenario;
}

export const EXAMPLES: Example[] = [
  {
    id: "silberschatz",
    title: "Clássico do Silberschatz",
    hint: "20 referências, 3 frames: FIFO 15, LRU 12, OPT 9 faults.",
    scenario: DEFAULT_SCENARIO,
  },
  {
    id: "belady",
    title: "Anomalia de Belady",
    hint: "Com FIFO, 4 frames dão mais faults que 3.",
    scenario: { config: { ...DEFAULT_SCENARIO.config, algorithm: "fifo" }, refs: BELADY },
  },
  {
    id: "clock",
    title: "Clock passo a passo",
    hint: "Veja o ponteiro dando segunda chance.",
    scenario: { config: { ...DEFAULT_SCENARIO.config, algorithm: "clock", frames: 3 }, refs: "1 2 3 1 4 5 2 1 6 3 1 2" },
  },
  {
    id: "tlb",
    title: "Endereços, TLB e escrita",
    hint: "Endereços virtuais, TLB de 2 entradas e páginas sujas.",
    scenario: {
      config: { page_size: 256, frames: 3, virtual_pages: 16, tlb_entries: 2, algorithm: "lru" },
      refs: "@0 @300w @520 @12 @0x210w @780 @300 @1040 @5 @520",
    },
  },
];

function toBase64Url(text: string): string {
  const bytes = new TextEncoder().encode(text);
  let bin = "";
  for (const b of bytes) bin += String.fromCharCode(b);
  return btoa(bin).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/, "");
}

function fromBase64Url(text: string): string | null {
  if (!/^[A-Za-z0-9_-]*$/.test(text)) return null;
  try {
    const bin = atob(text.replace(/-/g, "+").replace(/_/g, "/"));
    return new TextDecoder("utf-8", { fatal: true }).decode(Uint8Array.from(bin, (c) => c.charCodeAt(0)));
  } catch {
    return null;
  }
}

export function encodeScenario(s: Scenario): string {
  return "v1." + toBase64Url(JSON.stringify(s));
}

function isScenario(v: unknown): v is Scenario {
  if (!v || typeof v !== "object") return false;
  const s = v as Scenario;
  const c = s.config;
  return (
    typeof s.refs === "string" &&
    !!c &&
    typeof c === "object" &&
    ["page_size", "frames", "virtual_pages", "tlb_entries"].every(
      (k) => Number.isInteger((c as unknown as Record<string, unknown>)[k]),
    ) &&
    ALGORITHMS.includes(c.algorithm)
  );
}

export function decodeScenario(encoded: string): Scenario | null {
  if (!encoded.startsWith("v1.")) return null;
  const json = fromBase64Url(encoded.slice(3));
  if (json === null) return null;
  try {
    const v: unknown = JSON.parse(json);
    if (!isScenario(v)) return null;
    const { config, refs } = v;
    return {
      config: {
        page_size: config.page_size,
        frames: config.frames,
        virtual_pages: config.virtual_pages,
        tlb_entries: config.tlb_entries,
        algorithm: config.algorithm,
      },
      refs,
    };
  } catch {
    return null;
  }
}

/** Reads ?s= from a location hash like "#/paginacao?s=v1.xxx". */
export function hashParam(hash: string, key: string): string | null {
  const q = hash.indexOf("?");
  if (q < 0) return null;
  return new URLSearchParams(hash.slice(q + 1)).get(key);
}

export function scenarioFromHash(hash: string): Scenario {
  const s = hashParam(hash, "s");
  return (s && decodeScenario(s)) || structuredClone(DEFAULT_SCENARIO);
}

/** The scenario shared by every view during a visit. */
let current: Scenario = structuredClone(DEFAULT_SCENARIO);
export const scenario = {
  get: (): Scenario => current,
  set: (s: Scenario): void => {
    current = structuredClone(s);
  },
};
