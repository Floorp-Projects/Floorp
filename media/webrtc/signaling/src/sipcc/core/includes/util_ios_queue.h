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
