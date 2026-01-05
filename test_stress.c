#include "queue.h"
#include "test_common.h"
#include <stdint.h>
#include <string.h>
#include <limits.h>

typedef struct {
    int producer_id;
    int items_to_produce;
    barrier_t *start_barrier;
    atomic_int *global_produced;
    unsigned rng_state;
} producer_arg_t;

typedef struct {
    int consumer_id;
    int total_items;
    barrier_t *start_barrier;
    atomic_int *global_consumed;
    atomic_uchar *seen_bitmap; /* length = total_items */
    unsigned rng_state;
} consumer_arg_t;

/* Sentinel that can never be a valid item index [0 .. total_items-1] */
#define SENTINEL_VALUE UINT32_MAX

static inline uint32_t *make_item(uint32_t value) {
    uint32_t *p = (uint32_t*)malloc(sizeof(uint32_t));
    *p = value;
    return p;
}

int producer_thread(void *arg) {
    producer_arg_t *producer_args = (producer_arg_t*)arg;
    barrier_wait(producer_args->start_barrier);

    for (int i = 0; i < producer_args->items_to_produce; i++) {
        int unique_index = atomic_fetch_add(producer_args->global_produced, 1);
        uint32_t *heap_item = make_item((uint32_t)unique_index);
        enqueue(heap_item);

        int delay_ms = rng_range(&producer_args->rng_state, 0, 3);
        if (delay_ms) sleep_ms(delay_ms);
    }
    return 0;
}

int consumer_thread(void *arg) {
    consumer_arg_t *consumer_args = (consumer_arg_t*)arg;
    barrier_wait(consumer_args->start_barrier);

    while (1) {
        void *ptr = dequeue();
        uint32_t value = *(uint32_t*)ptr;
        free(ptr);

        /* Sentinel = exit (used to wake blocked consumers at shutdown) */
        if (value == SENTINEL_VALUE) {
            break;
        }

        ASSERT_TRUE(value < (uint32_t)consumer_args->total_items);

        /* mark seen; fail on duplicates */
        unsigned char expected0 = 0;
        if (!atomic_compare_exchange_strong(&consumer_args->seen_bitmap[value], &expected0, 1)) {
            fprintf(stderr, "Duplicate item detected: %u\n", value);
            exit(1);
        }

        atomic_fetch_add(consumer_args->global_consumed, 1);

        int delay_ms = rng_range(&consumer_args->rng_state, 0, 3);
        if (delay_ms) sleep_ms(delay_ms);
    }
    return 0;
}

int main(int argc, char **argv) {
    unsigned seed = 12345u;
    if (argc >= 2) seed = (unsigned)strtoul(argv[1], NULL, 10);

    initQueue();

    const int PRODUCERS = 6;
    const int CONSUMERS = 6;
    const int ITEMS_PER_PRODUCER = 5000;
    const int TOTAL_ITEMS = PRODUCERS * ITEMS_PER_PRODUCER;

    atomic_int global_produced;
    atomic_int global_consumed;
    atomic_init(&global_produced, 0);
    atomic_init(&global_consumed, 0);

    atomic_uchar *seen = (atomic_uchar*)malloc((size_t)TOTAL_ITEMS * sizeof(atomic_uchar));
    for (int i = 0; i < TOTAL_ITEMS; i++) atomic_init(&seen[i], 0);

    barrier_t start_barrier;
    barrier_init(&start_barrier, PRODUCERS + CONSUMERS);

    thrd_t producer_threads[PRODUCERS];
    thrd_t consumer_threads[CONSUMERS];
    producer_arg_t producer_args[PRODUCERS];
    consumer_arg_t consumer_args[CONSUMERS];

    unsigned s = seed;

    for (int i = 0; i < PRODUCERS; i++) {
        producer_args[i].producer_id = i;
        producer_args[i].items_to_produce = ITEMS_PER_PRODUCER;
        producer_args[i].start_barrier = &start_barrier;
        producer_args[i].global_produced = &global_produced;
        producer_args[i].rng_state = rng_next(&s);

        ASSERT_TRUE(thrd_create(&producer_threads[i], producer_thread, &producer_args[i]) == thrd_success);
    }

    for (int i = 0; i < CONSUMERS; i++) {
        consumer_args[i].consumer_id = i;
        consumer_args[i].total_items = TOTAL_ITEMS;
        consumer_args[i].start_barrier = &start_barrier;
        consumer_args[i].global_consumed = &global_consumed;
        consumer_args[i].seen_bitmap = seen;
        consumer_args[i].rng_state = rng_next(&s);

        ASSERT_TRUE(thrd_create(&consumer_threads[i], consumer_thread, &consumer_args[i]) == thrd_success);
    }

    /* Wait for producers to finish creating all real items */
    for (int i = 0; i < PRODUCERS; i++) thrd_join(producer_threads[i], NULL);
    ASSERT_TRUE(atomic_load(&global_produced) == TOTAL_ITEMS);

    /* Wait until consumers have consumed all real items */
    while (atomic_load(&global_consumed) < TOTAL_ITEMS) {
        sleep_ms(2);
    }

    /* Wake/stop all consumers that might be blocked in dequeue() */
    for (int i = 0; i < CONSUMERS; i++) {
        enqueue(make_item(SENTINEL_VALUE));
    }

    for (int i = 0; i < CONSUMERS; i++) thrd_join(consumer_threads[i], NULL);

    /* Validate no missing */
    for (int i = 0; i < TOTAL_ITEMS; i++) {
        unsigned char v = atomic_load(&seen[i]);
        if (v != 1) {
            fprintf(stderr, "Missing item: %d\n", i);
            exit(1);
        }
    }

    /* visited() should be >= TOTAL_ITEMS (it includes sentinel dequeues too) */
    ASSERT_TRUE(visited() >= (size_t)TOTAL_ITEMS);

    free(seen);
    barrier_destroy(&start_barrier);
    destroyQueue();

    printf("test_stress(seed=%u): PASS\n", seed);
    return 0;
}
