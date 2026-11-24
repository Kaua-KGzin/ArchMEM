// Answer checking and the student's progress, kept in localStorage.

export interface Progress {
  solved: number;
  attempted: number;
  streak: number;
  best: number;
}

const KEY = "archmem.progress.v1";
const EMPTY: Progress = { solved: 0, attempted: 0, streak: 0, best: 0 };

export function checkAnswer(input: string, answer: number | string): boolean {
  const given = input.trim().toLowerCase();
  if (!given) return false;
  if (typeof answer === "number") return /^-?\d+$/.test(given) && Number(given) === answer;
  return given === answer.toLowerCase();
}

export function loadProgress(): Progress {
  try {
    const raw = localStorage.getItem(KEY);
    if (!raw) return { ...EMPTY };
    const p = JSON.parse(raw) as Partial<Progress>;
    const n = (v: unknown) => (Number.isInteger(v) && (v as number) >= 0 ? (v as number) : 0);
    return { solved: n(p.solved), attempted: n(p.attempted), streak: n(p.streak), best: n(p.best) };
  } catch {
    return { ...EMPTY };
  }
}

export function recordAttempt(correct: boolean): Progress {
  const p = loadProgress();
  p.attempted++;
  if (correct) {
    p.solved++;
    p.streak++;
    p.best = Math.max(p.best, p.streak);
  } else {
    p.streak = 0;
  }
  try {
    localStorage.setItem(KEY, JSON.stringify(p));
  } catch {
    /* private mode / quota: progress just isn't kept */
  }
  return p;
}
