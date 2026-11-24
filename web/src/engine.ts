// Loads the C engine compiled to WebAssembly and exposes archmem_run.
import type { Response } from "./types";

interface Module {
  _archmem_run(ptr: number): number;
  _archmem_free(ptr: number): void;
  _free(ptr: number): void;
  stringToNewUTF8(s: string): number;
  UTF8ToString(ptr: number): string;
}

export interface Engine {
  run<T>(request: object): Response<T>;
}

let loading: Promise<Engine> | null = null;

export function loadEngine(): Promise<Engine> {
  if (!loading) {
    loading = (async () => {
      // Served from public/ as-is; not part of the bundle graph.
      const url = new URL("wasm/archmem.js", document.baseURI).href;
      const factory = (await import(/* @vite-ignore */ url)).default as () => Promise<Module>;
      const mod = await factory();
      return {
        run<T>(request: object): Response<T> {
          const ptr = mod.stringToNewUTF8(JSON.stringify(request));
          const out = mod._archmem_run(ptr);
          mod._free(ptr);
          const text = mod.UTF8ToString(out);
          mod._archmem_free(out);
          return JSON.parse(text) as Response<T>;
        },
      };
    })();
    loading.catch(() => {
      loading = null;
    });
  }
  return loading;
}
