/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * list.c
 *
 * This contains the implementation of NSS's thread-safe linked list.
 */

#ifndef BASE_H
#include "base.h"
#endif /* BASE_H */

struct nssListElementStr {
    PRCList link;
    void *data;
};

typedef struct nssListElementStr nssListElement;

struct nssListStr {
    NSSArena *arena;
    PZLock *lock;
    nssListElement *head;
    PRUint32 count;
    nssListCompareFunc compareFunc;
    nssListSortFunc sortFunc;
    PRBool i_alloced_arena;
};

struct nssListIteratorStr {
    PZLock *lock;
    nssList *list;
    nssListElement *current;
};

#define NSSLIST_LOCK_IF(list) \
    if ((list)->lock)         \
    PZ_Lock((list)->lock)

#define NSSLIST_UNLOCK_IF(list) \
    if ((list)->lock)           \
    PZ_Unlock((list)->lock)

static PRBool
pointer_compare(void *a, void *b)
{
    return (PRBool)(a == b);
}

static nssListElement *
nsslist_get_matching_element(nssList *list, void *data)
{
    nssListElement *node;
    node = list->head;
    if (!node) {
        return NULL;
    }
    while (node) {
        /* using a callback slows things down when it's just compare ... */
        if (list->compareFunc(node->data, data)) {
            break;
        }
        if (&node->link == PR_LIST_TAIL(&list->head->link)) {
            node = NULL;
            break;
        }
        node = (nssListElement *)PR_NEXT_LINK(&node->link);
    }
    return node;
}

NSS_IMPLEMENT nssList *
nssList_Create(NSSArena *arenaOpt, PRBool threadSafe)
{
    NSSArena *arena;
    nssList *list;
    PRBool i_alloced;
    if (arenaOpt) {
        arena = arenaOpt;
        i_alloced = PR_FALSE;
    } else {
        arena = nssArena_Create();
        i_alloced = PR_TRUE;
    }
    if (!arena) {
        return (nssList *)NULL;
    }
    list = nss_ZNEW(arena, nssList);
    if (!list) {
        if (!arenaOpt) {
            NSSArena_Destroy(arena);
        }
        return (nssList *)NULL;
    }
    if (threadSafe) {
        list->lock = PZ_NewLock(nssILockOther);
        if (!list->lock) {
            if (arenaOpt) {
                nss_ZFreeIf(list);
            } else {
                NSSArena_Destroy(arena);
            }
            return (nssList *)NULL;
        }
    }
    list->arena = arena;
    list->i_alloced_arena = i_alloced;
    list->compareFunc = pointer_compare;
    return list;
}

NSS_IMPLEMENT PRStatus
nssList_Destroy(nssList *list)
{
    if (!list) {
        return PR_SUCCESS;
    }
    if (!list->i_alloced_arena) {
        nssList_Clear(list, NULL);
    }
    if (list->lock) {
        (void)PZ_DestroyLock(list->lock);
    }
    if (list->i_alloced_arena) {
        NSSArena_Destroy(list->arena);
        list = NULL;
    }
    nss_ZFreeIf(list);
    return PR_SUCCESS;
}

NSS_IMPLEMENT void
nssList_SetCompareFunction(nssList *list, nssListCompareFunc compareFunc)
{
    list->compareFunc = compareFunc;
}

NSS_IMPLEMENT void
nssList_SetSortFunction(nssList *list, nssListSortFunc sortFunc)
{
    /* XXX if list already has elements, sort them */
    list->sortFunc = sortFunc;
}

NSS_IMPLEMENT nssListCompareFunc
nssList_GetCompareFunction(nssList *list)
{
    return list->compareFunc;
}

NSS_IMPLEMENT void
nssList_Clear(nssList *list, nssListElementDestructorFunc destructor)
{
    PRCList *link;
    nssListElement *node, *tmp;
    if (!list) {
        return;
    }
    NSSLIST_LOCK_IF(list);
    node = list->head;
    list->head = NULL;
    while (node && list->count > 0) {
        if (destructor)
            (*destructor)(node->data);
        link = &node->link;
        tmp = (nssListElement *)PR_NEXT_LINK(link);
        PR_REMOVE_LINK(link);
        nss_ZFreeIf(node);
        node = tmp;
        --list->count;
    }
    NSSLIST_UNLOCK_IF(list);
}

static PRStatus
nsslist_add_element(nssList *list, void *data)
{
    nssListElement *node = nss_ZNEW(list->arena, nssListElement);
    if (!node) {
        return PR_FAILURE;
    }
    PR_INIT_CLIST(&node->link);
    node->data = data;
    if (list->head) {
        if (list->sortFunc) {
            PRCList *link;
            nssListElement *currNode;
            currNode = list->head;
            /* insert in ordered list */
            while (currNode) {
                link = &currNode->link;
                if (list->sortFunc(data, currNode->data) <= 0) {
                    /* new element goes before current node */
                    PR_INSERT_BEFORE(&node->link, link);
                    /* reset head if this is first */
                    if (currNode == list->head)
                        list->head = node;
                    break;
                }
                if (link == PR_LIST_TAIL(&list->head->link)) {
                    /* reached end of list, append */
                    PR_INSERT_AFTER(&node->link, link);
                    break;
                }
                currNode = (nssListElement *)PR_NEXT_LINK(&currNode->link);
            }
        } else {
            /* not sorting */
            PR_APPEND_LINK(&node->link, &list->head->link);
        }
    } else {
        list->head = node;
    }
    ++list->count;
    return PR_SUCCESS;
}

NSS_IMPLEMENT PRStatus
nssList_Add(nssList *list, void *data)
{
    NSSLIST_LOCK_IF(list);
    (void)nsslist_add_element(list, data);
    NSSLIST_UNLOCK_IF(list);
    return PR_SUCCESS;
}

NSS_IMPLEMENT PRStatus
nssList_AddUnique(nssList *list, void *data)
{
    PRStatus nssrv;
    nssListElement *node;
    NSSLIST_LOCK_IF(list);
    node = nsslist_get_matching_element(list, data);
    if (node) {
        /* already in, finish */
        NSSLIST_UNLOCK_IF(list);
        return PR_SUCCESS;
    }
    nssrv = nsslist_add_element(list, data);
    NSSLIST_UNLOCK_IF(list);
    return nssrv;
}

NSS_IMPLEMENT PRStatus
nssList_Remove(nssList *list, void *data)
{
    nssListElement *node;
    NSSLIST_LOCK_IF(list);
    node = nsslist_get_matching_element(list, data);
    if (node) {
        if (node == list->head) {
            list->head = (nssListElement *)PR_NEXT_LINK(&node->link);
        }
        PR_REMOVE_LINK(&node->link);
        nss_ZFreeIf(node);
        if (--list->count == 0) {
            list->head = NULL;
        }
    }
    NSSLIST_UNLOCK_IF(list);
    return PR_SUCCESS;
}

NSS_IMPLEMENT void *
nssList_Get(nssList *list, void *data)
{
    nssListElement *node;
    NSSLIST_LOCK_IF(list);
    node = nsslist_get_matching_element(list, data);
    NSSLIST_UNLOCK_IF(list);
    return (node) ? node->data : NULL;
}

NSS_IMPLEMENT PRUint32
nssList_Count(nssList *list)
{
    return list->count;
}

NSS_IMPLEMENT PRStatus
nssList_GetArray(nssList *list, void **rvArray, PRUint32 maxElements)
{
    nssListElement *node;
    PRUint32 i = 0;
    PR_ASSERT(maxElements > 0);
    node = list->head;
    if (!node) {
        return PR_SUCCESS;
    }
    NSSLIST_LOCK_IF(list);
    while (node) {
        rvArray[i++] = node->data;
        if (i == maxElements)
            break;
        node = (nssListElement *)PR_NEXT_LINK(&node->link);
        if (node == list->head) {
            break;
        }
    }
    NSSLIST_UNLOCK_IF(list);
    return PR_SUCCESS;
}

NSS_IMPLEMENT nssList *
nssList_Clone(nssList *list)
{
    nssList *rvList;
    nssListElement *node;
    rvList = nssList_Create(NULL, (list->lock != NULL));
    if (!rvList) {
        return NULL;
    }
    NSSLIST_LOCK_IF(list);
    if (list->count > 0) {
        node = list->head;
        while (PR_TRUE) {
            nssList_Add(rvList, node->data);
            node = (nssListElement *)PR_NEXT_LINK(&node->link);
            if (node == list->head) {
                break;
            }
        }
    }
    NSSLIST_UNLOCK_IF(list);
    return rvList;
}

NSS_IMPLEMENT nssListIterator *
nssList_CreateIterator(nssList *list)
{
    nssListIterator *rvIterator;
    rvIterator = nss_ZNEW(NULL, nssListIterator);
    if (!rvIterator) {
        return NULL;
    }
    rvIterator->list = nssList_Clone(list);
    if (!rvIterator->list) {
        nss_ZFreeIf(rvIterator);
        return NULL;
    }
    rvIterator->current = rvIterator->list->head;
    if (list->lock) {
        rvIterator->lock = PZ_NewLock(nssILockOther);
        if (!rvIterator->lock) {
            nssList_Destroy(rvIterator->list);
            nss_ZFreeIf(rvIterator);
            rvIterator = NULL;
        }
    }
    return rvIterator;
}

NSS_IMPLEMENT void
nssListIterator_Destroy(nssListIterator *iter)
{
    if (iter->lock) {
        (void)PZ_DestroyLock(iter->lock);
    }
    if (iter->list) {
        nssList_Destroy(iter->list);
    }
    nss_ZFreeIf(iter);
}

NSS_IMPLEMENT void *
nssListIterator_Start(nssListIterator *iter)
{
    NSSLIST_LOCK_IF(iter);
    if (iter->list->count == 0) {
        return NULL;
    }
    iter->current = iter->list->head;
    return iter->current->data;
}

NSS_IMPLEMENT void *
nssListIterator_Next(nssListIterator *iter)
{
    nssListElement *node;
    PRCList *link;
    if (iter->list->count == 1 || iter->current == NULL) {
        /* Reached the end of the list.  Don't change the state, force to
         * user to call nssList_Finish to clean up.
         */
        return NULL;
    }
    node = (nssListElement *)PR_NEXT_LINK(&iter->current->link);
    link = &node->link;
    if (link == PR_LIST_TAIL(&iter->list->head->link)) {
        /* Signal the end of the list. */
        iter->current = NULL;
        return node->data;
    }
    iter->current = node;
    return node->data;
}

NSS_IMPLEMENT PRStatus
nssListIterator_Finish(nssListIterator *iter)
{
    iter->current = iter->list->head;
    return (iter->lock) ? PZ_Unlock(iter->lock) : PR_SUCCESS;
}
