// Page faults as the number of frames grows; shows Belady's anomaly.
import { lineChart } from "../chart";
import { alertBox, clear, h, numberField } from "../dom";
import type { Engine } from "../engine";
import { ALG_LABEL } from "../i18n";
import { scenario } from "../state";
import type { CurveResponse, Scenario } from "../types";
import { scenarioForm } from "./form";
import type { View } from "./view";

export function curveView(engine: Engine): View {
  const out = h("div", { class: "sim" });
  const maxFrames = numberField("Até quantos frames", 8, { id: "c-max", min: 1, max: 64 });
  let last: Scenario = scenario.get();

  const show = (s: Scenario) => {
    last = s;
    scenario.set(s);
    clear(out);
    const res = engine.run<CurveResponse>({
      action: "curve",
      config: s.config,
      max_frames: Number(maxFrames.input.value),
      refs: s.refs,
    });
    if (!res.ok) return void out.append(alertBox(res.error));
    const chart = lineChart(
      res.series.map((x) => ({ name: ALG_LABEL[x.algorithm], values: x.faults })),
      res.frames,
      {
        title: "Page faults por número de frames",
        xLabel: "frames",
        yLabel: "page faults",
        markers: res.belady.map((b) => ({ series: ALG_LABEL[b.algorithm], x: b.at_frames, label: "anomalia de Belady" })),
      },
    );
    out.append(
      h("h2", {}, "Mais memória sempre ajuda?"),
      h("div", { class: "card chart-card" }, chart),
      res.belady.length
        ? h(
            "div",
            { class: "card note" },
            h("strong", {}, "Anomalia de Belady encontrada. "),
            res.belady
              .map((b) => `${ALG_LABEL[b.algorithm]} faz MAIS faults com ${b.at_frames} frames do que com ${b.at_frames - 1}.`)
              .join(" "),
            " Isso só acontece com algoritmos que não são \"de pilha\", como o FIFO; LRU e OPT nunca pioram com mais frames.",
          )
        : h("p", { class: "muted" }, "Nenhuma anomalia de Belady nesta sequência. Experimente o exemplo \"Anomalia de Belady\"."),
    );
  };
  const form = scenarioForm(engine, scenario.get(), {
    showAlgorithm: false,
    submitLabel: "Traçar curva",
    onSubmit: show,
    onError: (m) => (clear(out), out.append(alertBox(m))),
  });
  form.insertBefore(maxFrames.field, form.querySelector("button[type=submit]"));
  maxFrames.input.addEventListener("change", () => show(last));
  show(scenario.get());
  return { el: h("div", { class: "layout" }, form, out) };
}
