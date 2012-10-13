/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _UTIL_IOS_QUEUE_H
#define _UTIL_IOS_QUEUE_H

/*
 * Define the queue data type and a basic enqueue structure
 */

typedef struct queuetype_ {
    void *qhead;                /* head of queue */
    void *qtail;                /* tail of queue */
    int count;                  /* possible count */
    int maximum;                /* maximum entries */
} queuetype;

typedef queuetype *queue_ptr_t;

typedef struct nexthelper_
{
    struct nexthelper_ *next;
    unsigned char data[4];
} queue_node, nexthelper, *node_ptr_t;

typedef enum get_node_status_ {
    GET_NODE_FAIL_EMPTY_LIST,
    GET_NODE_FAIL_END_OF_LIST,
    GET_NODE_SUCCESS
} get_node_status;

void queue_init(queuetype *q, int maximum);
void *peekqueuehead(queuetype* q);

/*Functions used by SIP */
int queryqueuedepth(queuetype const *q);
boolean checkqueue(queuetype *q, void *e);
void *p_dequeue(queuetype *qptr);
void p_enqueue(queuetype *qptr, void *eaddr);
void p_requeue(queuetype *qptr, void *eaddr);
void p_swapqueue(queuetype *qptr, void *enew, void *eold);
void p_unqueue(queuetype *q, void *e);
void p_unqueuenext(queuetype *q, void **prev);
void enqueue(queuetype *qptr, void *eaddr);
void *dequeue(queuetype *qptr);
void unqueue(queuetype *q, void *e);
void requeue(queuetype *qptr, void *eaddr);
void *remqueue(queuetype *qptr, void *eaddr, void *paddr);
void insqueue(queuetype *qptr, void *eaddr, void *paddr);
boolean queueBLOCK(queuetype *qptr);
boolean validqueue(queuetype *qptr, boolean print_message);

boolean queue_create_init(queue_ptr_t *queue);
void add_list(queue_ptr_t queue, node_ptr_t node);
get_node_status get_node(queue_ptr_t queue, node_ptr_t *node);
void queue_delete(queue_ptr_t queue);

#endif
