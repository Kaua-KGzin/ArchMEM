// Step-through controls shared by the paging and segmentation views:
// first / previous / play-pause / next / last, speed, slider and keyboard.
import { h } from "./dom";

export interface Timeline {
  el: HTMLElement;
  set(index: number): void;
  destroy(): void;
}

export function timeline(count: number, onChange: (index: number) => void, initial = 0): Timeline {
  let index = Math.min(Math.max(initial, 0), count - 1);
  let timer: number | null = null;
  let speed = 1;

  const label = h("output", { class: "tl-label", "aria-live": "polite" });
  const slider = h("input", {
    type: "range",
    min: 0,
    max: count - 1,
    value: index,
    "aria-label": "Linha do tempo",
    class: "tl-slider",
  });
  const play = h("button", { type: "button", class: "tl-btn tl-play", "aria-label": "Reproduzir" }, "▶");

  const stop = () => {
    if (timer !== null) window.clearInterval(timer);
    timer = null;
    play.textContent = "▶";
    play.setAttribute("aria-label", "Reproduzir");
  };
  const go = (i: number) => {
    index = Math.min(Math.max(i, 0), count - 1);
    slider.value = String(index);
    label.textContent = `passo ${index + 1} de ${count}`;
    onChange(index);
    if (index === count - 1) stop();
  };
  const start = () => {
    if (index === count - 1) go(0);
    timer = window.setInterval(() => go(index + 1), 1000 / speed);
    play.textContent = "⏸";
    play.setAttribute("aria-label", "Pausar");
  };

  play.addEventListener("click", () => (timer === null ? start() : stop()));
  slider.addEventListener("input", () => {
    stop();
    go(Number(slider.value));
  });
  const btn = (text: string, aria: string, fn: () => void) =>
    h("button", { type: "button", class: "tl-btn", "aria-label": aria, title: aria, onclick: () => (stop(), fn()) }, text);

  const speedSel = h(
    "select",
    { "aria-label": "Velocidade", class: "tl-speed" },
    ...[0.5, 1, 2, 4].map((v) => h("option", { value: v, selected: v === 1 }, `${String(v).replace(".", ",")}×`)),
  );
  speedSel.addEventListener("change", () => {
    speed = Number(speedSel.value);
    if (timer !== null) {
      stop();
      start();
    }
  });

  const onKey = (e: KeyboardEvent) => {
    const t = e.target as HTMLElement | null;
    if (t && (t.tagName === "INPUT" && (t as HTMLInputElement).type !== "range" || t.tagName === "TEXTAREA" || t.tagName === "SELECT")) return;
    if (e.key === "ArrowRight") (stop(), go(index + 1));
    else if (e.key === "ArrowLeft") (stop(), go(index - 1));
    else if (e.key === "Home") (stop(), go(0));
    else if (e.key === "End") (stop(), go(count - 1));
    else if (e.key === " " && t?.tagName !== "BUTTON") (e.preventDefault(), timer === null ? start() : stop());
    else return;
    e.preventDefault();
  };
  document.addEventListener("keydown", onKey);

  const el = h(
    "div",
    { class: "timeline", role: "group", "aria-label": "Controles do passo a passo" },
    btn("⏮", "Primeiro passo", () => go(0)),
    btn("◀", "Passo anterior (←)", () => go(index - 1)),
    play,
    btn("▶|", "Próximo passo (→)", () => go(index + 1)),
    btn("⏭", "Último passo", () => go(count - 1)),
    slider,
    label,
    speedSel,
  );
  queueMicrotask(() => go(index));
  return {
    el,
    set: go,
    destroy() {
      stop();
      document.removeEventListener("keydown", onKey);
    },
  };
}
