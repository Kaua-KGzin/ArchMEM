// Contiguous allocation lab: first-, best- and worst-fit, step by step.
import { alertBox, clear, h, numberField, pct, selectField } from "../dom";
import type { Engine } from "../engine";
import { SEG_WHY } from "../i18n";
import { timeline, type Timeline } from "../timeline";
import type { SegResponse, SegStep } from "../types";
import type { View } from "./view";

const SEG_EXAMPLES: { title: string; memory: number; ops: string }[] = [
  {
    title: "Comparar os três fits",
    memory: 1000,
    ops: "alloc A 100\nalloc B 200\nalloc C 100\nalloc D 400\nalloc F 100\nfree B\nfree D\nalloc E 50",
  },
  {
    title: "Fragmentação externa",
    memory: 400,
    ops: "alloc A 100\nalloc B 100\nalloc C 100\nalloc D 100\nfree A\nfree C\nalloc E 150",
  },
  {
    title: "Juntando lacunas",
    memory: 600,
    ops: "alloc A 200\nalloc B 200\nalloc C 200\nfree A\nfree C\nfree B\nalloc D 500",
  },
];

let saved = { memory: SEG_EXAMPLES[0].memory, fit: "first", ops: SEG_EXAMPLES[0].ops };

function memoryBar(st: SegStep, total: number): HTMLElement {
  return h(
    "div",
    { class: "membar", role: "img", "aria-label": st.map.map((r) => `${r.name ?? "livre"} ${r.base}–${r.base + r.size - 1}`).join(", ") },
    ...st.map.map((r) => {
      const chosen = st.chosen_hole && r.name === st.name && r.base === st.base;
      return h(
        "div",
        {
          class: `blk ${r.name ? "used" : "hole"} ${chosen ? "chosen" : ""}`,
          style: `flex-grow:${r.size}`,
          title: `${r.name ?? "livre"}: base ${r.base}, ${r.size} bytes`,
        },
        r.size / total > 0.06 ? h("span", {}, r.name ?? "livre", h("small", {}, `${r.size}`)) : null,
      );
    }),
  );
}

export function segmentationView(engine: Engine): View {
  const out = h("div", { class: "sim" });
  let tl: Timeline | null = null;
  const mem = numberField("Memória (bytes)", saved.memory, { id: "s-mem", min: 1, max: 1048576 });
  const fit = selectField("Estratégia", "s-fit", [["first", "First-fit"], ["best", "Best-fit"], ["worst", "Worst-fit"]], saved.fit);
  const ops = h("textarea", { id: "s-ops", rows: 9, spellcheck: "false" });
  ops.value = saved.ops;

  const show = () => {
    saved = { memory: Number(mem.input.value), fit: fit.select.value, ops: ops.value };
    tl?.destroy();
    tl = null;
    clear(out);
    const res = engine.run<SegResponse>({ action: "seg", memory_size: saved.memory, fit: saved.fit, ops: saved.ops });
    if (!res.ok) return void out.append(alertBox(res.error));
    const body = h("div", {});
    const t = res.totals;
    tl = timeline(res.steps.length, (i) => {
      const st = res.steps[i];
      clear(body);
      const head =
        st.op === "free"
          ? `free ${st.name}: libera ${st.size} bytes a partir de ${st.base}.`
          : st.result === "ok"
            ? `alloc ${st.name} (${st.size} bytes) → base ${st.base}, na lacuna de ${st.chosen_hole?.size} bytes em ${st.chosen_hole?.base}.`
            : `alloc ${st.name} (${st.size} bytes) → falhou.`;
      body.append(
        h(
          "section",
          { class: `explain ${st.result === "ok" ? "hit" : "fault"}`, "aria-live": "polite" },
          h("span", { class: `badge ${st.result === "ok" ? "hit" : "fault"}` }, st.result === "ok" ? "✓ ok" : "✗ falhou"),
          h("p", {}, h("strong", {}, head), " ", SEG_WHY[st.why] ?? ""),
        ),
        h("div", { class: "card" }, h("h3", {}, "Memória"), memoryBar(st, res.totals.memory_size)),
        st.op === "alloc" && st.candidates.length
          ? h(
              "div",
              { class: "card" },
              h("h3", {}, "Lacunas onde cabia"),
              h(
                "ul",
                { class: "cands" },
                ...st.candidates.map((c) =>
                  h(
                    "li",
                    { class: c.base === st.chosen_hole?.base ? "chosen" : "" },
                    `base ${c.base}, ${c.size} bytes${c.base === st.chosen_hole?.base ? " ← escolhida" : ""}`,
                  ),
                ),
              ),
            )
          : "",
      );
    });
    out.append(
      h(
        "div",
        { class: "stats" },
        ...[
          ["livre no fim", `${t.free} B`],
          ["maior lacuna", `${t.largest_hole} B`],
          ["lacunas", `${t.holes}`],
          ["falhas", `${t.failures}`],
          ["fragmentação externa", pct(t.external_fragmentation)],
        ].map(([l, v]) => h("div", { class: "stat" }, h("span", { class: "stat-v" }, v), h("span", { class: "stat-l" }, l))),
      ),
      tl.el,
      body,
    );
  };

  const form = h(
    "form",
    { class: "scenario-form", "aria-label": "Alocação" },
    h("h2", {}, "Alocação contígua"),
    h(
      "div",
      { class: "examples" },
      ...SEG_EXAMPLES.map((ex) =>
        h(
          "button",
          {
            type: "button",
            class: "chip",
            onclick: () => {
              mem.input.value = String(ex.memory);
              ops.value = ex.ops;
              show();
            },
          },
          ex.title,
        ),
      ),
    ),
    h("div", { class: "grid-2" }, mem.field, fit.field),
    h("label", { class: "field", for: "s-ops" }, h("span", {}, "Operações"), ops, h("small", {}, "Uma por linha: alloc NOME TAMANHO ou free NOME.")),
    h("button", { type: "submit", class: "primary" }, "Simular"),
  );
  form.addEventListener("submit", (e) => (e.preventDefault(), show()));
  fit.select.addEventListener("change", show);
  show();
  return { el: h("div", { class: "layout" }, form, out), destroy: () => tl?.destroy() };
}
