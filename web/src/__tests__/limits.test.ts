import { describe, expect, it } from "vitest";
import { MAX_STEP_REFS, countRefs, stepLimitError, stripWindow } from "../limits";

describe("large inputs", () => {
  it("step view refuses more than 2000 accesses with a clear message", () => {
    expect(MAX_STEP_REFS).toBe(2000);
    expect(countRefs("1 2, 3\n4w @10")).toBe(5);
    expect(stepLimitError(Array(2000).fill("1").join(" "))).toBeNull();
    const msg = stepLimitError(Array(2001).fill("1").join(" "));
    expect(msg).toContain("2000");
    expect(msg).toContain("Comparar");
  });

  it("shows only a window of the reference strip around the current step", () => {
    expect(stripWindow(10, 3)).toEqual({ from: 0, to: 10 });
    const w = stripWindow(10000, 5000);
    expect(w.to - w.from).toBeLessThanOrEqual(121);
    expect(w.from).toBeLessThanOrEqual(5000);
    expect(w.to).toBeGreaterThan(5000);
    expect(stripWindow(10000, 0).from).toBe(0);
    expect(stripWindow(10000, 9999).to).toBe(10000);
  });
});
