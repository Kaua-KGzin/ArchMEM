// Every sentence the student reads. The engine only emits keys + numbers.
import type { Algorithm, Step } from "./types";

export const WHY_KEYS = [
  "hit",
  "hit_tlb",
  "fault_free_frame",
  "fault_evict_fifo",
  "fault_evict_lru",
  "fault_evict_clock",
  "fault_evict_lfu",
  "fault_evict_opt",
] as const;

export const ALG_LABEL: Record<Algorithm, string> = {
  fifo: "FIFO",
  lru: "LRU",
  clock: "Clock",
  lfu: "LFU",
  opt: "Ótimo (OPT)",
};

export const ALG_RULE: Record<Algorithm, string> = {
  fifo: "Sai a página que entrou primeiro na memória.",
  lru: "Sai a página usada há mais tempo (Least Recently Used).",
  clock:
    "Segunda chance: o ponteiro percorre os frames; bit de referência 1 vira 0 e o ponteiro avança; sai a primeira página com bit 0. Página carregada entra com bit 1.",
  lfu: "Sai a página com menos usos enquanto está na memória; empate: a que entrou primeiro.",
  opt: "Sai a página que vai demorar mais para ser usada de novo (ou nunca). Impossível na prática: serve de referência. Empate: menor número.",
};

const list = (xs: number[]) =>
  xs.length <= 1 ? String(xs[0] ?? "") : `${xs.slice(0, -1).join(", ")} e ${xs[xs.length - 1]}`;

export function explain(s: Step): string {
  const a = s.why_args;
  const page = `página ${s.page}`;
  const victim = `página ${a.victim ?? s.victim?.page}`;
  const writeback = s.victim?.dirty
    ? ` Como a ${victim} estava suja (foi escrita), ela é gravada de volta no disco (write-back) antes de sair.`
    : "";
  const write = s.op === "W" ? " É uma escrita: a página fica marcada como suja." : "";
  const fault = `Page fault: a ${page} não está na memória`;
  switch (s.why) {
    case "hit":
      return `Hit: a ${page} já está no frame ${s.frame}; nenhum acesso ao disco.${write}`;
    case "hit_tlb":
      return `Hit na TLB: a tradução da ${page} para o frame ${s.frame} estava em cache, nem foi preciso consultar a tabela de páginas.${write}`;
    case "fault_free_frame":
      return `${fault}. Ainda há frame livre, então ela é carregada no frame ${s.frame} sem tirar ninguém.${write}`;
    case "fault_evict_fifo":
      return `${fault} e não há frame livre. FIFO tira a ${victim}, que entrou primeiro (no passo ${a.loaded}).${writeback}${write}`;
    case "fault_evict_lru":
      return `${fault} e não há frame livre. LRU tira a ${victim}, usada pela última vez no passo ${a.last} — a mais antiga entre as residentes.${writeback}${write}`;
    case "fault_evict_clock": {
      const cleared = a.cleared ?? [];
      const sweep = cleared.length
        ? ` O ponteiro deu segunda chance às páginas ${list(cleared)} (bit 1 virou 0)`
        : " O ponteiro já estava sobre uma página com bit 0";
      return `${fault} e não há frame livre.${sweep} e parou na ${victim}, que sai.${writeback}${write}`;
    }
    case "fault_evict_lfu":
      return `${fault} e não há frame livre. LFU tira a ${victim}, com ${a.count} uso${a.count === 1 ? "" : "s"} desde que entrou — o menor entre as residentes (empate: a que entrou primeiro).${writeback}${write}`;
    case "fault_evict_opt":
      return `${fault} e não há frame livre. OPT tira a ${victim}, porque ${
        a.next_use === -1 ? "ela não será usada de novo" : `ela só volta a ser usada no passo ${a.next_use}, mais tarde que as outras`
      }.${writeback}${write}`;
    default:
      return `Passo ${s.step}: página ${s.page}.`;
  }
}

export const SEG_WHY: Record<string, string> = {
  seg_first_fit: "First-fit: usa a primeira lacuna (do início da memória) onde o segmento cabe.",
  seg_best_fit: "Best-fit: usa a menor lacuna onde o segmento cabe, para sobrar menos espaço.",
  seg_worst_fit: "Worst-fit: usa a maior lacuna, para que o pedaço que sobra ainda seja útil.",
  seg_free: "Libera o segmento e junta o espaço com as lacunas vizinhas.",
  seg_fail_fragmentation:
    "Falhou por fragmentação externa: existe espaço livre suficiente no total, mas nenhuma lacuna contínua é grande o bastante.",
  seg_fail_no_space: "Falhou: não há espaço livre suficiente na memória.",
};
