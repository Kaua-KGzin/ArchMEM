// Runs every tests/scenarios/*.json through the native CLI (archmem --raw)
// and through the WebAssembly build, and requires byte-identical responses.
// Usage: node tests/parity.mjs [path/to/archmem]
import { execFileSync } from "node:child_process";
import { readdirSync, readFileSync, existsSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..");
const cli = process.argv[2] ?? join(root, process.platform === "win32" ? "archmem.exe" : "archmem");
const wasmJs = join(root, "web", "public", "wasm", "archmem.js");

if (!existsSync(wasmJs)) {
  console.error(`parity: ${wasmJs} nao existe (rode "make wasm")`);
  process.exit(1);
}
if (!existsSync(cli)) {
  console.error(`parity: ${cli} nao existe (rode "make")`);
  process.exit(1);
}

const { default: createArchMEM } = await import(pathToFileURL(wasmJs).href);
const mod = await createArchMEM();

function viaWasm(request) {
  const ptr = mod.stringToNewUTF8(request);
  const out = mod._archmem_run(ptr);
  mod._free(ptr);
  const text = mod.UTF8ToString(out);
  mod._archmem_free(out);
  return text;
}

function viaCli(file) {
  try {
    return execFileSync(cli, ["--raw", file], { encoding: "utf8" });
  } catch (err) {
    return err.stdout; // engine errors exit 1 but still print the response
  }
}

const dir = join(root, "tests", "scenarios");
const files = readdirSync(dir).filter((f) => f.endsWith(".json")).sort();
let failed = 0;
for (const name of files) {
  const file = join(dir, name);
  const request = readFileSync(file, "utf8"); // same bytes the CLI reads
  const a = viaCli(file);
  const b = viaWasm(request);
  if (a !== b) {
    failed++;
    let i = 0;
    while (i < a.length && a[i] === b[i]) i++;
    console.error(`parity FAIL ${name}: differ at ${i}\n  cli:  ${a.slice(i, i + 80)}\n  wasm: ${b.slice(i, i + 80)}`);
  }
}
if (failed) {
  console.error(`parity: ${failed}/${files.length} differ`);
  process.exit(1);
}
console.log(`parity ok: ${files.length}/${files.length}`);
