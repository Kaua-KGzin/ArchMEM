// Minimal DOM helpers: no framework, everything is plain elements.

type Child = Node | string | number | null | undefined | false;
type Attrs = Record<string, string | number | boolean | EventListener | null | undefined>;

export function h<K extends keyof HTMLElementTagNameMap>(
  tag: K,
  attrs: Attrs = {},
  ...children: (Child | Child[])[]
): HTMLElementTagNameMap[K] {
  const el = document.createElement(tag);
  for (const [key, value] of Object.entries(attrs)) {
    if (value === null || value === undefined || value === false) continue;
    if (key.startsWith("on") && typeof value === "function") {
      el.addEventListener(key.slice(2).toLowerCase(), value);
    } else if (key === "class") {
      el.className = String(value);
    } else if (value === true) {
      el.setAttribute(key, "");
    } else {
      el.setAttribute(key, String(value));
    }
  }
  append(el, children);
  return el;
}

export function append(el: Node, children: (Child | Child[])[]): void {
  for (const c of children.flat()) {
    if (c === null || c === undefined || c === false) continue;
    el.appendChild(typeof c === "string" || typeof c === "number" ? document.createTextNode(String(c)) : c);
  }
}

export function clear(el: Element): void {
  while (el.firstChild) el.removeChild(el.firstChild);
}

/** An accessible alert region for engine errors. */
export function alertBox(message: string): HTMLElement {
  return h("div", { class: "alert", role: "alert" }, h("strong", {}, "Não deu para simular: "), message);
}

/** Labelled numeric input. */
export function numberField(label: string, value: number, opts: { min: number; max: number; step?: number; id: string; hint?: string }) {
  const input = h("input", {
    type: "number",
    id: opts.id,
    min: opts.min,
    max: opts.max,
    step: opts.step ?? 1,
    value,
    inputmode: "numeric",
  });
  const field = h("label", { class: "field", for: opts.id }, h("span", {}, label), input, opts.hint ? h("small", {}, opts.hint) : null);
  return { field, input };
}

export function selectField(label: string, id: string, options: [string, string][], value: string) {
  const select = h("select", { id }, ...options.map(([v, text]) => h("option", { value: v, selected: v === value }, text)));
  const field = h("label", { class: "field", for: id }, h("span", {}, label), select);
  return { field, select };
}

export function pct(x: number): string {
  return `${(x * 100).toLocaleString("pt-BR", { maximumFractionDigits: 1 })}%`;
}
