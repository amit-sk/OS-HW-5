#include "queue.h"
#include "test_common.h"
#include <stdint.h>

int main(void) {
    initQueue();

    /* enqueue/dequeue FIFO single-thread */
    int a = 1, b = 2, c = 3;
    enqueue(&a);
    enqueue(&b);
    enqueue(&c);

    int *pa = (int*)dequeue();
    int *pb = (int*)dequeue();
    int *pc = (int*)dequeue();

    ASSERT_TRUE(*pa == 1);
    ASSERT_TRUE(*pb == 2);
    ASSERT_TRUE(*pc == 3);

    /* visited should be 3 after these dequeues */
    ASSERT_TRUE(visited() == 3);

    destroyQueue();
    printf("test_basic: PASS\n");
    return 0;
}
