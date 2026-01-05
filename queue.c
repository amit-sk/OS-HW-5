#include <stdbool.h>
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


typedef struct item_node {
    void *data;
    bool is_assigned;
    struct item_node *prev;
    struct item_node *next;
} item_node_t;

typedef struct waiting_thread_node {
    cnd_t *conditional_var;
    item_node_t *item;
    struct waiting_thread_node *next;
} waiting_thread_node_t;


// === globals ===

mtx_t lock;

// === queue globals ===
item_node_t *head = NULL;
item_node_t *tail = NULL;
atomic_int visited_count = 0;

// === globals for waiting threads ===
waiting_thread_node_t *waiting_queue_head = NULL;
waiting_thread_node_t *waiting_queue_tail = NULL;

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
    waiting_thread_node_t *waiting_thread = NULL;
    item_node_t *new_node = malloc(sizeof(item_node_t)); // You can assume malloc does not fail.
    memset(new_node, 0, sizeof(item_node_t));
    new_node->data = x;

    mtx_lock(&lock);
    
    if (head == NULL) {
        head = new_node;
        tail = head;
    } else {
        tail->next = new_node;
        new_node->prev = tail;
        tail = new_node;
    }

    if (waiting_queue_head != NULL) {
        // someone is waiting for an item to consume - signal first in line
        waiting_thread = waiting_queue_head;
        waiting_thread->item = new_node;
        cnd_signal(waiting_thread->conditional_var);

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
    item_node_t *node;
    
    mtx_lock(&lock);
    // if there is nothing to return right now, or if someone else is already waiting for this item - add this thread to the waiting queue
    if (head == NULL || head->is_assigned) {
        cnd_t *conditional_variable_ptr = malloc(sizeof(cnd_t));
        cnd_init(conditional_variable_ptr);

        waiting_thread_node_t *waiting_thread = malloc(sizeof(waiting_thread_node_t));
        memset(waiting_thread, 0, sizeof(waiting_thread_node_t));
        waiting_thread->conditional_var = conditional_variable_ptr;

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
        
        node = waiting_thread->item;
        free(waiting_thread->conditional_var);
        free(waiting_thread);
    } else {
        node = head;
    }
    
    // skip over the item in the queue
    if (node->prev != NULL) {
        node->prev->next = node->next;
    }
    if (node->next != NULL) {
        node->next->prev = node->prev;
    }

    // update head and tail
    if (tail == head) {
        // there was only one item in the queue - now will be empty
        tail = NULL;
    }
    if (head == node) {
        head = node->next;
    }
    if (tail == node) {
        tail = node->prev;
    }

    data = node->data;
    free(node);
    visited_count++;

    mtx_unlock(&lock);
    return data;
}

size_t visited(void) {
    return visited_count;
}

