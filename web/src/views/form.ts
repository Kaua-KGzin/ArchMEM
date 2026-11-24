// Scenario editor shared by the paging, compare and curve views.
import { h, numberField, selectField } from "../dom";
import type { Engine } from "../engine";
import { ALG_LABEL } from "../i18n";
import { EXAMPLES } from "../state";
import { ALGORITHMS, type Algorithm, type GenResponse, type Scenario } from "../types";

export interface FormOptions {
  showAlgorithm: boolean;
  submitLabel: string;
  onSubmit(s: Scenario): void;
  onError(message: string): void;
}

export function scenarioForm(engine: Engine, initial: Scenario, opts: FormOptions): HTMLFormElement {
  const c = initial.config;
  const frames = numberField("Frames", c.frames, { id: "f-frames", min: 1, max: 64 });
  const pageSize = selectField(
    "Tamanho da página",
    "f-page-size",
    [16, 32, 64, 128, 256, 512, 1024, 4096].map((n) => [String(n), `${n} B`]),
    String(c.page_size),
  );
  const vpages = numberField("Páginas virtuais", c.virtual_pages, { id: "f-vpages", min: 1, max: 4096 });
  const tlb = numberField("TLB (entradas)", c.tlb_entries, { id: "f-tlb", min: 0, max: 64, hint: "0 = sem TLB" });
  const alg = selectField("Algoritmo", "f-alg", ALGORITHMS.map((a) => [a, ALG_LABEL[a]]), c.algorithm);
  const refs = h("textarea", { id: "f-refs", rows: 3, spellcheck: "false", "aria-describedby": "f-refs-help" }, initial.refs);
  refs.value = initial.refs;

  const read = (): Scenario => ({
    config: {
      frames: Number(frames.input.value),
      page_size: Number(pageSize.select.value),
      virtual_pages: Number(vpages.input.value),
      tlb_entries: Number(tlb.input.value),
      algorithm: alg.select.value as Algorithm,
    },
    refs: refs.value,
  });
  const fill = (s: Scenario) => {
    frames.input.value = String(s.config.frames);
    pageSize.select.value = String(s.config.page_size);
    vpages.input.value = String(s.config.virtual_pages);
    tlb.input.value = String(s.config.tlb_entries);
    alg.select.value = s.config.algorithm;
    refs.value = s.refs;
  };

  const genKind = selectField(
    "Gerar sequência",
    "f-gen-kind",
    [
      ["locality", "Localidade (working set)"],
      ["uniform", "Aleatória"],
      ["loop", "Laço"],
      ["sequential", "Varredura sequencial"],
    ],
    "locality",
  );
  const genLen = numberField("Tamanho", 30, { id: "f-gen-len", min: 1, max: 10000 });
  const genButton = h("button", { type: "button", class: "secondary" }, "Gerar");
  genButton.addEventListener("click", () => {
    const res = engine.run<GenResponse>({
      action: "gen",
      kind: genKind.select.value,
      length: Number(genLen.input.value),
      pages: Math.min(Number(vpages.input.value) || 16, 4096),
      seed: Math.floor(Math.random() * 1e9),
      write_ratio: 0.2,
    });
    if (!res.ok) return opts.onError(res.error);
    refs.value = res.refs;
    opts.onSubmit(read());
  });

  const examples = h(
    "div",
    { class: "examples", role: "group", "aria-label": "Exemplos" },
    ...EXAMPLES.map((ex) =>
      h(
        "button",
        {
          type: "button",
          class: "chip",
          title: ex.hint,
          onclick: () => {
            fill(ex.scenario);
            opts.onSubmit(read());
          },
        },
        ex.title,
      ),
    ),
  );

  const form = h(
    "form",
    { class: "scenario-form", "aria-label": "Cenário" },
    h("h2", {}, "Cenário"),
    examples,
    h("div", { class: "grid-2" }, frames.field, opts.showAlgorithm ? alg.field : null, pageSize.field, vpages.field, tlb.field),
    h(
      "label",
      { class: "field", for: "f-refs" },
      h("span", {}, "Sequência de acessos"),
      refs,
      h(
        "small",
        { id: "f-refs-help" },
        "Ex.: 7 0 1 2w @130 — número = página lida, w = escrita, @ = endereço virtual (decimal ou 0x).",
      ),
    ),
    h("details", { class: "gen" }, h("summary", {}, "Gerar uma sequência"), h("div", { class: "grid-2" }, genKind.field, genLen.field), genButton),
    h("button", { type: "submit", class: "primary" }, opts.submitLabel),
  );
  form.addEventListener("submit", (e) => {
    e.preventDefault();
    opts.onSubmit(read());
  });
  return form;
}
