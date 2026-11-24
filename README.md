# ArchMEM

**Laboratório de memória virtual** para estudar Sistemas Operacionais: monte um
cenário, avance acesso por acesso e veja a TLB, a tabela de páginas e os frames
mudando — com a explicação de cada page fault e de cada página que sai.

**Use online:** https://archmem.vercel.app · parte da [linha Arch](https://archsys.vercel.app)

## O que dá para fazer

- **Paginação passo a passo** — para frente e para trás (teclado ← →, play com
  velocidade). Cada acesso mostra o endereço virtual dividido em bits de página e
  deslocamento, a TLB, os frames (com bit de referência, contador de usos, página
  suja) e a tabela de páginas, e explica em português o que aconteceu e por quê.
- **Cinco algoritmos**: FIFO, LRU, Clock (segunda chance), LFU e Ótimo (OPT).
- **Comparar** os cinco na mesma sequência, ranqueados por page faults.
- **Curva de faults × frames**, que encontra e marca a **anomalia de Belady**.
- **Segmentação** com first-fit, best-fit e worst-fit: mapa da memória, lacunas
  candidatas e fragmentação externa.
- **Desafios** gerados na hora ("quantos faults?", "qual página sai no passo 9?",
  "qual algoritmo erra menos?"), conferidos pelo mesmo motor, com a resolução
  passo a passo e o seu progresso salvo no navegador.
- **Links compartilháveis**: o cenário vai na URL.
- **CLI `archmem`** com os mesmos recursos no terminal.

Tudo roda localmente no seu navegador ou terminal; nada é enviado a servidor, e
o simulador não lê nem escreve a memória de nenhum outro processo.

## Um motor só, em C

O núcleo (`core/`) é C11 puro, sem dependências. Ele é compilado:

- **nativo**, para a CLI `archmem`;
- **para WebAssembly** (Emscripten), para o site.

Os dois falam com o motor pela mesma porta JSON, `archmem_run` (veja
[docs/api.md](docs/api.md)), e um teste de paridade exige respostas **idênticas
byte a byte** nos dois. Quer ver como o LRU ou o Clock funcionam por dentro? Leia
`core/paging.c`.

### Convenções dos algoritmos

Livros diferem em detalhes que mudam a resposta de exercícios. O ArchMEM segue o
Silberschatz e mostra a regra na tela:

| Algoritmo | Quem sai | Detalhes |
|---|---|---|
| FIFO | a que entrou primeiro | |
| LRU | a usada há mais tempo | |
| Clock | primeira com bit de referência 0 | página carregada entra com bit 1; o ponteiro começa no frame 0, zera bits 1 ao passar e para no frame seguinte ao substituído; hit só liga o bit |
| LFU | a com menos usos desde que entrou | contador zera quando a página sai; empate: a que entrou primeiro |
| OPT | a que demora mais para ser usada de novo (ou nunca) | empate: menor número de página |

Frames livres são usados em ordem crescente antes de qualquer substituição. A
TLB é totalmente associativa com LRU e perde a entrada da página que sai.

Os resultados de livro são testados (Silberschatz, 3 frames: FIFO 15, LRU 12,
OPT 9; Belady com FIFO: 9 faults com 3 frames e 10 com 4), e todos os
algoritmos são comparados com uma segunda implementação independente em 3000
sequências aleatórias.

## CLI

Baixe o binário em [Releases](https://github.com/Kaua-KGzin/ArchMEM/releases) ou
compile (`make`).

```bash
archmem run --algo lru --frames 3 --refs "7 0 1 2 0 3 0 4 2 3 0 3 2 1 2 0 1 7 0 1" --steps
archmem compare --frames 3 --refs "7 0 1 2 0 3 0 4 2 3 0 3 2 1 2 0 1 7 0 1"
archmem curve --algos fifo --max-frames 6 --refs "1 2 3 4 1 2 5 1 2 3 4 5"
archmem run --tlb 2 --page-size 256 --refs "@0 @300w @520 @0x210w"
archmem gen --kind locality --length 40 --pages 10 --seed 7
archmem seg --fit best --memory 1000 --ops "alloc A 300; alloc B 200; free A; alloc C 100"
archmem challenge --difficulty 2 --answer
```

Sequência de acessos: `7` lê a página 7, `7w` escreve nela, `@130` lê o
endereço virtual 130, `@0x82w` escreve no endereço 0x82. `--json` imprime a
resposta do motor sem formatação.

## Desenvolvimento

Requisitos: `gcc` ou `clang`, `make`, Node 24 e, para recompilar o WebAssembly,
o [Emscripten](https://emscripten.org/docs/getting_started/downloads.html).

```bash
make test        # testes do motor (C)
make sanitize    # com ASan/UBSan (Linux/macOS)
make cli-test    # testes da CLI
make wasm        # motor -> web/public/wasm (precisa do emcc)
node tests/parity.mjs   # CLI x WebAssembly, byte a byte

cd web
npm ci
npm test         # testes da interface
npm run dev      # http://localhost:5173
```

O `web/public/wasm` vai versionado para a Vercel buildar só a parte web; o CI
recompila o WebAssembly e roda a paridade a cada push.

```
core/        motor em C: paginação, algoritmos, TLB, segmentação, geradores, desafios, API JSON
cli/         archmem (CLI)
web/         site (TypeScript + Vite, sem framework)
tests/       testes do motor, da CLI e de paridade
docs/        api.md e os documentos de design
```

## Linha Arch

O ArchMEM é a peça de estudo da linha Arch (`arch.json`: id `archmem`, papel
`insight`). Ele é estático: o Arch Hub o acompanha pelo `/health` em
https://archmem.vercel.app/health. O deploy usa o
[arch-deploy](https://github.com/Kaua-KGzin/arch-deploy) (`arch-deploy.yaml`).

## Licença

MIT
