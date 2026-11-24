import { beforeEach, describe, expect, it } from "vitest";
import { checkAnswer, loadProgress, recordAttempt } from "../progress";

describe("challenges", () => {
  beforeEach(() => localStorage.clear());

  it("checkAnswer accepts equivalent input", () => {
    expect(checkAnswer(" 12 ", 12)).toBe(true);
    expect(checkAnswer("12", 13)).toBe(false);
    expect(checkAnswer("", 0)).toBe(false);
    expect(checkAnswer("doze", 12)).toBe(false);
    expect(checkAnswer("LRU", "lru")).toBe(true);
    expect(checkAnswer(" lru ", "lru")).toBe(true);
    expect(checkAnswer("Ótimo", "opt")).toBe(false);
  });

  it("progress persists and streak resets on a miss", () => {
    expect(loadProgress()).toEqual({ solved: 0, attempted: 0, streak: 0, best: 0 });
    recordAttempt(true);
    recordAttempt(true);
    expect(loadProgress()).toEqual({ solved: 2, attempted: 2, streak: 2, best: 2 });
    recordAttempt(false);
    expect(loadProgress()).toEqual({ solved: 2, attempted: 3, streak: 0, best: 2 });
    expect(localStorage.getItem("archmem.progress.v1")).not.toBeNull();
  });

  it("ignores corrupted stored progress", () => {
    localStorage.setItem("archmem.progress.v1", "{not json");
    expect(loadProgress()).toEqual({ solved: 0, attempted: 0, streak: 0, best: 0 });
  });
});
