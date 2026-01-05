#include <stdatomic.h>
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

// === queue globals ===
node_t *head = NULL;
node_t *tail = NULL;
atomic_int visited_count = 0;

// === globals for waiting threads ===
node_t *waiting_queue_head = NULL;
node_t *waiting_queue_tail = NULL;

void initQueue(void) {
    mtx_init(&lock, mtx_plain);
}

void destroyQueue(void) {
    /*
     * Assuming:
     * – No threads are currently executing enqueue() or dequeue()
     * – No threads are blocked/sleeping in dequeue() waiting for items
     * – The queue is empty when destroyQueue() is called
    */
    mtx_destroy(&lock);
}

void enqueue(void *x) {
    node_t *waiting_thread = NULL;
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

    if (waiting_queue_head != NULL) {
        // someone is waiting for an item to consume - signal first in line
        waiting_thread = waiting_queue_head;
        cnd_signal((cnd_t *)waiting_thread->data);

        if (waiting_queue_tail == waiting_queue_head) {
            // there was only one waiting thread - now none.
            waiting_queue_tail = NULL;
        }
        waiting_queue_head = waiting_queue_head->next;
    }

    mtx_unlock(&lock);
}

void *dequeue(void) {
    void *data;
    node_t *node;
    
    mtx_lock(&lock);
    // if there is nothing to return right now, or if someone else is already waiting for this item - add this thread to the waiting queue
    if (head == NULL || waiting_threads_count > 0) {
        cnd_t *conditional_variable_ptr = malloc(sizeof(cnd_t));
        cnd_init(conditional_variable_ptr);

        node_t *waiting_thread = malloc(sizeof(node_t));
        waiting_thread->data = conditional_variable_ptr;
        waiting_thread->next = 0;

        // add to waiting threads queue
        if (waiting_queue_head == NULL) {
            waiting_queue_head = waiting_thread;
            waiting_queue_tail = waiting_queue_head;
        } else {
            waiting_queue_tail->next = waiting_thread;
            waiting_queue_tail = waiting_thread;
        }
 
        cnd_wait(conditional_variable_ptr, &lock);
        cnd_destroy(conditional_variable_ptr);
        free(waiting_thread->data);
        free(waiting_thread);
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

