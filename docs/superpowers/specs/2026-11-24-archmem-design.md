# ArchMEM — laboratório de memória virtual da linha Arch

Status: rascunho para aprovação · 2026-11-24

## Intenção

O Simuler-Mem passa de demo para **produto da linha Arch**, no nível de
acabamento do ArchMAP e do System-PVD. O público é **estudantes de Sistemas
Operacionais estudando sozinhos**: o produto deve fazer a pessoa *entender*
paginação, substituição de páginas, TLB e segmentação, vendo cada acesso
acontecer e o motivo de cada decisão — e conferir o próprio raciocínio.

Hoje o repositório é um único `memory_sim.c` (~580 linhas, v1.1.0): tamanhos
fixos em `#define`, LRU apenas, um roteiro fixo em `run_simulation()` que
imprime sempre a mesma saída, sem TLB, sem entrada do usuário, sem testes.
O que vale levar: o modelo de tabela de páginas/frames/dirty bit, o LRU e o
first-fit de segmentação — reescritos como biblioteca.

## Objetivos

1. **Montar um cenário**: tamanho de página, nº de frames, bits de endereço,
   tamanho da TLB, algoritmo, e uma sequência de acessos (leitura/escrita) —
   digitada, gerada (aleatória, localidade/working set, laço, sequencial;
   com semente) ou escolhida entre exemplos clássicos.
2. **Passo a passo, para frente e para trás**: a cada acesso, ver o
   endereço virtual decomposto em página + deslocamento, a consulta à TLB, a
   tabela de páginas, os frames, e a explicação em texto do que aconteceu
   ("page fault: página 3 não residente; sem frame livre; LRU escolhe a
   página 7 (último uso no passo 4); página 7 estava suja → write-back").
3. **Comparar algoritmos** na mesma sequência: FIFO, LRU, Clock (segunda
   chance), LFU e Ótimo (OPT), lado a lado, com faults, hits, taxa de acerto
   e write-backs.
4. **Curva de faults × nº de frames** por algoritmo, que mostra a anomalia de
   Belady no FIFO.
5. **Segmentação**: alocar/liberar segmentos com first-fit, best-fit e
   worst-fit; ver lacunas e fragmentação externa (total livre, maior lacuna).
6. **Desafios**: o produto gera um exercício ("com 3 frames e LRU, quantos
   page faults?" / "qual página sai no passo 9?"), o estudante responde, e o
   produto confere e mostra a resolução passo a passo.
7. **CLI** com o mesmo motor: rodar, comparar, gerar curvas e traces no
   terminal, com saída legível e `--json`.
8. **Produto da linha**: `arch.json`, identidade visual da linha (tokens de
   `brand/arch-line.css` do System-Arch), health check, aparece no Arch Hub,
   README/docs de produto, CI, releases, deploy pelo arch-deploy.

## Não-objetivos

- Público professor: modo apresentação, autoria de listas de exercícios,
  turmas. (Próximo passo; o design não pode impedir.)
- Analisar programas reais (traces de valgrind/pin), simulação de cache de
  CPU, escalonamento de processos.
- Contas, backend, banco de dados. Tudo roda no navegador.
- Múltiplos processos com espaços de endereçamento próprios (fica para
  depois; o modelo de dados deve permitir um `pid` por acesso no futuro).

## Anti-objetivos (falharia mesmo "funcionando")

- Mostrar só números finais sem o **porquê** de cada passo.
- O CLI e a web darem resultados diferentes para o mesmo cenário.
- Uma interface pesada em framework que esconda o C — o C é parte do que o
  estudante pode ler e estudar.
- Resultados incorretos nos exemplos clássicos de livro (é material de estudo:
  errar um fault é pior que não ter a feature).

## Restrições

- Núcleo em **C11**, sem dependências, determinístico (mesma entrada → mesma
  saída em qualquer plataforma; PRNG próprio, sem `rand()`).
- Compila nativo (gcc/clang/MinGW) **e** para WebAssembly (Emscripten).
- Web estática (sem servidor), publicada na Vercel pelo **arch-deploy**.
- Sem `ARCHHUB_URL` nada muda; o Hub nunca é requisito.
- Textos da interface em português (é o idioma da linha).

## Abordagens consideradas

| | Como | Troca |
|---|---|---|
| **A. C → WebAssembly (escolhida)** | Núcleo C compilado para nativo (CLI) e WASM (web). Site estático. | Um motor só, o estudante lê o algoritmo em C, deploy estático. Custo: Emscripten no build/CI e uma ponte JSON entre C e UI. |
| B. Reescrever em TypeScript | Motor e UI em TS; CLI via Node. | Mais rápido de iterar a UI; perde o C, que é a identidade de um simulador de SO. |
| C. C nativo + servidor HTTP | UI chama uma API C a cada passo. | Precisa de processo sempre no ar (sem Vercel), mais operação, nenhum ganho para o estudante. |

Escolhida pelo usuário: **A**.

## Design

### Arquitetura

```
core/            biblioteca C pura (sem printf, sem globais, sem malloc oculto
                 fora de um arena/contexto): simulação, algoritmos, geradores,
                 segmentação, serialização JSON
cli/             archmem (nativo): parse de argumentos, saída texto/JSON
web/             site estático: TypeScript + Vite, chama o WASM
  wasm/          glue gerado pelo Emscripten (artefato de build)
tests/           testes C do core (referências de livro) + paridade CLI×WASM
arch.json        manifesto da linha
```

**Fronteira única**: `char *archmem_run(const char *request_json)` devolve
`response_json` (e `archmem_free`). A CLI e a web montam o mesmo JSON de
requisição. Isso garante paridade e deixa a ponte WASM trivial.

### Modelo de simulação (paginação)

- **Config**: `page_size` (potência de 2), `frames`, `virtual_pages` (ou bits
  de endereço virtual), `tlb_entries` (0 = sem TLB), `algorithm`.
- **Acesso**: `{op: R|W, address | page}` — aceitar endereço (para mostrar a
  decomposição) ou número de página direto (exercícios de livro usam páginas).
- **Saída**: um **log de eventos por passo**, completo o bastante para a UI
  reconstruir qualquer passo sem recalcular:
  `step, op, address, page, offset, tlb: hit|miss|off, result: hit|fault,
  frame, victim {page, frame, reason, dirty_writeback}, frames_after[]
  (página, bits de referência/uso, contadores relevantes ao algoritmo),
  explanation_key + parâmetros` (texto montado na UI a partir da chave, para
  traduzir/ajustar sem mexer no C).
- **Totais**: faults, hits, hit rate, substituições, write-backs, hits/misses
  de TLB.
- **Algoritmos**: FIFO, LRU, Clock (segunda chance), LFU, OPT. Convenções
  (seguem o Silberschatz; documentadas e exibidas na UI, porque mudam
  respostas de exercícios):
  - **Clock**: página carregada num fault entra com bit de referência 1; o
    ponteiro começa no frame 0; procurando vítima, bit 1 vira 0 e o ponteiro
    avança; ao substituir, o ponteiro para no frame seguinte ao substituído;
    um hit só liga o bit, sem mover o ponteiro. Frames livres são usados em
    ordem antes de qualquer substituição.
  - **LFU**: contador só enquanto a página está residente (zera ao sair);
    página carregada começa em 1; empate → a que está há mais tempo na
    memória (FIFO entre empatadas).
  - **OPT**: vítima é a página cujo próximo uso está mais longe (ou nunca);
    empate → menor número de página.
  - **LRU/FIFO**: padrão; frames livres em ordem crescente.
- **Oráculo para os algoritmos sem gabarito de livro** (Clock, LFU): uma
  implementação de referência separada, ingênua e lenta, escrita direto das
  regras acima, comparada com o motor em milhares de sequências aleatórias
  (teste de propriedade), além de casos calculados à mão nos testes.
- **TLB**: totalmente associativa, substituição LRU, invalidada quando a
  página sai da memória.
- **Curva**: mesma sequência para `frames = 1..N` por algoritmo.

### Segmentação

Sequência de operações `alloc(nome, tamanho)` / `free(nome)` sobre uma memória
de tamanho fixo, com first/best/worst-fit. Eventos por passo: lacuna
escolhida e por quê, mapa da memória, total livre, maior lacuna, falha por
fragmentação externa. Independente da paginação (módulo e aba próprios).

### Geradores de sequência

Aleatório uniforme, localidade (working set que desliza), laço, sequencial;
todos com semente; tamanho e nº de páginas configuráveis. Exemplos clássicos
embutidos (ex.: a sequência de 20 referências do Silberschatz; a sequência de
Belady `1 2 3 4 1 2 5 1 2 3 4 5`).

### Desafios

Gerados a partir de semente + dificuldade: tipo "quantos faults", "qual
página sai no passo k", "qual algoritmo teve menos faults". A resposta vem do
motor; a UI mostra a resolução usando o mesmo passo a passo. Progresso salvo
em `localStorage`.

### Web

- Abas: **Paginação** (cenário + passo a passo), **Comparar**, **Curva**,
  **Segmentação**, **Desafios**.
- Controles do passo a passo: início, voltar, avançar, fim, play com
  velocidade, teclado (← →), e um slider da linha do tempo.
- Cenário inteiro codificado no **hash da URL** (compartilhável e base do
  futuro modo professor).
- Identidade da linha: tokens `arch-line.css` e marca do System-Arch, tema
  claro/escuro.
- `/health` (JSON estático `{status: "ok", version}`) para o Hub.
- Acessível: navegável por teclado, cores com texto/ícone junto (fault não é
  só "vermelho").

### CLI `archmem`

```
archmem run      --algo lru --frames 3 --refs "7 0 1 2 0 3 ..." [--steps] [--json]
archmem compare  --frames 3 --refs ... [--algos fifo,lru,opt]
archmem curve    --refs ... --max-frames 8
archmem gen      --kind locality --length 50 --pages 10 --seed 42
archmem seg      --fit best --ops "alloc A 300; alloc B 600; free A; alloc C 120"
archmem run      --scenario cenario.json
```

`--help` e `--version` continuam.

### Linha Arch

- `arch.json`: `id: "archmem"`, `role: "insight"`, `runtime: "static"` +
  CLI, `health: "/health"`, `connectsTo: []`.
- **Sem adaptador de registro**: como o `arch-agent`, é estático e não tem
  processo para registrar/heartbeat; o Hub o conhece pelo catálogo e o
  mantém UP pelo health check ativo na URL pública.
- **URL pública**: projeto Vercel `archmem` → `https://archmem.vercel.app`.
  `/health` é um `health.json` estático (`{status, version}`) servido por
  rewrite no `vercel.json`.
- **Deploy**: `arch-deploy` v2.0.0 (já publicado), target `production`
  com `provider: vercel`, `before` buildando WASM + web.
- **Catálogo do Hub**: um PR pequeno no System-Arch adicionando `archmem`
  em `api/_lib/arch-line.json` — último passo do plano, depois do site no ar.

### Qualidade

- Testes do core contra resultados de livro (ex.: Silberschatz, 3 frames:
  FIFO 15, LRU 12, OPT 9; Belady FIFO 3 frames = 9, 4 frames = 10), regras de
  empate, TLB, segmentação, geradores determinísticos.
- Teste de paridade: mesmos cenários pela CLI nativa e pelo WASM (Node) → JSON
  idêntico.
- CI: build nativo Linux/macOS/Windows, testes, sanitizers (ASan/UBSan),
  build WASM + web, e release com binários da CLI.
- Deploy da web: arch-deploy (`vercel`), em tag/merge na main.

### Repositório

Criar `main`, torná-la padrão e limpar `feat/v1.1.0-release` (como no
arch-deploy). Versão do produto: **2.0.0**.

## Decisões que tomei (confira)

- **Nome `ArchMEM` / id `archmem`** — segue ArchMAP/ArchSegurity. O repositório
  pode ser renomeado para `ArchMEM` (o GitHub redireciona o nome antigo).
- **Papel `insight`** — nenhum papel atual é "educação"; `insight` é o mais
  próximo. Criar um papel novo exigiria mudar o contrato e o schema da linha.
- **Web em TypeScript + Vite, sem framework de UI** — o estado do passo a
  passo é um índice sobre um log de eventos já calculado; componentes simples
  bastam, e mantém o foco no C (mesmo espírito do dashboard do Hub).
- **Texto explicativo montado na UI** a partir de chaves emitidas pelo C.
- Convenções dos algoritmos como descritas acima (Silberschatz).
- Sem adaptador de Hub (precedente do arch-agent); catálogo + health check.
- O roteiro fixo de `memory_sim.c` sai; vira exemplos de cenário.

## Deixado para quem construir

Layout exato das telas e visual dos frames/tabelas; formato interno do arena
de memória no C; biblioteca de JSON (escrever um parser mínimo ou embutir um
de domínio público pequeno); ordem de implementação; quantidade e conteúdo
dos exemplos e desafios além dos citados.
