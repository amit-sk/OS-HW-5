#include<stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

void initQueue(void);
void destroyQueue(void);
void enqueue(void*);
void *dequeue(void);
size_t visited(void);

typedef struct node {
    void *data;
    struct node *next;
} node_t;

mtx_t lock;
cnd_t notEmpty;
node_t *head = NULL;
node_t *tail = NULL;
atomic_int visited_count = 0;

void initQueue(void) {
    mtx_init(&lock, mtx_plain);
    cnd_init(&notEmpty);
}

void destroyQueue(void) {
    /*
     * Assuming:
     * – No threads are currently executing enqueue() or dequeue()
     * – No threads are blocked/sleeping in dequeue() waiting for items
     * – The queue is empty when destroyQueue() is called
    */
    mtx_destroy(&lock);
    cnd_destroy(&notEmpty);
}

void enqueue(void *x) {
    node_t *new_node = malloc(sizeof(node_t)); // You can assume malloc does not fail.
    new_node->data = x;
    new_node->next = NULL;

    mtx_lock(&lock);
    
    if (head == NULL) {
        head = new_node;
        tail = head;
    } else {
        tail->next = new_node;
        tail = new_node;
    }

    cnd_signal(&notEmpty);
    mtx_unlock(&lock);
}

void *dequeue(void) {
    void *data;
    node_t *node;
    
    mtx_lock(&lock);
    while (head == NULL) {
        cnd_wait(&notEmpty, &lock);
    }
    
    node = head;
    if (tail == head) {
        // there was only one item in the queue - now will be empty
        tail = NULL;
    }
    head = head->next;

    data = node->data;
    free(node);
    visited_count++;

    mtx_unlock(&lock);
    return data;
}

size_t visited(void) {
    return visited_count;
}

