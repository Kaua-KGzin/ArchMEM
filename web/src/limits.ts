// Keeps the step-by-step view responsive. The engine accepts 10000 accesses,
// but a per-step log that long is ~50 MB of JSON and nobody steps through it;
// Compare and Curve (totals only) still take the full 10000.

export const MAX_STEP_REFS = 2000;
const STRIP_RADIUS = 60;

export function countRefs(refs: string): number {
  return refs.split(/[\s,]+/).filter(Boolean).length;
}

export function stepLimitError(refs: string): string | null {
  const n = countRefs(refs);
  return n > MAX_STEP_REFS
    ? `o passo a passo mostra até ${MAX_STEP_REFS} acessos e esta sequência tem ${n}. Para sequências maiores (até 10000), use Comparar ou Curva de faults.`
    : null;
}

/** Slice [from, to) of the reference strip shown around the current step. */
export function stripWindow(total: number, index: number): { from: number; to: number } {
  if (total <= STRIP_RADIUS * 2 + 1) return { from: 0, to: total };
  const from = Math.max(0, Math.min(index - STRIP_RADIUS, total - (STRIP_RADIUS * 2 + 1)));
  return { from, to: Math.min(total, from + STRIP_RADIUS * 2 + 1) };
}
