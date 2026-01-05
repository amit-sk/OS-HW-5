#include "queue.h"
#include "test_common.h"
#include <stdatomic.h>

typedef struct {
    atomic_int started;
    atomic_int finished;
    void *result;
} consumer_state_t;

int consumer_thread(void *arg) {
    consumer_state_t *st = (consumer_state_t *)arg;
    atomic_store(&st->started, 1);

    void *item = dequeue();   /* should block until enqueue happens */
    st->result = item;

    atomic_store(&st->finished, 1);
    return 0;
}

int main(void) {
    initQueue();

    consumer_state_t st;
    atomic_init(&st.started, 0);
    atomic_init(&st.finished, 0);
    st.result = NULL;

    thrd_t t;
    ASSERT_TRUE(thrd_create(&t, consumer_thread, &st) == thrd_success);

    /* Wait until consumer thread started and likely blocked in dequeue */
    while (atomic_load(&st.started) == 0) { /* spin */ }

    /* Give it time to reach the blocking point */
    sleep_ms(50);

    /* Should still be blocked (not finished) because queue is empty */
    ASSERT_TRUE(atomic_load(&st.finished) == 0);

    int payload = 777;
    enqueue(&payload);

    /* Now it should complete relatively soon */
    for (int i = 0; i < 200; i++) {
        if (atomic_load(&st.finished)) break;
        sleep_ms(5);
    }
    ASSERT_TRUE(atomic_load(&st.finished) == 1);
    ASSERT_TRUE(*(int*)st.result == 777);

    thrd_join(t, NULL);

    destroyQueue();
    printf("test_blocking: PASS\n");
    return 0;
}
