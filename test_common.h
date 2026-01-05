#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <threads.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "ASSERT FAIL: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        exit(1); \
    } \
} while(0)

static inline void sleep_ms(long ms) {
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    thrd_sleep(&ts, NULL);
}

/* ---------------- Barrier (C11 threads has no built-in barrier) ---------------- */

typedef struct {
    mtx_t mtx;
    cnd_t cv;
    int target;
    int count;
    int generation;
} barrier_t;

static inline void barrier_init(barrier_t *b, int target) {
    mtx_init(&b->mtx, mtx_plain);
    cnd_init(&b->cv);
    b->target = target;
    b->count = 0;
    b->generation = 0;
}

static inline void barrier_destroy(barrier_t *b) {
    cnd_destroy(&b->cv);
    mtx_destroy(&b->mtx);
}

static inline void barrier_wait(barrier_t *b) {
    mtx_lock(&b->mtx);
    int gen = b->generation;

    b->count++;
    if (b->count == b->target) {
        b->generation++;
        b->count = 0;
        cnd_broadcast(&b->cv);
        mtx_unlock(&b->mtx);
        return;
    }

    while (gen == b->generation) {
        cnd_wait(&b->cv, &b->mtx);
    }
    mtx_unlock(&b->mtx);
}

/* ---------------- Random (deterministic by seed) ---------------- */

static inline unsigned rng_next(unsigned *state) {
    /* simple LCG */
    *state = (*state * 1103515245u + 12345u);
    return *state;
}

static inline int rng_range(unsigned *state, int lo, int hi) {
    unsigned r = rng_next(state);
    int span = hi - lo + 1;
    return lo + (int)(r % (unsigned)span);
}

#endif
