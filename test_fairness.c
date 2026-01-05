#include "queue.h"
#include "test_common.h"
#include <stdatomic.h>
#include <stdint.h>

typedef struct {
    int consumer_index;
    barrier_t *start_barrier;
    atomic_int *sleep_order_counter;
    int my_sleep_order;
    void *got_item;
} consumer_arg_t;

int fairness_consumer(void *arg) {
    consumer_arg_t *a = (consumer_arg_t*)arg;

    /* synchronize all consumers to start together */
    barrier_wait(a->start_barrier);

    /* enforce a known sleep order: lower index sleeps first (small delay ladder) */
    sleep_ms(a->consumer_index * 5);

    a->my_sleep_order = atomic_fetch_add(a->sleep_order_counter, 1);

    /* should sleep because queue empty */
    a->got_item = dequeue();

    return 0;
}

int main(void) {
    initQueue();

    const int N = 20;
    thrd_t consumers[N];
    consumer_arg_t args[N];

    barrier_t start_barrier;
    barrier_init(&start_barrier, N + 1);

    atomic_int sleep_order_counter;
    atomic_init(&sleep_order_counter, 0);

    for (int i = 0; i < N; i++) {
        args[i].consumer_index = i;
        args[i].start_barrier = &start_barrier;
        args[i].sleep_order_counter = &sleep_order_counter;
        args[i].my_sleep_order = -1;
        args[i].got_item = NULL;
        ASSERT_TRUE(thrd_create(&consumers[i], fairness_consumer, &args[i]) == thrd_success);
    }

    /* release all consumers */
    barrier_wait(&start_barrier);

    /* wait until all consumers recorded their sleep order (meaning: all attempted dequeue) */
    while (atomic_load(&sleep_order_counter) < N) {
        sleep_ms(1);
    }

    /* Now enqueue N items labelled by sleep order: item[k] must go to the k-th sleeper */
    int items[N];
    for (int k = 0; k < N; k++) items[k] = 10000 + k;

    for (int k = 0; k < N; k++) {
        enqueue(&items[k]);
    }

    for (int i = 0; i < N; i++) {
        thrd_join(consumers[i], NULL);
    }

    /* Build inverse mapping: sleeper order -> which consumer had that order */
    int consumer_by_sleep_order[N];
    for (int k = 0; k < N; k++) consumer_by_sleep_order[k] = -1;

    for (int i = 0; i < N; i++) {
        int order = args[i].my_sleep_order;
        ASSERT_TRUE(order >= 0 && order < N);
        ASSERT_TRUE(consumer_by_sleep_order[order] == -1);
        consumer_by_sleep_order[order] = i;
    }

    /* Check each sleep order k got item[k] */
    for (int k = 0; k < N; k++) {
        int consumer_index = consumer_by_sleep_order[k];
        ASSERT_TRUE(consumer_index != -1);
        printf("consumer index: (%d)\n", consumer_index);

        int got = *(int*)args[consumer_index].got_item;
        int expected = 10000 + k;
        printf("expecting: (%d), got (%d)\n", expected, got);
        ASSERT_TRUE(got == expected);
    }

    barrier_destroy(&start_barrier);
    destroyQueue();

    printf("test_fairness: PASS\n");
    return 0;
}
