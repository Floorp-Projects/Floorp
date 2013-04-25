/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr_types.h"
#include "cpr_stdlib.h"
#include "cpr_stdio.h"
#include "cpr_string.h"
#include "util_ios_queue.h"
#include "CSFLog.h"

/* Forward function declarations */
void enqueue_inline(queuetype *qptr, void *eaddr);
void *dequeue_inline(queuetype *qptr);
void unqueue_inline(queuetype *q, void *e);
void requeue_inline(queuetype *qptr, void *eaddr);

/* Type definitions */
#define validmem(x) x

/*
 * queue_init
 * Initialize a queuetype
 */
void
queue_init (queuetype * q, int maximum)
{
    q->qhead = NULL;
    q->qtail = NULL;
    q->count = 0;
    q->maximum = maximum;
}

#ifdef MGCP_FIXME

/*
 * peekqueuehead -- Return address of element at head of queue.
 * Does not lock out interrupts.
 */
void *
peekqueuehead (queuetype *q)
{
    nexthelper *p;

    p = (nexthelper *) q->qhead;    /* first member */
    return (p);

}
#endif

/*
 * enqueue - add an element to a fifo queue.
 * Note no interrupt interlocking.
 */
void
enqueue (queuetype *qptr, void *eaddr)
{
    nexthelper *node;

    node = (nexthelper *) eaddr;
    node->next = NULL;
    enqueue_inline(qptr, (void *)node);
}

/*
 * dequeue - remove first element of a fifo queue.
 * Note no interrupt interlocking.
 */
void *
dequeue (queuetype *qptr)
{
    return (dequeue_inline(qptr));
}

/*
 * enqueue_inline - add an element to a fifo queue.
 */
void
enqueue_inline (queuetype *qptr, void *eaddr)
{
    nexthelper *p, *ptr;

    p = (nexthelper *) qptr->qtail; /* last element pointer */
    ptr = (nexthelper *) eaddr;
    /*
     * Make sure the element isn't already queued or the last
     * element in this list
     */
    if ((ptr->next != NULL) || (p == ptr)) {
        CSFLogError("src-common", "Queue: Error, queue corrupted %p %p",
                qptr, eaddr);
        return;
    }
    if (!p)                     /* q empty */
        qptr->qhead = ptr;
    else                        /* not empty */
        p->next = ptr;          /* update link */
    qptr->qtail = ptr;          /* tail points to new element */
    qptr->count++;
}

/*
 * dequeue_inline - remove first element of a fifo queue.
 */
void *
dequeue_inline (queuetype * qptr)
{
    nexthelper *p;

    if (qptr == NULL)
        return (NULL);
    p = (nexthelper *) qptr->qhead; /* points to head of queue */
    if (p) {                    /* is there a list? */
        qptr->qhead = p->next;  /* next link */
        if (!p->next) {
            qptr->qtail = NULL; /* this was last guy, so zap tail */
        }
        p->next = NULL;         /* clear link, just in case */
    }
    if (p && (--qptr->count < 0) && qptr->maximum) {
        CSFLogError("src-common",
          "Queue: Error, queue count under or over %d\n", qptr->count);
        qptr->count = 0;
    }
    return (p);
}

#ifdef MGCP_FIXME
/*
 * queue_create_init initializes the queue structure
 *
 * queue_ptr_t *queue: pointer to pointer to the queue structure
 * The function returns TRUE if the queue can be initialized, FALSE otherwise
 */
boolean
queue_create_init (queue_ptr_t *queue)
{
    if (queue) {
        *queue = cpr_malloc(sizeof(queuetype));
        if (*queue == NULL)
            return FALSE;

        queue_init((queuetype *) *queue, 0);
        return TRUE;
    } else {
        return FALSE;
    }
}

/*
 * queue_delete deletes the queue structure initialized by
 * queue_create_init previously
 *
 * queue_ptr_t queue: pointer to the queue structure
 * The function does not return any value
 */
void
queue_delete (queue_ptr_t queue)
{
    if (queue) {
        cpr_free((queuetype *) queue);
    }
}


/*
 *
 * add_list add a node to the head of the queue
 * queue_ptr_t queue: pointer to the queue structure
 * node_ptr_t node: node to add to the queue
 * The function does not return any value
 */
void
add_list (queue_ptr_t queue, node_ptr_t node)
{
    if (queue && node) {
        node->next = NULL;
        enqueue_inline((queuetype *) queue, node);
    }
}

/*
 * get_node returns a node from the queue. The head of the queue is
 * returned if the argument node is pointing to NULL, or the node next to it is
 * returned if it is pointing to non-NULL
 *
 * queue_ptr_t queue: pointer to the queue structure
 * node_ptr_t node: pointer to pointer to a node in the queue if the next
 * node is to be returned, other wise pointer to NULL to return the head of
 * the queue
 * The function returns a node from the queue in the argument node and
 * return value GET_NODE_SUCCESS, or GET_NODE_FAIL_EMPTY_LIST if list
 * is empty, or GET_NODE_FAIL_END_OF_LIST if end of list is reached
 */
get_node_status
get_node (queue_ptr_t queue, node_ptr_t *node)
{
    if (queue && node) {
        if (!*node) {
            /* get from head of the queue */
            *node = peekqueuehead(queue);
            if (!*node)
                return GET_NODE_FAIL_EMPTY_LIST;
            else
                return GET_NODE_SUCCESS;
        } else {
            *node = (*node)->next;
            if (!*node)
                return GET_NODE_FAIL_END_OF_LIST;
            else
                return GET_NODE_SUCCESS;
        }
    } else {
        return GET_NODE_FAIL_EMPTY_LIST;
    }
}
#endif
