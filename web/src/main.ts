import "./style.css";
import { alertBox, clear, h } from "./dom";
import { loadEngine, type Engine } from "./engine";
import { encodeScenario, scenario, scenarioFromHash } from "./state";
import { challengesView } from "./views/challenges";
import { compareView } from "./views/compare";
import { curveView } from "./views/curve";
import { pagingView } from "./views/paging";
import { segmentationView } from "./views/segmentation";
import type { View } from "./views/view";

type Route = { id: string; title: string; render: (engine: Engine) => View };

export const ROUTES: Route[] = [
  { id: "paginacao", title: "Paginação", render: pagingView },
  { id: "comparar", title: "Comparar", render: compareView },
  { id: "curva", title: "Curva de faults", render: curveView },
  { id: "segmentacao", title: "Segmentação", render: segmentationView },
  { id: "desafios", title: "Desafios", render: challengesView },
];

// Views whose state is the shared paging scenario; their links carry it.
const SHARES_SCENARIO = new Set(["paginacao", "comparar", "curva"]);

export function shareUrl(origin: string, id: string, encoded: string): string {
  return `${origin}/#/${id}${SHARES_SCENARIO.has(id) ? `?s=${encoded}` : ""}`;
}

document.getElementById("share")?.addEventListener("click", async (e) => {
  const button = e.currentTarget as HTMLButtonElement;
  const url = shareUrl(location.origin, routeId(location.hash), encodeScenario(scenario.get()));
  try {
    await navigator.clipboard.writeText(url);
    button.textContent = "Link copiado ✓";
  } catch {
    history.replaceState(null, "", url);
    button.textContent = "Link na barra de endereço";
  }
  setTimeout(() => (button.textContent = "Compartilhar"), 2500);
});

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
