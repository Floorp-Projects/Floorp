/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */
#include "collectn.h"
#include "prerror.h"
#include "prmem.h"
#include "seccomon.h"
#include "serv.h"


SSMCollection *
SSM_NewCollection(void)
{
    SSMCollection *coll;

    coll = PR_NEWZAP(SSMCollection);
    if (coll == NULL)
        goto loser;

    coll->lock = PR_NewMonitor();
    if (coll->lock == NULL)
        goto loser;

    PR_INIT_CLIST(&coll->list);

    return coll;

 loser:
    SSM_DestroyCollection(coll);
    return NULL;
}

SSMStatus
SSM_DestroyCollection(SSMCollection *victim)
{
    if (victim == NULL)
        return PR_SUCCESS;

    PR_ASSERT(victim->nItems == 0);
    if (victim->nItems != 0)
        return PR_FAILURE;

    if (victim->lock)
        PR_DestroyMonitor(victim->lock);

    PR_Free(victim);

    return PR_SUCCESS;
}

static SSMListItem *
new_list_item(PRIntn priority, void *data)
{
    SSMListItem *item;

    item = PR_NEWZAP(SSMListItem);
    if (item == NULL)
        return NULL;

    item->priority = priority;
    item->data = data;
    return item;
}

static SSMListItem *
find_list_item(SSMCollection *list, void *which)
{
    PRCList *link;
  
    for(link = PR_LIST_HEAD(&list->list); link != &list->list;
        link = PR_NEXT_LINK(link)) 
    {

        SSMListItem *item = (SSMListItem *) link;
        if (item->data == which)
            return item;
    }
    return NULL;
}

/* Insert (item) into the list before (before). 
   If (before) is NULL, append to the list. */
SSMStatus
ssm_InsertSafe(SSMCollection *list, PRIntn priority, void *data, void *before)
{
    SSMListItem *beforeItem = NULL, *item;

    if ((data == NULL) || (list == NULL)) 
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return PR_FAILURE;
    }
  
    /* Find (before) in the list if it's here. */
    if (before == NULL) 
        beforeItem = (SSMListItem *) &list->list;
    else
        beforeItem = find_list_item(list, before);

    /* Create a new list item. */
    item = new_list_item(priority, data);
    if (item == NULL)
        return PR_FAILURE;

    PR_INSERT_BEFORE(&item->link, &beforeItem->link);
    list->nItems++;
    list->priorityCount[priority]++;
  
    return PR_SUCCESS;
}

/* Insert (data) into the list before (before). 
   If (before) is NULL, append to the list. */
SSMStatus
SSM_Insert(SSMCollection *list, PRIntn priority, void *data, void *before)
{
    SSMStatus rv;

    if (list == NULL) 
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return PR_FAILURE;
    }

    PR_EnterMonitor(list->lock);
    rv = ssm_InsertSafe(list, priority, data, before);
    PR_Notify(list->lock);
    PR_ExitMonitor(list->lock);

    return rv;
}

/* Remove (data) from the list. */
SSMStatus
ssm_RemoveSafe(SSMCollection *list, void *data)
{
    SSMStatus rv = PR_SUCCESS;
    SSMListItem *item;

    if ((data == NULL) || (list == NULL)) 
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return PR_FAILURE;
    }
  
    item = find_list_item(list, data);
    if (item == NULL) 
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return PR_FAILURE;
    }

    if ((item->priority >= 0) && (item->priority < SSM_PRIORITY_MAX))
        list->priorityCount[item->priority]--;

    PR_REMOVE_LINK(&item->link);
    PR_Free(item);
    list->nItems--;
  
    return rv;
}

/* Remove (item) from the list. */
SSMStatus
SSM_Remove(SSMCollection *list, void *item)
{
    SSMStatus rv;

    if (list == NULL) 
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return PR_FAILURE;
    }

    PR_EnterMonitor(list->lock);
    rv = ssm_RemoveSafe(list, item);
    PR_Notify(list->lock);
    PR_ExitMonitor(list->lock);

    return rv;
}

/* Count the number of items in the list. */
PRIntn
ssm_CountSafe(SSMCollection *list, PRIntn priority)
{
    PRIntn result = 0;
    PRIntn i;

    if (priority == SSM_PRIORITY_ANY)
        result = list->nItems;
    else
    {
        for(i=priority; i <= SSM_PRIORITY_MAX; i++)
            result += list->priorityCount[i];
    }

    return result;
}

/* Count the number of items in the list. */
PRIntn
SSM_CountPriority(SSMCollection *list, PRIntn priority)
{
    int count;

    if ((list == NULL) 
        || (priority < SSM_PRIORITY_ANY) 
        || (priority > SSM_PRIORITY_MAX)) 
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return -1;
    }

    PR_EnterMonitor(list->lock);
    count = ssm_CountSafe(list, priority);
    PR_ExitMonitor(list->lock);

    return count;
}

PRIntn
SSM_Count(SSMCollection *list)
{
    return SSM_CountPriority(list, SSM_PRIORITY_ANY);
}

/* Get (which)th item from the list. zero-based index. */
void *
SSM_At(SSMCollection *list, PRIntn which)
{
    PRCList *link;

    if ((list == NULL) || (which >= list->nItems)) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return NULL;
    }

    PR_EnterMonitor(list->lock);

    link = PR_LIST_HEAD(&list->list);
    for ( ; which > 0 ; which--) {
        link = PR_NEXT_LINK(link);
    }
	 
    PR_ExitMonitor(list->lock);

    return ((SSMListItem *) link)->data;
}


SSMStatus
SSM_Enqueue(SSMCollection *list, PRIntn priority, void *item)
{
    /* Insert this element at the end. */
    return SSM_Insert(list, priority, item, NULL);
}

SSMListItem *
ssm_FirstAtPriority(SSMCollection *list, PRIntn priority)
{
    SSMListItem *result;
    SSMListItem *link;

    result = NULL; /* in case we fail */

    if ((list == NULL) || (priority > SSM_PRIORITY_MAX)) 
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return NULL;
    }

    if (PR_CLIST_IS_EMPTY(&list->list))
        result = NULL;
    else if (priority == SSM_PRIORITY_ANY)
        result = (SSMListItem *) PR_LIST_HEAD(&list->list);
    else /* asked for priority and list not empty */
    {
        /* Find the first element from the list at (priority). */
        link = (SSMListItem *) PR_LIST_HEAD(&list->list);
        while((link != (SSMListItem *) &list->list) && 
              (link->priority < priority))
            link = (SSMListItem *) PR_NEXT_LINK(&link->link);

        if (link != (SSMListItem *) &list->list)
            result = link;
    }

    return result;
}

SSMStatus
SSM_Dequeue(SSMCollection *list, PRIntn priority, 
	    void **result, PRBool doBlock)
{
    void *data = NULL;
    SSMListItem *link;
	SSMStatus rv = PR_SUCCESS;

    *result = NULL; /* in case we fail */

    if (list == NULL) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return PR_FAILURE;
    }

    PR_EnterMonitor(list->lock);

    while(doBlock && (ssm_CountSafe(list, priority) == 0)) {
        rv = PR_Wait(list->lock, PR_INTERVAL_NO_TIMEOUT);
		if (rv == PR_PENDING_INTERRUPT_ERROR)
		{
			/* We got interrupted, bail */
			return rv;
		}
        /*        SSM_DEBUG("ssm_CountSafe (prio %d) is %d.\n", 
                  priority, ssm_CountSafe(list, priority));*/
    }

    /* Pop the first element from the list at (priority). */
    if (!PR_CLIST_IS_EMPTY(&list->list)) {
        link = ssm_FirstAtPriority(list, priority);
        if (link)
        {
            data = link->data;

            if ((link->priority >= 0) && (link->priority < SSM_PRIORITY_MAX))
                list->priorityCount[link->priority]--;
	    
            PR_REMOVE_LINK(&link->link);
            PR_Free(link);

            list->nItems--;
        }
    }

    PR_Notify(list->lock);
    PR_ExitMonitor(list->lock);

    *result = data;
    return PR_SUCCESS;
}

