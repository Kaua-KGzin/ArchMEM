import { describe, expect, it } from "vitest";
import { DEFAULT_SCENARIO, decodeScenario, encodeScenario, scenarioFromHash } from "../state";

describe("scenario links", () => {
  it("scenario round-trips through the hash", () => {
    const s = {
      config: { page_size: 128, frames: 4, virtual_pages: 32, tlb_entries: 2, algorithm: "clock" as const },
      refs: "1 2w @300 4",
    };
    const encoded = encodeScenario(s);
    expect(encoded.startsWith("v1.")).toBe(true);
    expect(encoded).not.toMatch(/[+/=]/);
    expect(decodeScenario(encoded)).toEqual(s);
    expect(scenarioFromHash(`#/paginacao?s=${encoded}`)).toEqual(s);
  });

  it("hash with garbage falls back to default scenario", () => {
    expect(decodeScenario("v1.@@@")).toBeNull();
    expect(decodeScenario("v2.abc")).toBeNull();
    expect(decodeScenario(encodeScenario(DEFAULT_SCENARIO).replace("v1.", "v1.x"))).toBeNull();
    expect(scenarioFromHash("#/paginacao?s=v1.@@@")).toEqual(DEFAULT_SCENARIO);
    expect(scenarioFromHash("")).toEqual(DEFAULT_SCENARIO);
  });

  it("rejects decoded objects with the wrong shape", () => {
    const bad = "v1." + btoa(JSON.stringify({ config: { frames: "3" }, refs: 5 })).replace(/=+$/, "");
    expect(decodeScenario(bad)).toBeNull();
  });
});
