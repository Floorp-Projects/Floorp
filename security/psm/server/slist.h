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
#ifndef __SORTEDLIST_H__
#define __SORTEDLIST_H__

#include "prtypes.h"
#include "prmon.h"
#include "prclist.h"
#include "ssmerrs.h"
#include "ssmdefs.h"
#include "nspr.h"

struct _SSMSortedListItem;
typedef PRIntn (* SSMCompare_fn)(const void * p1, const void * p2);
typedef void (* SSMListFree_fn)(void * data);
typedef SSMStatus (* SSMSortedListEnumerator_fn)(PRIntn index, void * arg, 
						 void * key, void * data);


/* This is an ordered collection of items without repetition. */
/* This is to be used as a link in a PRCList.
 * You can cast any of the PRCList *'s in the list to one of these.
 *  See prclist.h for details.
 */
typedef struct _SSMSortedListItem
{
    PRCList link;
    void *key;
    void *data;
} SSMSortedListItem;

typedef struct _SSMSortedListFn
{
  SSMCompare_fn  keyCompare;
  SSMListFree_fn freeListItemData;
  SSMListFree_fn freeListItemKey;
} SSMSortedListFn;

/* An ordered collection, implemented in terms of PRCList. */
struct _SSMSortedList
{
  PRCList list;
  PRMonitor *lock;
  PRIntn nItems;
  SSMSortedListFn func;
};

 
typedef struct _SSMSortedList SSMSortedList;


SSMSortedList*
SSMSortedList_New(SSMSortedListFn * functions);
SSMStatus 
SSMSortedList_Destroy(SSMSortedList *victim);
SSMStatus
SSMSortedList_Insert(SSMSortedList * slist, void * key, void * data);
SSMStatus 
SSMSortedList_Find(SSMSortedList * slist, void * key, void ** data);
PRBool
SSMSortedList_Lookup(SSMSortedList * slist, void * key);
SSMStatus 
SSMSortedList_FindNext(SSMSortedList * slist, void * key, void ** data);
SSMStatus
SSMSortedList_Remove(SSMSortedList * slist, void * key, void ** data);
PRIntn
SSMSortedList_Enumerate(SSMSortedList * slist, SSMSortedListEnumerator_fn func,
			void * arg);

/* 
   SSMStatus
   SSMSortedList_AtSorted(SSMSortedList *list, PRIntn which, 
   void ** key, void **data);
   */

SSMSortedListItem * 
slist_remove_item(SSMSortedList * list, SSMSortedListItem * item, PRBool doFree);
SSMSortedListItem * 
slist_allocate_item(void * key, void * data);
SSMSortedListItem *
slist_find_next(SSMSortedList * slist, void * key);
SSMSortedListItem *
slist_find_item(SSMSortedList * slist, void * key);

#endif /* __SORTEDLIST_H__ */
