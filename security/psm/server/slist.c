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
#include "slist.h"


SSMSortedList *
SSMSortedList_New(SSMSortedListFn * functions) 
{
  SSMSortedList * list = NULL;
  
  if (!functions) 
    goto loser;
  
  list = PR_NEWZAP(SSMSortedList);
  if (list == NULL)
    goto loser;
  
  list->lock = PR_NewMonitor();
  if (list->lock == NULL)
    goto loser;
  
  list->func.keyCompare = functions->keyCompare;
  list->func.freeListItemData = functions->freeListItemData;
  list->func.freeListItemKey = functions->freeListItemKey;
  
  PR_INIT_CLIST(&list->list);
  list->nItems = 0;
  return list;
loser:
  if (list) {
    if (list->lock)
      PR_DestroyMonitor(list->lock);
    PR_Free(list);
  }
  return NULL;
}

SSMStatus 
SSMSortedList_Destroy(SSMSortedList *victim)
{
  PRCList * link, *next;
  
  if (!victim)
    goto done;
  
  PR_EnterMonitor(victim->lock);
  /* free elements */
  for(link = PR_LIST_HEAD(&victim->list); link != &victim->list;){
    next = PR_NEXT_LINK(link);
    slist_remove_item(victim, (SSMSortedListItem *)link, PR_TRUE);
    link = next;
  }

  PR_ExitMonitor(victim->lock);
  PR_ASSERT(victim->nItems == 0);
  PR_DestroyMonitor(victim->lock);  
  PR_Free(victim);
done:
  return SSM_SUCCESS;
}

SSMSortedListItem * slist_remove_item(SSMSortedList * list, 
				      SSMSortedListItem * item, 
				      PRBool doFree) 
{
  PR_REMOVE_LINK(&item->link);
  list->nItems--;
  if (doFree) {
    list->func.freeListItemData(item->data);
    list->func.freeListItemKey(item->key);
    PR_Free(item);
    return NULL;
  }
  else return item;
}

SSMStatus
SSMSortedList_Insert(SSMSortedList * slist, void * key, void * data)
{
  SSMSortedListItem * item, * nextItem;

  if (!slist)  
    goto loser;
  
  item = slist_allocate_item(key, data);
  if (!item) 
    goto loser;

  PR_EnterMonitor(slist->lock);
  nextItem = slist_find_next(slist, key);
  PR_INSERT_BEFORE(&item->link, &nextItem->link);
  slist->nItems++;
  PR_ExitMonitor(slist->lock);

  return SSM_SUCCESS;
loser:
  return SSM_FAILURE;
}

SSMSortedListItem * slist_allocate_item(void * key, void * data)
{
  SSMSortedListItem * item;

  item = PR_NEWZAP(SSMSortedListItem);
  if (item) 
    {
      item->data = data;
      item->key = key;
    }
  return item;
}
  
SSMStatus 
SSMSortedList_Find(SSMSortedList * slist, void * key, void ** data)
{
  SSMStatus rv = SSM_FAILURE;
  SSMSortedListItem * item; 

  if (!slist || !data)
    goto loser;

  *data = NULL;
  PR_EnterMonitor(slist->lock);
  item = slist_find_item(slist, key);
  if (item) {
    *data = item->data;
    rv = SSM_SUCCESS;
  }
  PR_ExitMonitor(slist->lock);
loser:
  return rv;
}

SSMStatus
SSMSortedList_Remove(SSMSortedList * slist, void * key, void ** data)
{
  SSMSortedListItem * item;
  SSMStatus rv = SSM_FAILURE;

  if (!slist) 
    goto loser;
  PR_EnterMonitor(slist->lock);
  item = slist_find_item(slist, key);
  if (item) {
    rv = SSM_SUCCESS;
    if (data)
      *data = item->data;
    slist_remove_item(slist, item, PR_FALSE); 
    slist->func.freeListItemKey(item->key);
    PR_Free(item);
  }
  PR_ExitMonitor(slist->lock);
loser:
  return rv;

}

SSMStatus 
SSMSortedList_FindNext(SSMSortedList * slist, void * key, void ** data)
{
  SSMSortedListItem * item;
  
  if (!slist || !data) 
    return SSM_FAILURE;
  
  PR_EnterMonitor(slist->lock);
  item = slist_find_next(slist, key);
  *data = item->data;
  PR_ExitMonitor(slist->lock);
  return SSM_SUCCESS;
}


SSMSortedListItem *
slist_find_next(SSMSortedList * slist, void * key)
{
  PRCList * link;
  SSMSortedListItem * item; 

  for(link = PR_LIST_HEAD(&slist->list); link != &slist->list;
	link = PR_NEXT_LINK(link))
    if (slist->func.keyCompare(((SSMSortedListItem *)link)->key, key) > 0) {
      /* insert here */ 
      item = (SSMSortedListItem *)link; 
      goto done;
    }
  item = (SSMSortedListItem *)&slist->list;
done:
  return item;
}

PRBool
SSMSortedList_Lookup(SSMSortedList * slist, void * key)
{
  SSMSortedListItem * item;
  if (!slist) 
    goto loser;
  PR_EnterMonitor(slist->lock);
  item = slist_find_item(slist, key);
  PR_ExitMonitor(slist->lock);
  if (item)
    return PR_TRUE;
loser:
  return PR_FALSE;
}

SSMSortedListItem *
slist_find_item(SSMSortedList * slist, void * key)
{
  PRCList * link;
  
  for(link = PR_LIST_HEAD(&slist->list); link != &slist->list;
      link = PR_NEXT_LINK(link))
    if (slist->func.keyCompare(((SSMSortedListItem *)link)->key, key) == 0) /* found it */ 
      return ((SSMSortedListItem *)link);
  return NULL;
}

PRIntn
SSMSortedList_Enumerate(SSMSortedList * slist, SSMSortedListEnumerator_fn func,
			void * arg)
{
  PRCList * link;
  PRIntn numentries = 0;
  SSMStatus rv;

  if (!slist || !func) 
    goto loser;

  PR_EnterMonitor(slist->lock);
  for(link = PR_LIST_HEAD(&slist->list); link != &slist->list;
      link = PR_NEXT_LINK(link)) {
    rv = func(numentries, arg, ((SSMSortedListItem *)link)->key, 
	      ((SSMSortedListItem *)link)->data); 
    if (rv == SSM_SUCCESS) 
      numentries ++;
  }
  PR_ExitMonitor(slist->lock);
  
loser:
  return numentries;
}
  







