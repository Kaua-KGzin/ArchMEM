#include "paging.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *ALG_NAMES[ALG_COUNT] = {"fifo", "lru", "clock", "lfu", "opt"};

int alg_parse(const char *name, Algorithm *out) {
    if (!name) return 1;
    for (int i = 0; i < ALG_COUNT; i++) {
        const char *a = ALG_NAMES[i], *b = name;
        while (*a && tolower((unsigned char)*b) == *a) a++, b++;
        if (!*a && !*b) {
            *out = (Algorithm)i;
            return 0;
        }
    }
    return 1;
}

const char *alg_name(Algorithm a) { return (a >= 0 && a < ALG_COUNT) ? ALG_NAMES[a] : "?"; }

int paging_config_check(const PagingConfig *c, char *err, size_t errlen) {
    if (c->page_size < 16 || c->page_size > 65536 || (c->page_size & (c->page_size - 1)))
        return snprintf(err, errlen, "page_size deve ser potencia de 2 entre 16 e 65536"), 1;
    if (c->frames < 1 || c->frames > PAGING_MAX_FRAMES)
        return snprintf(err, errlen, "frames deve estar entre 1 e %d", PAGING_MAX_FRAMES), 1;
    if (c->virtual_pages < 1 || c->virtual_pages > PAGING_MAX_PAGES)
        return snprintf(err, errlen, "virtual_pages deve estar entre 1 e %d", PAGING_MAX_PAGES), 1;
    if (c->tlb_entries < 0 || c->tlb_entries > PAGING_MAX_TLB)
        return snprintf(err, errlen, "tlb_entries deve estar entre 0 e %d", PAGING_MAX_TLB), 1;
    if (c->algorithm < 0 || c->algorithm >= ALG_COUNT) return snprintf(err, errlen, "algoritmo desconhecido"), 1;
    return 0;
}

typedef struct {
    int page; /* -1 = free */
    int ref;
    int count;
    int loaded;
    int last;
    int dirty;
} Frame;

typedef struct {
    int page;
    int last;
} TlbEntry;

typedef struct {
    const PagingConfig *cfg;
    const int *pages; /* resolved page per step, for OPT look-ahead */
    size_t n;
    Frame frames[PAGING_MAX_FRAMES];
    int *where; /* page -> frame or -1 */
    TlbEntry tlb[PAGING_MAX_TLB];
    int tlb_n;
    int hand;
} Sim;

/* Victim choice. Returns the frame index; fills the explanation fields. */
typedef struct {
    int frame;
    int next_use; /* OPT only, -1 = never */
    int cleared[PAGING_MAX_FRAMES];
    int n_cleared;
} Choice;

static int next_use(const Sim *s, int page, size_t after) {
    for (size_t i = after; i < s->n; i++)
        if (s->pages[i] == page) return (int)i + 1;
    return -1;
}

static int choose_victim(Sim *s, size_t idx, Choice *c) {
    int F = s->cfg->frames;
    c->next_use = -1;
    c->n_cleared = 0;
    int best = 0;
    switch (s->cfg->algorithm) {
    case ALG_FIFO:
        for (int f = 1; f < F; f++)
            if (s->frames[f].loaded < s->frames[best].loaded) best = f;
        break;
    case ALG_LRU:
        for (int f = 1; f < F; f++)
            if (s->frames[f].last < s->frames[best].last) best = f;
        break;
    case ALG_OPT: {
        int best_next = -2;
        for (int f = 0; f < F; f++) {
            int nu = next_use(s, s->frames[f].page, idx + 1);
            int dist = nu < 0 ? 1 << 30 : nu;
            int best_dist = best_next == -2 ? -1 : (best_next < 0 ? 1 << 30 : best_next);
            if (dist > best_dist || (dist == best_dist && s->frames[f].page < s->frames[best].page)) {
                best = f;
                best_next = nu;
            }
        }
        c->next_use = best_next;
        break;
    }
    case ALG_LFU:
        for (int f = 1; f < F; f++) {
            const Frame *a = &s->frames[f], *b = &s->frames[best];
            if (a->count < b->count || (a->count == b->count && a->loaded < b->loaded)) best = f;
        }
        break;
    case ALG_CLOCK:
        /* Second chance: clear set bits until a frame with ref=0 comes up.
         * At most F bits are set, so at most F are cleared. */
        while (s->frames[s->hand].ref) {
            s->frames[s->hand].ref = 0;
            c->cleared[c->n_cleared++] = s->frames[s->hand].page;
            s->hand = (s->hand + 1) % F;
        }
        best = s->hand;
        s->hand = (s->hand + 1) % F;
        break;
    default:
        return -1;
    }
    c->frame = best;
    return 0;
}

static void tlb_remove(Sim *s, int page) {
    for (int i = 0; i < s->tlb_n; i++) {
        if (s->tlb[i].page == page) {
            memmove(&s->tlb[i], &s->tlb[i + 1], (size_t)(s->tlb_n - i - 1) * sizeof s->tlb[0]);
            s->tlb_n--;
            return;
        }
    }
}

/* Returns 1 on a TLB hit (refreshing its LRU stamp). */
static int tlb_lookup(Sim *s, int page, int step) {
    for (int i = 0; i < s->tlb_n; i++) {
        if (s->tlb[i].page == page) {
            s->tlb[i].last = step;
            return 1;
        }
    }
    return 0;
}

/* Caches a translation after a miss, evicting the LRU entry if full. Called
 * after the fault is handled, so a victim's entry is already gone. */
static void tlb_insert(Sim *s, int page, int step) {
    if (s->tlb_n < s->cfg->tlb_entries) {
        s->tlb[s->tlb_n++] = (TlbEntry){page, step};
    } else {
        int lru = 0;
        for (int i = 1; i < s->tlb_n; i++)
            if (s->tlb[i].last < s->tlb[lru].last) lru = i;
        s->tlb[lru] = (TlbEntry){page, step};
    }
}

static void emit_frames(const Sim *s, Buf *b) {
    buf_putc(b, '[');
    for (int f = 0; f < s->cfg->frames; f++) {
        const Frame *fr = &s->frames[f];
        if (f) buf_putc(b, ',');
        if (fr->page < 0) {
            buf_puts(b, "null");
            continue;
        }
        buf_printf(b, "{\"page\":%d,\"ref\":%d,\"count\":%d,\"loaded\":%d,\"last\":%d,\"dirty\":%s}", fr->page,
                   fr->ref, fr->count, fr->loaded, fr->last, fr->dirty ? "true" : "false");
    }
    buf_putc(b, ']');
}

int paging_run(const PagingConfig *cfg, const Ref *refs, size_t n, Buf *out, Totals *t, char *err,
               size_t errlen) {
    memset(t, 0, sizeof *t);
    if (paging_config_check(cfg, err, errlen)) return 1;
    if (n == 0) return snprintf(err, errlen, "sequencia de acessos vazia"), 1;
    if (n > REFS_MAX) return snprintf(err, errlen, "sequencia longa demais (limite %d acessos)", REFS_MAX), 1;

    long space = (long)cfg->page_size * cfg->virtual_pages;
    int *pages = malloc(n * sizeof *pages);
    int *offsets = malloc(n * sizeof *offsets);
    int *where = malloc((size_t)cfg->virtual_pages * sizeof *where);
    if (!pages || !offsets || !where) {
        free(pages), free(offsets), free(where);
        return snprintf(err, errlen, "sem memoria"), 1;
    }
    for (size_t i = 0; i < n; i++) {
        if (refs[i].is_address) {
            if (refs[i].value >= space) {
                snprintf(err, errlen, "acesso %zu: endereco %ld fora do espaco virtual (0..%ld)", i + 1, refs[i].value,
                         space - 1);
                free(pages), free(offsets), free(where);
                return 1;
            }
            pages[i] = (int)(refs[i].value / cfg->page_size);
            offsets[i] = (int)(refs[i].value % cfg->page_size);
        } else {
            if (refs[i].value >= cfg->virtual_pages) {
                snprintf(err, errlen, "acesso %zu: pagina %ld fora do espaco virtual (0..%d)", i + 1, refs[i].value,
                         cfg->virtual_pages - 1);
                free(pages), free(offsets), free(where);
                return 1;
            }
            pages[i] = (int)refs[i].value;
            offsets[i] = 0;
        }
    }

    Sim s;
    memset(&s, 0, sizeof s);
    s.cfg = cfg;
    s.pages = pages;
    s.n = n;
    s.where = where;
    for (int f = 0; f < PAGING_MAX_FRAMES; f++) s.frames[f].page = -1;
    for (int p = 0; p < cfg->virtual_pages; p++) where[p] = -1;

    if (out) buf_putc(out, '[');
    int rc = 0;
    for (size_t i = 0; i < n; i++) {
        int k = (int)i + 1;
        int page = pages[i];
        int write = refs[i].op == 'W';
        t->accesses++;

        const char *tlb = "off";
        int tlb_hit = 0;
        if (cfg->tlb_entries > 0) {
            tlb_hit = tlb_lookup(&s, page, k);
            tlb = tlb_hit ? "hit" : "miss";
            if (tlb_hit) t->tlb_hits++;
            else t->tlb_misses++;
        }

        int frame = where[page];
        const char *why;
        int victim_page = -1, victim_dirty = 0;
        Frame victim_before = {0};
        Choice choice = {0};
        if (frame >= 0) {
            Frame *fr = &s.frames[frame];
            fr->last = k;
            fr->count++;
            fr->ref = 1;
            fr->dirty |= write;
            t->hits++;
            why = tlb_hit ? "hit_tlb" : "hit";
        } else {
            t->faults++;
            frame = -1;
            for (int f = 0; f < cfg->frames; f++)
                if (s.frames[f].page < 0) {
                    frame = f;
                    break;
                }
            if (frame >= 0) {
                why = "fault_free_frame";
            } else {
                if (choose_victim(&s, i, &choice)) {
                    snprintf(err, errlen, "algoritmo %s ainda nao implementado", alg_name(cfg->algorithm));
                    rc = 1;
                    break;
                }
                frame = choice.frame;
                victim_before = s.frames[frame];
                victim_page = victim_before.page;
                victim_dirty = victim_before.dirty;
                if (victim_dirty) t->writebacks++;
                t->replacements++;
                where[victim_page] = -1;
                tlb_remove(&s, victim_page);
                static const char *WHY[ALG_COUNT] = {"fault_evict_fifo", "fault_evict_lru", "fault_evict_clock",
                                                     "fault_evict_lfu", "fault_evict_opt"};
                why = WHY[cfg->algorithm];
            }
            s.frames[frame] = (Frame){page, 1, 1, k, k, write};
            where[page] = frame;
        }
        if (cfg->tlb_entries > 0 && !tlb_hit) tlb_insert(&s, page, k);

        if (!out) continue;
        if (i) buf_putc(out, ',');
        buf_printf(out,
                   "{\"step\":%d,\"op\":\"%c\",\"address\":%ld,\"page\":%d,\"offset\":%d,\"tlb\":\"%s\","
                   "\"result\":\"%s\",\"frame\":%d,\"victim\":",
                   k, write ? 'W' : 'R', (long)page * cfg->page_size + offsets[i], page, offsets[i], tlb,
                   victim_page >= 0 || strcmp(why, "fault_free_frame") == 0 ? "fault" : "hit", frame);
        if (victim_page >= 0) {
            buf_printf(out, "{\"page\":%d,\"frame\":%d,\"dirty\":%s}", victim_page, frame,
                       victim_dirty ? "true" : "false");
        } else {
            buf_puts(out, "null");
        }
        buf_printf(out, ",\"why\":\"%s\",\"why_args\":{", why);
        if (victim_page >= 0) {
            buf_printf(out, "\"victim\":%d,\"loaded\":%d,\"last\":%d,\"count\":%d,\"next_use\":%d,\"cleared\":[",
                       victim_page, victim_before.loaded, victim_before.last, victim_before.count, choice.next_use);
            for (int c = 0; c < choice.n_cleared; c++) buf_printf(out, c ? ",%d" : "%d", choice.cleared[c]);
            buf_putc(out, ']');
        }
        buf_puts(out, "},\"frames\":");
        emit_frames(&s, out);
        buf_printf(out, ",\"hand\":%d,\"tlb_pages\":[", s.hand);
        for (int e = 0; e < s.tlb_n; e++) buf_printf(out, e ? ",%d" : "%d", s.tlb[e].page);
        buf_puts(out, "]}");
    }
    if (out && !rc) buf_putc(out, ']');
    free(pages), free(offsets), free(where);
    return rc;
}
