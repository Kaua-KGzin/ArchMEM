// Compare every algorithm on the same scenario, ranked by page faults.
import { alertBox, clear, h, pct } from "../dom";
import type { Engine } from "../engine";
import { ALG_LABEL, ALG_RULE } from "../i18n";
import { scenario } from "../state";
import type { CompareResponse, Scenario } from "../types";
import { scenarioForm } from "./form";
import type { View } from "./view";

export function compareView(engine: Engine): View {
  const out = h("div", { class: "sim" });
  const show = (s: Scenario) => {
    scenario.set(s);
    clear(out);
    const res = engine.run<CompareResponse>({ action: "compare", config: s.config, refs: s.refs });
    if (!res.ok) return void out.append(alertBox(res.error));
    const rows = [...res.results].sort((a, b) => a.totals.faults - b.totals.faults);
    const worst = Math.max(1, ...rows.map((r) => r.totals.faults));
    const best = rows[0].totals.faults;
    out.append(
      h("h2", {}, `Mesma sequência, ${s.config.frames} frames: quem erra menos?`),
      h(
        "table",
        { class: "rank" },
        h(
          "thead",
          {},
          h("tr", {}, ...["#", "Algoritmo", "Page faults", "", "Taxa de acerto", "Write-backs", ""].map((t) => h("th", { scope: "col" }, t))),
        ),
        h(
          "tbody",
          {},
          ...rows.map((r, i) =>
            h(
              "tr",
              { class: r.totals.faults === best ? "best" : "" },
              h("td", {}, i + 1),
              h("th", { scope: "row", title: ALG_RULE[r.algorithm] }, ALG_LABEL[r.algorithm]),
              h("td", { class: "num" }, r.totals.faults),
              h(
                "td",
                { class: "barcell", "aria-hidden": "true" },
                h("span", { class: "bar", style: `width:${(r.totals.faults / worst) * 100}%` }),
              ),
              h("td", { class: "num" }, pct(r.totals.hit_rate)),
              h("td", { class: "num" }, r.totals.writebacks),
              h(
                "td",
                {},
                h(
                  "a",
                  {
                    href: "#/paginacao",
                    onclick: () => scenario.set({ ...s, config: { ...s.config, algorithm: r.algorithm } }),
                  },
                  "passo a passo →",
                ),
              ),
            ),
          ),
        ),
      ),
      h(
        "p",
        { class: "muted" },
        "O Ótimo (OPT) conhece o futuro, então é o limite inferior: nenhum algoritmo real faz menos faults que ele. Quanto mais perto do OPT, melhor.",
      ),
    );
  };
  const form = scenarioForm(engine, scenario.get(), {
    showAlgorithm: false,
    submitLabel: "Comparar",
    onSubmit: show,
    onError: (m) => (clear(out), out.append(alertBox(m))),
  });
  show(scenario.get());
  return { el: h("div", { class: "layout" }, form, out) };
}
