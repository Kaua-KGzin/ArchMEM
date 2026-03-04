# Simuler Memorio

Simulador educacional de gerenciamento de memoria em C (paginacao + segmentacao), com foco em fundamentos de Sistemas Operacionais.

Versao atual: `1.1.0`

## Visao Geral
Este projeto simula comportamentos importantes de um gerenciador de memoria:
- paginacao sob demanda com metadados de tabela de paginas;
- substituicao de paginas por LRU quando a memoria fisica enche;
- segmentacao com alocacao first-fit no espaco de enderecamento virtual;
- metricas de execucao (faults, hits, substituicoes, dirty writes, leituras e escritas).

Ideal para estudo, demonstracoes e discussoes tecnicas de SO.

## Novidades da 1.1.0
- separacao entre paginas virtuais e frames fisicos;
- implementacao real do caminho de substituicao LRU;
- operacoes de leitura/escrita por byte;
- estatisticas de execucao com hit rate;
- alocacao de segmentos por first-fit em lacunas contiguas;
- flags de CLI (`--help`, `--version`);
- Makefile melhorado para build release/debug/sanitize.

## Build
Requisitos:
- `gcc` ou `clang`
- `make`

Compilar binario release:
```bash
make
```

Compilar e executar:
```bash
make run
```

Compilar com debug:
```bash
make debug
```

Compilar com sanitizers (ASan + UBSan):
```bash
make sanitize
```

Limpar artefatos:
```bash
make clean
```

Build rapido no Windows (sem `make`):
```powershell
gcc memory_sim.c -std=c11 -Wall -Wextra -Wpedantic -O2 -o memory_sim.exe
.\memory_sim.exe
```

## Uso
Executar simulacao padrao:
```bash
./memory_sim
```

Mostrar ajuda:
```bash
./memory_sim --help
```

Mostrar versao:
```bash
./memory_sim --version
```

## Fluxo Git (copiar este repositorio)
Clonar o repositorio:
```bash
git clone https://github.com/Kaua-KGzin/Simuler-Mem.git
cd Simuler-Mem
```

Atualizar sua copia local:
```bash
git pull origin main
```

Fluxo basico para enviar alteracoes:
```bash
git checkout -b feat/minha-melhoria
git add .
git commit -m "feat: minha melhoria"
git push -u origin feat/minha-melhoria
```

## Fluxo da Simulacao
1. Inicializa memoria fisica, tabela de paginas e tabela de segmentos.
2. Escreve em mais paginas virtuais do que frames fisicos disponiveis.
3. Dispara substituicao LRU automaticamente.
4. Executa acessos em paginas quentes para atualizar recencia.
5. Aloca/libera segmentos e reutiliza lacunas com first-fit.
6. Exibe tabelas finais e metricas da execucao.

## Estrutura do Projeto
- `memory_sim.c`: implementacao do simulador
- `Makefile`: automacao de build/run/debug/sanitize
- `CHANGELOG.md`: historico de versoes
- `LICENSE`: licenca do projeto

## Roadmap (ideias para v1.2.0)
- politicas FIFO e CLOCK
- entrada de simulacao via arquivo de configuracao
- camada de simulacao de TLB
- tabelas de paginas por processo
- harness de testes para cenarios do alocador

## Licenca
Projeto sob licenca MIT. Veja [LICENSE](LICENSE).
