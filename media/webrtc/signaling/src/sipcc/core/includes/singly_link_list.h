/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Cisco Systems SIP Stack.
 *
 * The Initial Developer of the Original Code is
 * Cisco Systems (CSCO).
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Enda Mannion <emannion@cisco.com>
 *  Suhas Nandakumar <snandaku@cisco.com>
 *  Ethan Hugg <ehugg@cisco.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _SINGLY_LINK_LIST_H
#define _SINGLY_LINK_LIST_H

typedef enum {
    SLL_MATCH_FOUND,
    SLL_MATCH_NOT_FOUND
} sll_match_e;

/*
 * type definition for find function pointer. The find function takes two arguments:
 * 1. find_by_p - pointer to the key
 * 2. data_p - pointer to the linked list node data.
 */
typedef sll_match_e(*sll_find_callback_t)(void *find_by_p, void *data_p);

typedef void *sll_handle_t;

typedef enum {
    SLL_RET_SUCCESS,
    SLL_RET_INVALID_ARGS,
    SLL_RET_MALLOC_FAILURE,
    SLL_RET_NODE_NOT_FOUND,
    SLL_RET_LIST_NOT_EMPTY,
    SLL_RET_OTHER_FAILURE
} sll_return_e;

/*
 * sll_create(): creates a signly linked list control block and initializes it.
 *               Applications shall call this first before performing any singly
 *               linked list primitives, such as append, remove, find or destroy.
 *
 * Parameters: find_fp - function pointer which will be used to find the matching node.
 *
 * Returns: list handle or NULL if it can not create the list.
 */
extern sll_handle_t sll_create(sll_find_callback_t find_fp);

/*
 * sll_destroy(): if the list is empty, it frees the list.
 *                It is the responsibility of the applications to empty
 *                the list before destroying the list.
 *
 * Parameters: list_handle - handle to the list.
 *
 * Returns: SLL_RET_SUCCESS if it successfully destroys.
 *          SLL_RET_INVALID_ARGS if the arguments are invalid.
 *          SLL_RET_LIST_NOT_EMPTY if the list is not empty.
 */
extern sll_return_e sll_destroy(sll_handle_t list_handle);

/*
 * sll_append(): creates a list node and appends it to the list.
 *               Applications are responsible for memory management of the
 *               data that the node will point to.
 *
 * Parameters: list_handle - handle to the list.
 *             data_p - pointer to the data that the list node will point to.
 *
 * Returns: SLL_RET_SUCCESS if it successfully appends.
 *          SLL_RET_INVALID_ARGS if the arguments are invalid.
 *          SLL_RET_MALLOC_FAILURE if memory allocation fails.
 */
extern sll_return_e sll_append(sll_handle_t list_handle, void *data_p);

/*
 * sll_remove(): removes the node from the list and frees the node.
 *               Applications are responsible for memory management of the
 *               data that the node points to.
 *
 * Parameters: list_handle - handle to the list.
 *             data_p - pointer to the data that the list node points to.
 *
 * Returns: SLL_RET_SUCCESS if it successfully removes.
 *          SLL_RET_INVALID_ARGS if the arguments are invalid.
 *          SLL_RET_NODE_NOT_FOUND if the node is not found in the list.
 */
extern sll_return_e sll_remove(sll_handle_t list_handle, void *data_p);

/*
 * sll_find(): finds the matching node data using find_fp function.
 *
 * Parameters: list_handle - handle to the list.
 *             find_by_p - pointer to the opaque data that will be used by find_fp function.
 *
 * Returns: pointer to the data or NULL if it can not find.
 */
extern void *sll_find(sll_handle_t list_handle, void *find_by_p);

/*
 * sll_next(): returns pointer to the data in the next node to the node holding data_p.
 *             if data_p is NULL, then returns pointer to the data in the first node.
 *             Applications can use this primitive to walk through the list. Typically,
 *             it can be used to remove the individual nodes and to destroy the list
 *             before shutting down/resetting the application.
 *
 * Parameters: list_handle - handle to the list.
 *             data_p - pointer to the data that the list node points to.
 *
 * Returns: pointer to the data or NULL if it can not find.
 */
extern void *sll_next(sll_handle_t list_handle, void *data_p);

/*
 * sll_count(): returns the number of elements in the list.
 *              count of the linked list.
 *
 * Parameters: list_handle - handle to the list.
 *
 * Returns: returns the number of elements in the list.
 */
extern unsigned int sll_count(sll_handle_t list_handle);

#endif
