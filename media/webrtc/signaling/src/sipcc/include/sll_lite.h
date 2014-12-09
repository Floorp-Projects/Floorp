/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _SLL_LITE_H
#define _SLL_LITE_H

#include "cpr_types.h"
/*
 * List node structure
 */
typedef struct sll_lite_node_t_ {
    struct sll_lite_node_t_ *next_p;   /* pointer to the next node */
} sll_lite_node_t;

/*
 * List control structure
 */
typedef struct sll_lite_list_t_ {
    uint16_t        count;    /* number of elements on the list    */
    sll_lite_node_t *head_p;  /* pointer to the head or first node */
    sll_lite_node_t *tail_p;  /* pointer to the tail or last node  */
} sll_lite_list_t;

typedef enum {
    SLL_LITE_RET_SUCCESS,
    SLL_LITE_RET_INVALID_ARGS,
    SLL_LITE_RET_NODE_NOT_FOUND,
    SLL_LITE_RET_OTHER_FAILURE
} sll_lite_return_e;

/*
 * Convenient macros
 */
#define SLL_LITE_LINK_HEAD(link) \
     (((sll_lite_list_t *)link)->head_p)

#define SLL_LITE_LINK_TAIL(link) \
     (((sll_lite_list_t *)link)->tail_p)

#define SLL_LITE_LINK_NEXT_NODE(node) \
     (((sll_lite_node_t *)node)->next_p)

#define SLL_LITE_NODE_COUNT(link) \
     (((sll_lite_list_t *)link)->count)

/**
 * sll_lite_init initializes list control structure given by the
 * caller.
 *
 * @param[in] list - pointer to the list control structure
 *                  sll_lite_list_t
 *
 * @return        - SLL_LITE_RET_SUCCESS for success
 *                - SLL_LITE_RET_INVALID_ARGS when arguments are
 *                  invalid.
 */
extern sll_lite_return_e
sll_lite_init(sll_lite_list_t *list);

/**
 * sll_lite_link_head puts node to the head of the list.
 *
 * @param[in] list - pointer to the list control structure
 *                  sll_lite_list_t. The list must be
 *                  initialized prior.
 * @param[in] node - pointer to the list node structure.
 *
 * @return        - SLL_LITE_RET_SUCCESS for success
 *                - SLL_LITE_RET_INVALID_ARGS when arguments are
 *                  invalid.
 */
extern sll_lite_return_e
sll_lite_link_head(sll_lite_list_t *list, sll_lite_node_t *node);

/**
 * sll_lite_link_tail puts node to the tail of the list.
 *
 * @param[in] list - pointer to the list control structure
 *                  sll_lite_list_t. The list must be
 *                  initialized prior.
 * @param[in] node - pointer to the list node structure.
 *
 * @return        - SLL_LITE_RET_SUCCESS for success
 *                - SLL_LITE_RET_INVALID_ARGS when arguments are
 *                  invalid.
 */
sll_lite_return_e
sll_lite_link_tail(sll_lite_list_t *list, sll_lite_node_t *node);

/**
 * sll_lite_unlink_head removes head node from the head of the list and
 * returns it to the caller.
 *
 * @param[in] list - pointer to the list control structure
 *                  sll_lite_list_t. The list must be
 *                  initialized prior.
 *
 * @return        Pointer to the head node if one exists otherwise
 *                return NULL.
 */
extern sll_lite_node_t *
sll_lite_unlink_head(sll_lite_list_t *list);

/**
 * sll_lite_unlink_tail removes tail node from the list and
 * returns it to the caller.
 *
 * @param[in] list - pointer to the list control structure
 *                  sll_lite_list_t. The list must be
 *                  initialized prior.
 *
 * @return        Pointer to the tail node if one exists otherwise
 *                return NULL.
 */
sll_lite_node_t *
sll_lite_unlink_tail(sll_lite_list_t *list);

/**
 * sll_lite_remove removes the given node from the list.
 *
 * @param[in] list - pointer to the list control structure
 *                  sll_lite_list_t. The list must be
 *                  initialized prior.
 * @param[in] node - pointer to the list node structure to be
 *                  removed.
 *
 * @return        - SLL_LITE_RET_SUCCESS for success
 *                - SLL_LITE_RET_INVALID_ARGS when arguments are
 *                  invalid.
 *                - SLL_LITE_RET_NODE_NOT_FOUND when the node
 *                  to remove is not found.
 */
extern sll_lite_return_e
sll_lite_remove(sll_lite_list_t *list, sll_lite_node_t *node);

#endif
