/* archmem: command line for the ArchMEM engine. Every command builds a JSON
 * request, hands it to archmem_run (the same entry point the web app calls
 * through WebAssembly) and prints the response for people, or verbatim with
 * --json. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../core/archmem.h"
#include "../core/buf.h"
#include "../core/json.h"

#define EXIT_ENGINE 1
#define EXIT_USAGE 2

typedef struct {
    const char *name;
    const char *value;
} Flag;

typedef struct {
    Flag flags[32];
    int n;
    int json, steps, answer;
} Args;

static const char *USAGE =
    "ArchMEM " ARCHMEM_VERSION " - laboratorio de memoria virtual\n"
    "\n"
    "uso:\n"
    "  archmem run      --algo lru --frames 3 --refs \"7 0 1 2 0 3\" [--steps] [--json]\n"
    "  archmem run      --scenario cenario.json [--json]\n"
    "  archmem compare  --frames 3 --refs \"...\" [--algos fifo,lru,clock,lfu,opt]\n"
    "  archmem curve    --refs \"...\" [--max-frames 8] [--algos ...]\n"
    "  archmem gen      --kind uniform|locality|loop|sequential --length 50 --pages 10 [--seed 42] [--write-ratio 0.2]\n"
    "  archmem seg      --fit first|best|worst --memory 1024 --ops \"alloc A 300; alloc B 200; free A\"\n"
    "  archmem challenge [--seed 7] [--difficulty 1|2|3] [--answer]\n"
    "\n"
    "opcoes de paginacao: --page-size 64  --pages 16 (paginas virtuais)  --tlb 0 (entradas)\n"
    "acessos: N le a pagina N, Nw escreve, @130 le o endereco 130, @0x82w escreve\n"
    "--json imprime a resposta do motor sem formatar\n";

static const char *flag(const Args *a, const char *name, const char *def) {
    for (int i = 0; i < a->n; i++)
        if (strcmp(a->flags[i].name, name) == 0) return a->flags[i].value;
    return def;
}

static int parse_args(int argc, char **argv, int start, Args *a) {
    memset(a, 0, sizeof *a);
    for (int i = start; i < argc; i++) {
        const char *f = argv[i];
        if (strcmp(f, "--json") == 0) a->json = 1;
        else if (strcmp(f, "--steps") == 0) a->steps = 1;
        else if (strcmp(f, "--answer") == 0) a->answer = 1;
        else if (strncmp(f, "--", 2) == 0) {
            if (i + 1 >= argc || strncmp(argv[i + 1], "--", 2) == 0) {
                fprintf(stderr, "erro: %s precisa de um valor\n", f);
                return 1;
            }
            if (a->n == 32) return 1;
            a->flags[a->n++] = (Flag){f + 2, argv[++i]};
        } else {
            fprintf(stderr, "erro: argumento inesperado \"%s\"\n", f);
            return 1;
        }
    }
    return 0;
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    Buf b;
    buf_init(&b);
    char chunk[4096];
    size_t n;
    while ((n = fread(chunk, 1, sizeof chunk, f)) > 0) buf_append(&b, chunk, n);
    fclose(f);
    return buf_take(&b);
}

/* Writes "key":value for a numeric flag (validated by the engine). */
static void num_field(Buf *b, const char *key, const char *value, int *first) {
    if (!value) return;
    char *end;
    double d = strtod(value, &end);
    buf_printf(b, "%s\"%s\":", *first ? "" : ",", key);
    if (*value && !*end) buf_json_num(b, d);
    else buf_json_str(b, value); /* lets the engine report the bad value */
    *first = 0;
}

static void str_field(Buf *b, const char *key, const char *value, int *first) {
    if (!value) return;
    buf_printf(b, "%s\"%s\":", *first ? "" : ",", key);
    buf_json_str(b, value);
    *first = 0;
}

static void config_object(Buf *b, const Args *a) {
    int first = 1;
    buf_puts(b, "\"config\":{");
    num_field(b, "page_size", flag(a, "page-size", NULL), &first);
    num_field(b, "frames", flag(a, "frames", NULL), &first);
    num_field(b, "virtual_pages", flag(a, "pages", NULL), &first);
    num_field(b, "tlb_entries", flag(a, "tlb", NULL), &first);
    str_field(b, "algorithm", flag(a, "algo", NULL), &first);
    buf_putc(b, '}');
}

static void algorithms_array(Buf *b, const Args *a) {
    const char *list = flag(a, "algos", NULL);
    if (!list) return;
    buf_puts(b, ",\"algorithms\":[");
    const char *p = list;
    int first = 1;
    while (*p) {
        const char *start = p;
        while (*p && *p != ',') p++;
        char name[24];
        size_t len = (size_t)(p - start) < sizeof name - 1 ? (size_t)(p - start) : sizeof name - 1;
        memcpy(name, start, len);
        name[len] = '\0';
        if (!first) buf_putc(b, ',');
        buf_json_str(b, name);
        first = 0;
        if (*p) p++;
    }
    buf_putc(b, ']');
}

/* Runs the request; prints errors; returns the parsed response or NULL. */
static JVal *engine(const char *request, int raw_json, int *exit_code) {
    char *res = archmem_run(request);
    char err[128];
    JVal *v = json_parse(res, err, sizeof err);
    int ok = json_bool(json_get(v, "ok"), 0);
    if (raw_json) printf("%s\n", res);
    else if (!ok) fprintf(stderr, "erro: %s\n", json_str(json_get(v, "error"), "resposta invalida do motor"));
    archmem_free(res);
    *exit_code = ok ? 0 : EXIT_ENGINE;
    if (!ok || raw_json) {
        json_free(v);
        return NULL;
    }
    return v;
}

static int geti(const JVal *o, const char *key) { return (int)json_num(json_get(o, key), 0); }

static const char *why_short(const char *why) {
    static const struct {
        const char *key, *text;
    } map[] = {
        {"hit", "hit"},
        {"hit_tlb", "hit (TLB)"},
        {"fault_free_frame", "frame livre"},
        {"fault_evict_fifo", "FIFO: sai a mais antiga"},
        {"fault_evict_lru", "LRU: sai a menos usada recentemente"},
        {"fault_evict_clock", "Clock: segunda chance"},
        {"fault_evict_lfu", "LFU: sai a menos usada"},
        {"fault_evict_opt", "OPT: sai a usada mais tarde"},
    };
    for (size_t i = 0; i < sizeof map / sizeof map[0]; i++)
        if (strcmp(map[i].key, why) == 0) return map[i].text;
    return why;
}

static void print_totals(const JVal *t) {
    printf("acessos %d | faults %d | hits %d | taxa de acerto %.1f%% | substituicoes %d | write-backs %d", geti(t, "accesses"),
           geti(t, "faults"), geti(t, "hits"), json_num(json_get(t, "hit_rate"), 0) * 100.0, geti(t, "replacements"),
           geti(t, "writebacks"));
    if (geti(t, "tlb_hits") + geti(t, "tlb_misses") > 0)
        printf(" | TLB %d hits / %d misses", geti(t, "tlb_hits"), geti(t, "tlb_misses"));
    printf("\n");
}

static void print_run(const JVal *v, int steps) {
    const JVal *c = json_get(v, "config");
    printf("ArchMEM %s | %s | %d frames | pagina %d B | %d paginas virtuais | TLB %d\n", ARCHMEM_VERSION,
           json_str(json_get(c, "algorithm"), "?"), geti(c, "frames"), geti(c, "page_size"), geti(c, "virtual_pages"),
           geti(c, "tlb_entries"));
    if (steps) {
        printf("\npasso | ref   | pag:desl | TLB  | resultado | frames depois        | motivo\n");
        const JVal *list = json_get(v, "steps");
        for (size_t i = 0; i < json_len(list); i++) {
            const JVal *s = json_at(list, i);
            char ref[24], frames[96] = "[", pd[24];
            snprintf(ref, sizeof ref, "%d%s", geti(s, "page"), strcmp(json_str(json_get(s, "op"), "R"), "W") == 0 ? "w" : "");
            snprintf(pd, sizeof pd, "%d:%d", geti(s, "page"), geti(s, "offset"));
            const JVal *fr = json_get(s, "frames");
            for (size_t f = 0; f < json_len(fr) && strlen(frames) < sizeof frames - 8; f++) {
                char cell[12];
                const JVal *x = json_at(fr, f);
                snprintf(cell, sizeof cell, "%s%s", f ? " " : "", json_type(x) == J_NULL ? "." : "");
                strcat(frames, cell);
                if (json_type(x) != J_NULL) {
                    snprintf(cell, sizeof cell, "%d", geti(x, "page"));
                    strcat(frames, cell);
                }
            }
            strcat(frames, "]");
            const char *why = why_short(json_str(json_get(s, "why"), ""));
            char victim[32] = "";
            if (json_type(json_get(s, "victim")) == J_OBJ)
                snprintf(victim, sizeof victim, " (sai %d%s)", geti(json_get(s, "victim"), "page"),
                         json_bool(json_get(json_get(s, "victim"), "dirty"), 0) ? ", write-back" : "");
            printf("%5d | %-5s | %-8s | %-4s | %-9s | %-20s | %s%s\n", geti(s, "step"), ref, pd,
                   json_str(json_get(s, "tlb"), ""), strcmp(json_str(json_get(s, "result"), ""), "hit") ? "FAULT" : "hit",
                   frames, why, victim);
        }
        printf("\n");
    }
    print_totals(json_get(v, "totals"));
}

static void print_compare(const JVal *v) {
    const JVal *r = json_get(v, "results");
    size_t n = json_len(r);
    size_t order[8];
    for (size_t i = 0; i < n && i < 8; i++) order[i] = i;
    for (size_t i = 0; i < n; i++) /* stable insertion sort by faults */
        for (size_t j = i; j > 0 && geti(json_get(json_at(r, order[j]), "totals"), "faults") <
                                         geti(json_get(json_at(r, order[j - 1]), "totals"), "faults");
             j--) {
            size_t t = order[j];
            order[j] = order[j - 1];
            order[j - 1] = t;
        }
    printf("  # | algoritmo | faults | hits | acerto | write-backs\n");
    for (size_t i = 0; i < n; i++) {
        const JVal *x = json_at(r, order[i]);
        const JVal *t = json_get(x, "totals");
        printf("%3zu | %-9s | %6d | %4d | %5.1f%% | %d\n", i + 1, json_str(json_get(x, "algorithm"), "?"), geti(t, "faults"),
               geti(t, "hits"), json_num(json_get(t, "hit_rate"), 0) * 100.0, geti(t, "writebacks"));
    }
}

static void print_curve(const JVal *v) {
    const JVal *frames = json_get(v, "frames"), *series = json_get(v, "series");
    printf("faults por numero de frames\n\nframes");
    for (size_t s = 0; s < json_len(series); s++) printf(" | %6s", json_str(json_get(json_at(series, s), "algorithm"), "?"));
    printf("\n");
    int max = 1;
    for (size_t s = 0; s < json_len(series); s++)
        for (size_t f = 0; f < json_len(frames); f++) {
            int x = (int)json_num(json_at(json_get(json_at(series, s), "faults"), f), 0);
            if (x > max) max = x;
        }
    for (size_t f = 0; f < json_len(frames); f++) {
        printf("%6d", (int)json_num(json_at(frames, f), 0));
        for (size_t s = 0; s < json_len(series); s++)
            printf(" | %6d", (int)json_num(json_at(json_get(json_at(series, s), "faults"), f), 0));
        if (json_len(series) == 1) {
            int x = (int)json_num(json_at(json_get(json_at(series, 0), "faults"), f), 0);
            printf("  ");
            for (int k = 0; k < x * 30 / max; k++) putchar('#');
        }
        printf("\n");
    }
    const JVal *b = json_get(v, "belady");
    if (json_len(b) == 0) printf("\nnenhuma anomalia de Belady nesta sequencia\n");
    for (size_t i = 0; i < json_len(b); i++)
        printf("\nanomalia de Belady: %s tem MAIS faults com %d frames do que com %d\n",
               json_str(json_get(json_at(b, i), "algorithm"), "?"), geti(json_at(b, i), "at_frames"),
               geti(json_at(b, i), "at_frames") - 1);
}

static void print_seg(const JVal *v) {
    const JVal *steps = json_get(v, "steps");
    printf("segmentacao (%s-fit)\n\n", json_str(json_get(v, "fit"), "?"));
    for (size_t i = 0; i < json_len(steps); i++) {
        const JVal *s = json_at(steps, i);
        const char *op = json_str(json_get(s, "op"), "");
        const char *res = json_str(json_get(s, "result"), "");
        if (strcmp(op, "free") == 0)
            printf("%3d free  %-8s -> libera %d bytes em %d\n", geti(s, "step"), json_str(json_get(s, "name"), ""),
                   geti(s, "size"), geti(s, "base"));
        else if (strcmp(res, "ok") == 0)
            printf("%3d alloc %-8s %6d -> base %d (lacuna de %d bytes)\n", geti(s, "step"), json_str(json_get(s, "name"), ""),
                   geti(s, "size"), geti(s, "base"), geti(json_get(s, "chosen_hole"), "size"));
        else
            printf("%3d alloc %-8s %6d -> FALHOU (%s)\n", geti(s, "step"), json_str(json_get(s, "name"), ""), geti(s, "size"),
                   strcmp(json_str(json_get(s, "why"), ""), "seg_fail_fragmentation") == 0 ? "fragmentacao externa"
                                                                                            : "sem espaco");
    }
    const JVal *t = json_get(v, "totals");
    printf("\nlivre %d de %d bytes | maior lacuna %d | lacunas %d | falhas %d | fragmentacao externa %.0f%%\n", geti(t, "free"),
           geti(t, "memory_size"), geti(t, "largest_hole"), geti(t, "holes"), geti(t, "failures"),
           json_num(json_get(t, "external_fragmentation"), 0) * 100.0);
}

static void print_challenge(const JVal *v, int show_answer) {
    const JVal *c = json_get(v, "challenge"), *p = json_get(c, "params");
    const char *kind = json_str(json_get(c, "kind"), "");
    const char *refs = json_str(json_get(json_get(c, "scenario"), "refs"), "");
    printf("desafio (nivel %d, semente %d)\n\nsequencia: %s\n", geti(c, "difficulty"), geti(c, "seed"), refs);
    if (strcmp(kind, "victim") == 0)
        printf("com %d frames e %s, qual pagina sai da memoria no passo %d?\n", geti(p, "frames"),
               json_str(json_get(p, "algorithm"), "?"), geti(p, "step"));
    else if (strcmp(kind, "best_algorithm") == 0)
        printf("com %d frames, qual destes tem MENOS page faults: fifo, lru, clock ou lfu?\n", geti(p, "frames"));
    else
        printf("com %d frames e %s, quantos page faults acontecem?\n", geti(p, "frames"), json_str(json_get(p, "algorithm"), "?"));
    const JVal *ans = json_get(c, "answer");
    if (show_answer) {
        if (json_type(ans) == J_STR) printf("\nresposta: %s\n", json_str(ans, ""));
        else printf("\nresposta: %d\n", (int)json_num(ans, 0));
    } else {
        printf("\n(rode de novo com --answer para ver a resposta)\n");
    }
}

int main(int argc, char **argv) {
    if (argc < 2 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "help") == 0) {
        fputs(USAGE, argc < 2 ? stderr : stdout);
        return argc < 2 ? EXIT_USAGE : 0;
    }
    if (strcmp(argv[1], "--version") == 0) {
        printf("%s\n", ARCHMEM_VERSION);
        return 0;
    }
    if (strcmp(argv[1], "--raw") == 0) { /* used by the parity tests */
        if (argc != 3) return EXIT_USAGE;
        char *req = read_file(argv[2]);
        if (!req) {
            fprintf(stderr, "erro: nao consegui ler %s\n", argv[2]);
            return EXIT_USAGE;
        }
        char *res = archmem_run(req);
        fputs(res, stdout);
        int ok = strstr(res, "\"ok\":true") == res + 1;
        archmem_free(res);
        free(req);
        return ok ? 0 : EXIT_ENGINE;
    }

    const char *cmd = argv[1];
    Args a;
    if (parse_args(argc, argv, 2, &a)) return EXIT_USAGE;

    Buf req;
    buf_init(&req);
    char *scenario = NULL;
    if (strcmp(cmd, "run") == 0 && flag(&a, "scenario", NULL)) {
        scenario = read_file(flag(&a, "scenario", NULL));
        if (!scenario) {
            fprintf(stderr, "erro: nao consegui ler %s\n", flag(&a, "scenario", NULL));
            return EXIT_USAGE;
        }
        buf_puts(&req, scenario);
        free(scenario);
    } else if (strcmp(cmd, "run") == 0 || strcmp(cmd, "compare") == 0 || strcmp(cmd, "curve") == 0) {
        buf_printf(&req, "{\"action\":\"%s\",", cmd);
        config_object(&req, &a);
        algorithms_array(&req, &a);
        int first = 0;
        num_field(&req, "max_frames", flag(&a, "max-frames", NULL), &first);
        str_field(&req, "refs", flag(&a, "refs", NULL), &first);
        buf_putc(&req, '}');
    } else if (strcmp(cmd, "gen") == 0) {
        int first = 0;
        buf_puts(&req, "{\"action\":\"gen\"");
        str_field(&req, "kind", flag(&a, "kind", NULL), &first);
        num_field(&req, "length", flag(&a, "length", NULL), &first);
        num_field(&req, "pages", flag(&a, "pages", NULL), &first);
        num_field(&req, "seed", flag(&a, "seed", NULL), &first);
        num_field(&req, "write_ratio", flag(&a, "write-ratio", NULL), &first);
        buf_putc(&req, '}');
    } else if (strcmp(cmd, "seg") == 0) {
        int first = 0;
        buf_puts(&req, "{\"action\":\"seg\"");
        str_field(&req, "fit", flag(&a, "fit", NULL), &first);
        num_field(&req, "memory_size", flag(&a, "memory", NULL), &first);
        str_field(&req, "ops", flag(&a, "ops", NULL), &first);
        buf_putc(&req, '}');
    } else if (strcmp(cmd, "challenge") == 0) {
        int first = 0;
        buf_puts(&req, "{\"action\":\"challenge\"");
        num_field(&req, "seed", flag(&a, "seed", NULL), &first);
        num_field(&req, "difficulty", flag(&a, "difficulty", NULL), &first);
        buf_putc(&req, '}');
    } else {
        fprintf(stderr, "erro: comando desconhecido \"%s\"\n\n%s", cmd, USAGE);
        return EXIT_USAGE;
    }

    char *request = buf_take(&req);
    int code;
    JVal *v = engine(request, a.json, &code);
    free(request);
    if (!v) return code;
    if (strcmp(cmd, "run") == 0) print_run(v, a.steps);
    else if (strcmp(cmd, "compare") == 0) print_compare(v);
    else if (strcmp(cmd, "curve") == 0) print_curve(v);
    else if (strcmp(cmd, "gen") == 0) printf("%s\n", json_str(json_get(v, "refs"), ""));
    else if (strcmp(cmd, "seg") == 0) print_seg(v);
    else print_challenge(v, a.answer);
    json_free(v);
    return 0;
}
