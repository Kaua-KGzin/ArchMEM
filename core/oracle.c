#include "oracle.h"

int oracle_faults(Algorithm alg, int frames, const int *pages, size_t n) {
    int mem[PAGING_MAX_FRAMES], used = 0;      /* frame contents */
    int loaded_at[PAGING_MAX_FRAMES];           /* step loaded */
    int used_at[PAGING_MAX_FRAMES];             /* last use */
    int uses[PAGING_MAX_FRAMES];                /* LFU count while resident */
    int refbit[PAGING_MAX_FRAMES];              /* Clock */
    int hand = 0, faults = 0;

    for (size_t t = 0; t < n; t++) {
        int p = pages[t], at = -1;
        for (int f = 0; f < used; f++)
            if (mem[f] == p) at = f;
        if (at >= 0) {
            used_at[at] = (int)t;
            uses[at]++;
            refbit[at] = 1;
            continue;
        }
        faults++;
        int slot;
        if (used < frames) {
            slot = used++;
        } else if (alg == ALG_FIFO) {
            slot = 0;
            for (int f = 0; f < frames; f++)
                if (loaded_at[f] < loaded_at[slot]) slot = f;
        } else if (alg == ALG_LRU) {
            slot = 0;
            for (int f = 0; f < frames; f++)
                if (used_at[f] < used_at[slot]) slot = f;
        } else if (alg == ALG_LFU) {
            slot = 0;
            for (int f = 0; f < frames; f++)
                if (uses[f] < uses[slot] || (uses[f] == uses[slot] && loaded_at[f] < loaded_at[slot])) slot = f;
        } else if (alg == ALG_CLOCK) {
            while (refbit[hand]) {
                refbit[hand] = 0;
                hand = (hand + 1) % frames;
            }
            slot = hand;
            hand = (hand + 1) % frames;
        } else { /* OPT */
            slot = -1;
            size_t far = 0;
            for (int f = 0; f < frames; f++) {
                size_t next = n + 1; /* never used again */
                for (size_t u = t + 1; u < n; u++)
                    if (pages[u] == mem[f]) {
                        next = u;
                        break;
                    }
                if (slot < 0 || next > far || (next == far && mem[f] < mem[slot])) {
                    slot = f;
                    far = next;
                }
            }
        }
        mem[slot] = p;
        loaded_at[slot] = (int)t;
        used_at[slot] = (int)t;
        uses[slot] = 1;
        refbit[slot] = 1;
    }
    return faults;
}
