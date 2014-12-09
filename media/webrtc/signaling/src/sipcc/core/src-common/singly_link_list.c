/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr_stdio.h"
#include "cpr_stdlib.h"
#include "singly_link_list.h"

/*
 * The following typedefs are included here because we do not want to
 * reveal the internals to other modules.
 */
typedef struct slink_list_node_t {
    struct slink_list_node_t *next_p; /* pointer to next node */
    void                     *data_p; /* pointer to node data */
} slink_list_node_t;

typedef struct {
    slink_list_node_t  *first_p; /* pointer to first node */
    slink_list_node_t  *last_p;  /* pointer to last node */
    unsigned int        count;   /* number of nodes in the list */
    sll_find_callback_t find_fp; /* find function used to find a matching node */
} slink_list_t;

/*
 *     LIST          NODE-1        NODE-2        NODE-3
 *   -----------    ----------    ----------    ----------
 *   | first_p |--->| next_p |--->| next_p |--->| next_p |--->NULL
 *   | count   |    | data_p |    | data_p | |->| data_p |
 *   | find_fp |    ----------    ---------- |  ----------
 *   | last_p  |-----------------------------|
 *   -----------
 */

/*
 * sll_create(): creates a signly linked list control block and initializes it.
 *
 * Parameters: find_fp - function pointer which will be used to find
 *                       the matching node.
 *
 * Returns: list handle or NULL if it can not create the list.
 */
sll_handle_t
sll_create (sll_find_callback_t find_fp)
{
    slink_list_t *link_list_p;

    link_list_p = (slink_list_t *) cpr_malloc(sizeof(slink_list_t));
    if (link_list_p == NULL) {
        return NULL;
    }
    link_list_p->first_p = NULL;
    link_list_p->last_p = NULL;
    link_list_p->count = 0;
    link_list_p->find_fp = find_fp;

    return (sll_handle_t) link_list_p;
}

/*
 * sll_destroy(): if the list is empty, it frees the list.
 *
 * Parameters: list_handle - handle to the list.
 *
 * Returns: SLL_RET_SUCCESS if it successfully destroys.
 *          SLL_RET_INVALID_ARGS if the arguments are invalid.
 *          SLL_RET_LIST_NOT_EMPTY if the list is not empty.
 */
sll_return_e
sll_destroy (sll_handle_t list_handle)
{
    slink_list_t *list_p = (slink_list_t *) list_handle;

    /*
     * validate the arguments.
     */
    if (list_p == NULL) {
        return SLL_RET_INVALID_ARGS;
    }

    /*
     * check if the list is empty.
     */
    if (list_p->count != 0) {
        return SLL_RET_LIST_NOT_EMPTY;
    }

    /*
     * free the memory allocated for list.
     */
    cpr_free(list_p);
    return SLL_RET_SUCCESS;
}

/*
 * sll_append(): creates a list node and appends it to the list.
 *
 * Parameters: list_handle - handle to the list.
 *             data_p - pointer to the data that the list node will point to.
 *
 * Returns: SLL_RET_SUCCESS if it successfully appends.
 *          SLL_RET_INVALID_ARGS if the arguments are invalid.
 *          SLL_RET_MALLOC_FAILURE if memory allocation fails.
 */
sll_return_e
sll_append (sll_handle_t list_handle, void *data_p)
{
    slink_list_t *list_p = (slink_list_t *) list_handle;
    slink_list_node_t *list_node_p;

    /*
     * validate the arguments.
     */
    if ((list_p == NULL) || (data_p == NULL)) {
        return SLL_RET_INVALID_ARGS;
    }

    /*
     * create the node.
     */
    list_node_p = (slink_list_node_t *) cpr_malloc(sizeof(slink_list_node_t));
    if (list_node_p == NULL) {
        return SLL_RET_MALLOC_FAILURE;
    }
    list_node_p->next_p = NULL;
    list_node_p->data_p = data_p;

    /*
     * append to the list.
     */
    if (list_p->first_p == NULL) { /* if the list is empty */
        list_p->first_p = list_p->last_p = list_node_p;
    } else {
        list_p->last_p->next_p = list_node_p;
        list_p->last_p = list_node_p;
    }
    list_p->count = list_p->count + 1;

    return SLL_RET_SUCCESS;
}

/*
 * sll_remove(): removes the node from the list and frees the node.
 *
 * Parameters: list_handle - handle to the list.
 *             data_p - pointer to the data that the list node points to.
 *
 * Returns: SLL_RET_SUCCESS if it successfully removes.
 *          SLL_RET_INVALID_ARGS if the arguments are invalid.
 *          SLL_RET_NODE_NOT_FOUND if the node is not found in the list.
 */
sll_return_e
sll_remove (sll_handle_t list_handle, void *data_p)
{
    slink_list_t *list_p = (slink_list_t *) list_handle;
    slink_list_node_t *list_node_p;
    slink_list_node_t *prev_node_p;

    /*
     * validate the arguments.
     */
    if ((list_p == NULL) || (data_p == NULL)) {
        return SLL_RET_INVALID_ARGS;
    }

    /*
     * search for the node to be removed
     */
    prev_node_p = NULL;
    list_node_p = list_p->first_p;
    while (list_node_p) {
        if (list_node_p->data_p == data_p) {
            break;
        }
        prev_node_p = list_node_p;
        list_node_p = list_node_p->next_p;
    }
    if (list_node_p == NULL) {
        return SLL_RET_NODE_NOT_FOUND;
    }

    /*
     * remove the node.
     */
    if (prev_node_p == NULL) { /* if we are removing the first node */
        list_p->first_p = list_node_p->next_p;
        if (list_p->last_p == list_node_p) { /* if it had only one node */
            list_p->last_p = list_p->first_p;
        }
    } else {
        prev_node_p->next_p = list_node_p->next_p;
        if (list_p->last_p == list_node_p) { /* if we are removing last node */
            list_p->last_p = prev_node_p;
        }
    }
    cpr_free(list_node_p);
    list_p->count = list_p->count - 1;

    return SLL_RET_SUCCESS;
}

/*
 * sll_find(): finds the matching node data using find_fp function.
 *
 * Parameters: list_handle - handle to the list.
 *             find_by_p - pointer to the opaque data that will be used
 *                         by find_fp function.
 *
 * Returns: pointer to the data or NULL if it can not find.
 */
void *
sll_find (sll_handle_t list_handle, void *find_by_p)
{
    slink_list_t *list_p = (slink_list_t *) list_handle;
    slink_list_node_t *list_node_p;
    sll_match_e match;

    /*
     * validate the arguments.
     */
    if ((list_p == NULL) || (find_by_p == NULL) || (list_p->find_fp == NULL)) {
        return NULL;
    }

    list_node_p = list_p->first_p;
    while (list_node_p) {
        match = (*(list_p->find_fp))(find_by_p, list_node_p->data_p);
        if (match == SLL_MATCH_FOUND) {
            break;
        }
        list_node_p = list_node_p->next_p;
    }

    if (list_node_p == NULL) {
        return NULL;
    }

    return list_node_p->data_p;
}

/*
 * sll_next(): returns pointer to the data in the next node to the node
 *             holding data_p.  if data_p is NULL, then returns pointer
 *             to the data in the first node.
 *
 * Parameters: list_handle - handle to the list.
 *             data_p - pointer to the data that the list node points to.
 *
 * Returns: pointer to the data or NULL if it can not find.
 */
void *
sll_next (sll_handle_t list_handle, void *data_p)
{
    slink_list_t *list_p = (slink_list_t *) list_handle;
    slink_list_node_t *list_node_p;

    /*
     * validate the arguments.
     */
    if (list_p == NULL) {
        return NULL;
    }

    if (data_p == NULL) { /* if the first node is requested */
        if (list_p->first_p == NULL) {
            return NULL;
        }
        return list_p->first_p->data_p;
    }

    list_node_p = list_p->first_p;
    while (list_node_p) {
        if (list_node_p->data_p == data_p) {
            break;
        }
        list_node_p = list_node_p->next_p;
    }

    if (list_node_p == NULL) {
        return NULL;
    }

    if (list_node_p->next_p == NULL) {
        return NULL;
    }

    return list_node_p->next_p->data_p;
}

/*
 * sll_count(): returns the number of elements in the list.
 *              count of the linked list.
 *
 * Parameters: list_handle - handle to the list.
 *
 * Returns: returns the number of elements in the list.
 */
unsigned int
sll_count (sll_handle_t list_handle)
{
    slink_list_t *list_p = (slink_list_t *) list_handle;

    return (list_p->count);
}

/*************************************** THE END **************************/
