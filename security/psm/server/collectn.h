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
#ifndef __COLLECTN_H__
#define __COLLECTN_H__

#include "prtypes.h"
#include "prmon.h"
#include "prclist.h"
#include "ssmdefs.h"

/* definitions for priority */
#define SSM_PRIORITY_ANY -1
#define SSM_PRIORITY_NORMAL 0
#define SSM_PRIORITY_UI 1
#define SSM_PRIORITY_SHUTDOWN 30 /* always pull this if it's available */
#define SSM_PRIORITY_MAX 31

/* This is an ordered collection of items without repetition. */
/* This is to be used as a link in a PRCList.
 * You can cast any of the PRCList *'s in the list to one of these.
 *  See prclist.h for details.
 */
typedef struct _SSMListItem
{
    PRCList link;
    PRIntn  priority;
    void *data;
} SSMListItem;

/* An ordered collection, implemented in terms of PRCList. */
struct _SSMCollection
{
    PRMonitor *lock;
    PRIntn nItems;
    PRIntn priorityCount[SSM_PRIORITY_MAX+1];
    PRCList list;
};

typedef struct _SSMCollection SSMCollection;

/************************************************************
** FUNCTION: SSM_NewCollection
** RETURNS:
**   SSMCollection *
**     returns a pointer to a new SSMCollection.
**     return NULL on error.
**
*************************************************************/
SSMCollection *SSM_NewCollection(void);


/************************************************************
** FUNCTION: SSM_DestroyCollection
** INPUTS:
**   victim
**     The collection to be destroyed.
** RETURNS:
**   PR_SUCCESS on success.
**   PR_FAILURE on failure.  This can't happen (yet).
**
*************************************************************/
SSMStatus SSM_DestroyCollection(SSMCollection *victim);


/************************************************************
** FUNCTION: SSM_Enqueue
** DESCRIPTION: Append (item) to (list).
** INPUTS:
**   list
**     The collection to be added to.
**   priority
**     The priority of the new item in the queue.
**   item
**     The data item to be enqueued.
** RETURNS:
**   PR_SUCCESS on success.
**   PR_FAILURE on failure.
**
*************************************************************/
SSMStatus SSM_Enqueue(SSMCollection *list, PRIntn priority, void *item);


/************************************************************
** FUNCTION: SSM_Dequeue
** DESCRIPTION: Retrieve a data item from the head of (list).
** INPUTS:
**   list
**     The collection to be retrieved from.
**   priority
**     The priority level of items to be removed from the queue. 
**     Anything below the given priority level is ignored. 
**     Pass SSM_PRIORITY_ANY to fetch the first item from the 
**     queue regardless of priority.
**   doBlock
**     PR_TRUE if the function should block on an empty collection.
**     PR_FALSE if the function should not block.
** OUTPUTS:
**   item
**     A pointer to the void * to hold the retrieved item.
** RETURNS:
**   PR_SUCCESS on success.
**   PR_FAILURE on failure.
**
*************************************************************/
SSMStatus SSM_Dequeue(SSMCollection *list, PRIntn priority, void **item, PRBool doBlock);


/************************************************************
** FUNCTION: SSM_Insert
** DESCRIPTION: Insert a new data item into (list) before (before).
** INPUTS:
**   list
**     The collection to be retrieved from.
**   priority
**     The priority of the item to be inserted.
**   item
**     The data item to insert.
**   before
**     The list member before which to insert.
** RETURNS:
**   PR_SUCCESS on success.
**   PR_FAILURE on failure.
**
*************************************************************/
SSMStatus SSM_Insert(SSMCollection *list, PRIntn priority, 
		    void *item, void *before);


/************************************************************
** FUNCTION: SSM_Remove
** DESCRIPTION: Remove (item) from (list).
** INPUTS:
**   list
**     The collection to be removed from.
**   item
**     The data item to be removed.
** RETURNS:
**   PR_SUCCESS on success.
**   PR_FAILURE on failure.
**
*************************************************************/
SSMStatus SSM_Remove(SSMCollection *list, void *item);

/************************************************************
** FUNCTION: SSM_Count
** DESCRIPTION: Returns the number of items in (list).
** INPUTS:
**   list
**     The collection to be counted.
**   priority
**     Only count items at or above (priority). Pass SSM_PRIORITY_ANY
**     to count all items in the list.
** RETURNS:
**   The number of items in the list.
**
*************************************************************/
PRIntn SSM_Count(SSMCollection *list);
PRIntn SSM_CountPriority(SSMCollection *list, PRIntn priority);


/************************************************************
** FUNCTION: SSM_At
** DESCRIPTION: Get the (which)'th item in (list).
** INPUTS:
**   list
**     The collection to be indexed.
**   which
**     Index of item to retrieve.  Zero is first item.
**
*************************************************************/
void *SSM_At(SSMCollection *list, PRIntn which);

#endif /* __COLLECTN_H__ */
