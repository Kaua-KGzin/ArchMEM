// Inline SVG line chart (no library). Series are told apart by color AND
// dash pattern, so the chart still reads for color-blind students and in print.

const NS = "http://www.w3.org/2000/svg";
const COLORS = ["#5b6cf0", "#e5484d", "#2fbf71", "#e5b53a", "#b86bff"];
const DASHES = ["", "6 4", "2 4", "10 4 2 4", "1 3"];

export interface Series {
  name: string;
  values: number[];
}

export interface ChartOptions {
  title?: string;
  xLabel?: string;
  yLabel?: string;
  markers?: { series: string; x: number | string; label: string }[];
}

function el<K extends keyof SVGElementTagNameMap>(tag: K, attrs: Record<string, string | number> = {}, text?: string) {
  const node = document.createElementNS(NS, tag);
  for (const [k, v] of Object.entries(attrs)) node.setAttribute(k, String(v));
  if (text !== undefined) node.textContent = text;
  return node;
}

export function lineChart(series: Series[], xLabels: (string | number)[], opts: ChartOptions = {}): SVGSVGElement {
  const W = 640, H = 320;
  const m = { top: 44, right: 20, bottom: 48, left: 48 }; // top band holds the legend
  const iw = W - m.left - m.right, ih = H - m.top - m.bottom;
  const peak = Math.max(1, ...series.flatMap((s) => s.values));
  const step = Math.max(1, Math.ceil(peak / 5)); // integer, evenly spaced ticks
  const max = step * Math.ceil(peak / step);
  const n = xLabels.length;
  const x = (i: number) => m.left + (n <= 1 ? iw / 2 : (i / (n - 1)) * iw);
  const y = (v: number) => m.top + ih - (v / max) * ih;

  const svg = el("svg", { viewBox: `0 0 ${W} ${H}`, role: "img", class: "chart" });
  const title = opts.title ?? "Gráfico";
  svg.append(el("title", {}, title));
  const desc = series.map((s) => `${s.name}: ${s.values.join(", ")}`).join("; ");
  svg.append(el("desc", {}, `${opts.xLabel ?? "x"}: ${xLabels.join(", ")}. ${desc}.`));

  // grid + y ticks
  for (let v = 0; v <= max; v += step) {
    svg.append(el("line", { x1: m.left, x2: m.left + iw, y1: y(v), y2: y(v), class: "grid" }));
    svg.append(el("text", { x: m.left - 8, y: y(v) + 4, "text-anchor": "end", class: "tick" }, String(v)));
  }
  xLabels.forEach((label, i) =>
    svg.append(el("text", { x: x(i), y: m.top + ih + 18, "text-anchor": "middle", class: "tick" }, String(label))),
  );
  if (opts.xLabel) svg.append(el("text", { x: m.left + iw / 2, y: H - 8, "text-anchor": "middle", class: "axis" }, opts.xLabel));
  if (opts.yLabel)
    svg.append(el("text", { x: 14, y: m.top + ih / 2, "text-anchor": "middle", class: "axis", transform: `rotate(-90 14 ${m.top + ih / 2})` }, opts.yLabel));

  series.forEach((s, si) => {
    const color = COLORS[si % COLORS.length];
    const d = s.values.map((v, i) => `${i ? "L" : "M"}${x(i).toFixed(1)},${y(v).toFixed(1)}`).join(" ");
    const path = el("path", { d, class: "series", fill: "none", stroke: color, "stroke-width": 2.5 });
    if (DASHES[si % DASHES.length]) path.setAttribute("stroke-dasharray", DASHES[si % DASHES.length]);
    svg.append(path);
    s.values.forEach((v, i) => {
      const dot = el("circle", { cx: x(i), cy: y(v), r: 3.5, fill: color });
      dot.append(el("title", {}, `${s.name}, ${xLabels[i]}: ${v}`));
      svg.append(dot);
    });
  });

  for (const mk of opts.markers ?? []) {
    const si = series.findIndex((s) => s.name === mk.series);
    const xi = xLabels.findIndex((l) => String(l) === String(mk.x));
    if (si < 0 || xi < 0) continue;
    const g = el("g", { class: "marker" });
    const cx = x(xi), cy = y(series[si].values[xi]);
    g.append(el("circle", { cx, cy, r: 9, fill: "none", stroke: COLORS[si % COLORS.length], "stroke-width": 2 }));
    g.append(el("text", { x: cx, y: Math.max(cy - 14, 12), "text-anchor": "middle", class: "marker-label" }, mk.label));
    svg.append(g);
  }

  // legend
  const legend = el("g", { class: "legend" });
  series.forEach((s, si) => {
    const lx = m.left + si * 112, ly = 14;
    const line = el("line", { x1: lx, x2: lx + 22, y1: ly, y2: ly, stroke: COLORS[si % COLORS.length], "stroke-width": 2.5 });
    if (DASHES[si % DASHES.length]) line.setAttribute("stroke-dasharray", DASHES[si % DASHES.length]);
    legend.append(line, el("text", { x: lx + 28, y: ly + 4, class: "legend-label" }, s.name));
  });
  svg.append(legend);
  return svg;
}
