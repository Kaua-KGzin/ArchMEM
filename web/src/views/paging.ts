// Paging lab: scenario on the left, the simulation one access at a time on the right.
import { alertBox, clear, h, pct } from "../dom";
import type { Engine } from "../engine";
import { ALG_LABEL, ALG_RULE, explain } from "../i18n";
import { stepLimitError, stripWindow } from "../limits";
import { hashParam, scenario } from "../state";
import { timeline, type Timeline } from "../timeline";
import type { RunResponse, Scenario, Step } from "../types";
import { scenarioForm } from "./form";
import type { View } from "./view";

const bits = (n: number) => Math.max(1, Math.ceil(Math.log2(Math.max(2, n))));
const bin = (v: number, width: number) => v.toString(2).padStart(width, "0");

function addressPanel(s: Step, r: RunResponse): HTMLElement {
  const pb = bits(r.config.virtual_pages);
  const ob = Math.log2(r.config.page_size);
  return h(
    "section",
    { class: "panel", "aria-labelledby": "p-addr" },
    h("h3", { id: "p-addr" }, "Endereço virtual"),
    h(
      "p",
      { class: "addr" },
      h("span", { class: "num" }, s.address),
      " = ",
      h("span", { class: "bits page-bits", title: `${pb} bits de página` }, bin(s.page, pb)),
      h("span", { class: "bits off-bits", title: `${ob} bits de deslocamento` }, bin(s.offset, ob)),
    ),
    h(
      "p",
      { class: "muted" },
      `página ${s.page} · deslocamento ${s.offset} · `,
      s.frame >= 0 ? `endereço físico ${s.frame * r.config.page_size + s.offset} (frame ${s.frame})` : "",
    ),
  );
}

function tlbPanel(s: Step, r: RunResponse): HTMLElement | null {
  if (r.config.tlb_entries === 0) return null;
  const status = s.tlb === "hit" ? "hit" : "miss";
  return h(
    "section",
    { class: "panel", "aria-labelledby": "p-tlb" },
    h("h3", { id: "p-tlb" }, "TLB ", h("span", { class: `badge ${status}` }, s.tlb === "hit" ? "✓ hit" : "✗ miss")),
    h(
      "ul",
      { class: "tlb" },
      ...Array.from({ length: r.config.tlb_entries }, (_, i) => {
        const p = s.tlb_pages[i];
        return h("li", { class: p === s.page ? "current" : "" }, p === undefined ? "—" : `pág. ${p}`);
      }),
    ),
  );
}

function framesPanel(s: Step, r: RunResponse): HTMLElement {
  const alg = r.config.algorithm;
  return h(
    "section",
    { class: "panel frames-panel", "aria-labelledby": "p-frames" },
    h("h3", { id: "p-frames" }, "Memória física (frames)"),
    h(
      "ol",
      { class: "frames", start: 0 },
      ...s.frames.map((f, i) => {
        const isCur = i === s.frame;
        const cls = ["frame", f ? "" : "free", isCur ? (s.result === "hit" ? "cur-hit" : "cur-fault") : ""].join(" ");
        const pointer = alg === "clock" && i === s.hand ? h("span", { class: "hand", title: "ponteiro do Clock" }, "➜") : null;
        return h(
          "li",
          { class: cls, "aria-current": isCur ? "true" : null },
          pointer,
          h("span", { class: "frame-no" }, `frame ${i}`),
          f
            ? [
                h("strong", { class: "frame-page" }, `pág. ${f.page}`),
                h(
                  "span",
                  { class: "frame-meta" },
                  alg === "clock" ? `ref ${f.ref}` : alg === "lfu" ? `usos ${f.count}` : alg === "fifo" ? `entrou ${f.loaded}` : `uso ${f.last}`,
                  f.dirty ? h("span", { class: "dirty", title: "página suja (escrita)" }, " ✎ suja") : null,
                ),
              ]
            : h("span", { class: "frame-page muted" }, "livre"),
          isCur && s.victim ? h("span", { class: "evicted" }, `saiu pág. ${s.victim.page}${s.victim.dirty ? " (write-back)" : ""}`) : null,
        );
      }),
    ),
  );
}

function pageTablePanel(s: Step, r: RunResponse, seen: number[]): HTMLElement {
  const resident = new Map<number, number>();
  s.frames.forEach((f, i) => f && resident.set(f.page, i));
  const dirty = new Set(s.frames.filter((f) => f?.dirty).map((f) => f!.page));
  const pages = r.config.virtual_pages <= 32 ? Array.from({ length: r.config.virtual_pages }, (_, i) => i) : seen;
  return h(
    "section",
    { class: "panel", "aria-labelledby": "p-pt" },
    h("h3", { id: "p-pt" }, "Tabela de páginas"),
    h(
      "table",
      { class: "pt" },
      h("thead", {}, h("tr", {}, h("th", { scope: "col" }, "pág."), h("th", { scope: "col" }, "válida"), h("th", { scope: "col" }, "frame"), h("th", { scope: "col" }, "suja"))),
      h(
        "tbody",
        {},
        ...pages.map((p) =>
          h(
            "tr",
            { class: p === s.page ? "current" : "" },
            h("td", {}, p),
            h("td", {}, resident.has(p) ? "1" : "0"),
            h("td", {}, resident.has(p) ? resident.get(p)! : "—"),
            h("td", {}, dirty.has(p) ? "1" : "0"),
          ),
        ),
      ),
    ),
  );
}

function refStrip(r: RunResponse, index: number, onPick: (i: number) => void): HTMLElement {
  const { from, to } = stripWindow(r.steps.length, index);
  return h(
    "ol",
    { class: "refstrip", "aria-label": "Sequência de acessos", start: from + 1 },
    from > 0 ? h("li", { class: "muted", "aria-hidden": "true" }, "…") : null,
    ...r.steps.slice(from, to).map((s, j) => {
      const i = from + j;
      return h(
        "li",
        {},
        h(
          "button",
          {
            type: "button",
            class: ["ref", i === index ? "current" : "", i <= index ? (s.result === "hit" ? "was-hit" : "was-fault") : ""].join(" "),
            "aria-label": `passo ${i + 1}: página ${s.page}${s.op === "W" ? " escrita" : ""}${i <= index ? (s.result === "hit" ? ", hit" : ", fault") : ""}`,
            onclick: () => onPick(i),
          },
          `${s.page}${s.op === "W" ? "w" : ""}`,
          i <= index ? h("span", { class: "mark", "aria-hidden": "true" }, s.result === "hit" ? "✓" : "✗") : null,
        ),
      );
    }),
    to < r.steps.length ? h("li", { class: "muted", "aria-hidden": "true" }, "…") : null,
  );
}

/** Faults and write-backs after each step, computed once per simulation. */
function prefixTotals(r: RunResponse): { faults: number[]; writebacks: number[] } {
  const faults: number[] = [], writebacks: number[] = [];
  let f = 0, w = 0;
  for (const s of r.steps) {
    if (s.result === "fault") f++;
    if (s.victim?.dirty) w++;
    faults.push(f);
    writebacks.push(w);
  }
  return { faults, writebacks };
}

function runningTotals(r: RunResponse, sums: ReturnType<typeof prefixTotals>, index: number): HTMLElement {
  const done = { length: index + 1 };
  const faults = sums.faults[index];
  const writebacks = sums.writebacks[index];
  const stat = (label: string, value: string, cls = "") => h("div", { class: `stat ${cls}` }, h("span", { class: "stat-v" }, value), h("span", { class: "stat-l" }, label));
  return h(
    "div",
    { class: "stats", "aria-live": "polite" },
    stat("page faults", `${faults}`, "fault"),
    stat("hits", `${done.length - faults}`, "hit"),
    stat("taxa de acerto", pct(done.length ? (done.length - faults) / done.length : 0)),
    stat("write-backs", `${writebacks}`),
    stat(`total (${r.steps.length} acessos)`, `${r.totals.faults} faults`),
  );
}

export function pagingView(engine: Engine): View {
  const out = h("div", { class: "sim" });
  let tl: Timeline | null = null;
  // "Ver a resolução" links open at a given step: #/paginacao?s=...&passo=9
  let startAt = Math.max(1, Number(hashParam(location.hash, "passo")) || 1) - 1;

  const simulate = (s: Scenario) => {
    scenario.set(s);
    tl?.destroy();
    tl = null;
    clear(out);
    const tooLong = stepLimitError(s.refs);
    if (tooLong) {
      out.append(alertBox(tooLong));
      return;
    }
    const res = engine.run<RunResponse>({ action: "run", config: s.config, refs: s.refs });
    if (!res.ok) {
      out.append(alertBox(res.error));
      return;
    }
    const seen = [...new Set(res.steps.map((x) => x.page))].sort((a, b) => a - b);
    const sums = prefixTotals(res);
    const body = h("div", { class: "step-body" });
    const strip = h("div", {});
    const totals = h("div", {});
    tl = timeline(res.steps.length, (i) => {
      const st = res.steps[i];
      clear(body);
      clear(strip);
      clear(totals);
      strip.append(refStrip(res, i, (j) => tl?.set(j)));
      totals.append(runningTotals(res, sums, i));
      body.append(
        h(
          "section",
          { class: `explain ${st.result}`, "aria-live": "polite" },
          h("span", { class: `badge ${st.result}` }, st.result === "hit" ? "✓ hit" : "✗ page fault"),
          h("p", {}, explain(st)),
        ),
        h("div", { class: "panels" }, addressPanel(st, res), tlbPanel(st, res) ?? "", framesPanel(st, res), pageTablePanel(st, res, seen)),
      );
    }, startAt);
    startAt = 0;
    out.append(
      h("p", { class: "rule" }, h("strong", {}, ALG_LABEL[res.config.algorithm] + ": "), ALG_RULE[res.config.algorithm]),
      totals,
      strip,
      tl.el,
      body,
    );
  };

  const form = scenarioForm(engine, scenario.get(), {
    showAlgorithm: true,
    submitLabel: "Simular",
    onSubmit: simulate,
    onError: (msg) => {
      clear(out);
      out.append(alertBox(msg));
    },
  });
  simulate(scenario.get());
  return {
    el: h("div", { class: "layout" }, form, out),
    destroy: () => tl?.destroy(),
  };
}
