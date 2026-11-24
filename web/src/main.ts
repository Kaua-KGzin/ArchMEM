import "./style.css";
import { alertBox, clear, h } from "./dom";
import { loadEngine, type Engine } from "./engine";
import { scenario, scenarioFromHash } from "./state";
import { pagingView } from "./views/paging";
import type { View } from "./views/view";

type Route = { id: string; title: string; render: (engine: Engine) => View };

const placeholder = (title: string) => (): View => ({
  el: h("p", { class: "muted" }, `${title}: em construção.`),
});

export const ROUTES: Route[] = [
  { id: "paginacao", title: "Paginação", render: pagingView },
  { id: "comparar", title: "Comparar", render: placeholder("Comparar") },
  { id: "curva", title: "Curva de faults", render: placeholder("Curva") },
  { id: "segmentacao", title: "Segmentação", render: placeholder("Segmentação") },
  { id: "desafios", title: "Desafios", render: placeholder("Desafios") },
];

export function routeId(hash: string): string {
  const path = hash.replace(/^#\/?/, "").split("?")[0];
  return ROUTES.some((r) => r.id === path) ? path : ROUTES[0].id;
}

const main = document.getElementById("view")!;
const nav = document.getElementById("tabs")!;
let current: View | null = null;

function renderNav(active: string) {
  clear(nav);
  for (const r of ROUTES)
    nav.append(
      h("a", { href: `#/${r.id}`, class: r.id === active ? "tab active" : "tab", "aria-current": r.id === active ? "page" : null }, r.title),
    );
}

async function render() {
  const id = routeId(location.hash);
  renderNav(id);
  if (location.hash.includes("?s=")) scenario.set(scenarioFromHash(location.hash));
  try {
    const engine = await loadEngine();
    current?.destroy?.();
    clear(main);
    const route = ROUTES.find((r) => r.id === id)!;
    current = route.render(engine);
    main.append(current.el);
    document.title = `${route.title} · ArchMEM`;
  } catch (err) {
    clear(main);
    main.append(alertBox(`o motor WebAssembly não carregou (${(err as Error).message}). Recarregue a página.`));
  }
}

window.addEventListener("hashchange", render);
render();
