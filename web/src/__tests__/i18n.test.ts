import { describe, expect, it } from "vitest";
import { WHY_KEYS, explain } from "../i18n";
import type { Step } from "../types";

function step(why: string, extra: Partial<Step> = {}): Step {
  return {
    step: 4,
    op: "R",
    address: 128,
    page: 2,
    offset: 0,
    tlb: "off",
    result: why.startsWith("hit") ? "hit" : "fault",
    frame: 1,
    victim: why.startsWith("fault_evict") ? { page: 7, frame: 1, dirty: false } : null,
    why,
    why_args: { victim: 7, loaded: 1, last: 1, count: 1, next_use: -1, cleared: [7, 0] },
    frames: [],
    hand: 0,
    tlb_pages: [],
    ...extra,
  };
}

describe("explain", () => {
  it("explain covers every why key", () => {
    expect(WHY_KEYS).toHaveLength(8);
    for (const key of WHY_KEYS) {
      const text = explain(step(key));
      expect(text).not.toContain(key);
      expect(text.length).toBeGreaterThan(20);
    }
  });

  it("names the pages and the reason for an LRU eviction", () => {
    const text = explain(step("fault_evict_lru"));
    expect(text).toContain("página 2");
    expect(text).toContain("página 7");
    expect(text).toContain("passo 1");
  });

  it("mentions the write-back when the victim was dirty", () => {
    const text = explain(step("fault_evict_fifo", { victim: { page: 7, frame: 1, dirty: true } }));
    expect(text.toLowerCase()).toContain("write-back");
  });

  it("explains OPT 'never used again'", () => {
    expect(explain(step("fault_evict_opt"))).toContain("não será usada de novo");
  });
});
