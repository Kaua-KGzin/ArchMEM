// Self-check exercises: the engine draws a scenario and knows the answer.
import { alertBox, clear, h, selectField } from "../dom";
import type { Engine } from "../engine";
import { ALG_LABEL } from "../i18n";
import { checkAnswer, loadProgress, recordAttempt, type Progress } from "../progress";
import { encodeScenario, scenario } from "../state";
import type { Challenge, ChallengeResponse } from "../types";
import type { View } from "./view";

function question(c: Challenge): string {
  const alg = c.params.algorithm ? ALG_LABEL[c.params.algorithm] : "";
  if (c.kind === "victim") return `Com ${c.params.frames} frames e ${alg}, qual página sai da memória no passo ${c.params.step}?`;
  if (c.kind === "best_algorithm")
    return `Com ${c.params.frames} frames, qual destes faz MENOS page faults: FIFO, LRU, Clock ou LFU? (responda fifo, lru, clock ou lfu)`;
  return `Com ${c.params.frames} frames e ${alg}, quantos page faults acontecem?`;
}

function progressLine(p: Progress): HTMLElement {
  return h(
    "p",
    { class: "muted", "aria-live": "polite" },
    `Acertos: ${p.solved} de ${p.attempted} · sequência atual: ${p.streak} · melhor sequência: ${p.best}`,
  );
}

export function challengesView(engine: Engine): View {
  const out = h("div", { class: "sim challenge" });
  const level = selectField("Nível", "c-level", [["1", "1 · contar faults"], ["2", "2 · quem sai?"], ["3", "3 · qual algoritmo?"]], "1");
  const progress = h("div", {}, progressLine(loadProgress()));

  const next = () => {
    clear(out);
    const seed = crypto.getRandomValues(new Uint32Array(1))[0] % 2147483647;
    const res = engine.run<ChallengeResponse>({ action: "challenge", seed, difficulty: Number(level.select.value) });
    if (!res.ok) return void out.append(alertBox(res.error));
    const c = res.challenge;
    const input = h("input", { id: "c-answer", autocomplete: "off", inputmode: c.kind === "best_algorithm" ? "text" : "numeric" });
    const feedback = h("div", { "aria-live": "assertive" });
    const solution = () => {
      scenario.set(c.scenario);
      const step = c.kind === "victim" ? `&passo=${c.params.step}` : "";
      location.hash = `#/paginacao?s=${encodeScenario(c.scenario)}${step}`;
    };
    let answered = false;
    const form = h(
      "form",
      { class: "card" },
      h("p", { class: "q" }, question(c)),
      h("p", {}, "Sequência: ", h("code", { class: "refs" }, c.scenario.refs)),
      h("label", { class: "field", for: "c-answer" }, h("span", {}, "Sua resposta"), input),
      h("div", { class: "row" }, h("button", { type: "submit", class: "primary" }, "Conferir"), h("button", { type: "button", onclick: next }, "Outro desafio")),
      feedback,
    );
    form.addEventListener("submit", (e) => {
      e.preventDefault();
      if (!input.value.trim()) return input.focus();
      const ok = checkAnswer(input.value, c.answer);
      if (!answered) {
        answered = true;
        clear(progress);
        progress.append(progressLine(recordAttempt(ok)));
      }
      clear(feedback);
      feedback.append(
        h(
          "div",
          { class: `explain ${ok ? "hit" : "fault"}` },
          h("span", { class: `badge ${ok ? "hit" : "fault"}` }, ok ? "✓ certo" : "✗ ainda não"),
          h(
            "p",
            {},
            ok ? "Isso mesmo! " : `A resposta é ${typeof c.answer === "string" ? ALG_LABEL[c.answer as keyof typeof ALG_LABEL] ?? c.answer : c.answer}. `,
            h("button", { type: "button", class: "linkish", onclick: solution }, "Ver a resolução passo a passo →"),
          ),
        ),
      );
    });
    out.append(form);
    input.focus();
  };

  level.select.addEventListener("change", next);
  next();
  return {
    el: h(
      "div",
      { class: "layout" },
      h(
        "aside",
        { class: "scenario-form" },
        h("h2", {}, "Desafios"),
        h("p", { class: "muted" }, "Resolva no papel e confira. A resposta vem do mesmo motor do passo a passo, então a resolução sempre bate."),
        level.field,
        progress,
      ),
      out,
    ),
  };
}
