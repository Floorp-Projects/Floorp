/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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


// wfList.cpp
// A doubly linked list manager
//
// Created by dp Suresh <dp@netscape.com>

#ifdef TEST
#include <stream.h>
#endif /* TEST */


#include <string.h>         // for NULL

#include "wfList.h"


//-------------//
// Constructor //
//-------------//

wfList::wfList(WF_FREE_FUNC freeFunc)
: head(NULL), tail(NULL), freeItemFunc(freeFunc)
{
}


//------------//
// Destructor //
//------------//

wfList::~wfList()
{
	removeAll();
}


//----------------------------------//
// Implementation of public methods //
//----------------------------------//


wfList::ERROR_CODE
wfList::add(void *item)
{
	struct wfListElement *ele = new (struct wfListElement);
	
	if (!ele) return (NO_MEMORY);
	
	ele->item = item;
	ele->next = NULL;
	ele->prev = NULL;
	
	ERROR_CODE ret = listAdd(ele);
	if (ret != SUCCESS)
	  {
		delete ele;
	  }
	
	return (ret);
}

wfList::ERROR_CODE
wfList::remove (void *item)
{
	struct wfListElement *ele = find(item);
	if (ele == NULL)
	  {
		return (NOT_FOUND);
	  }
	
	ERROR_CODE ret = listRemove(ele);
	if (ret == SUCCESS)
	  {
		listDeleteItem(ele->item);
		delete ele;
	  }
	return (ret);
}

wfList::ERROR_CODE
wfList::removeAll (void)
{
	ERROR_CODE ret = SUCCESS;
	while (head && ret == SUCCESS)
	  {
		wfListElement *ele = head;
		ERROR_CODE ret = listRemove(ele);
		if (ret != SUCCESS)
		  {
			break;
		  }
		listDeleteItem(ele->item);
		delete ele;
	  }
	return (ret);
}

wfList::ERROR_CODE
wfList::isEmpty()
{
	return ((head == NULL) ? WF_TRUE : WF_FALSE);
}

wfList::ERROR_CODE
wfList::isExist(void *item)
{
	wfListElement *tmp = find(item);
	return((tmp != NULL) ? WF_TRUE : WF_FALSE);
}

wfList::ERROR_CODE
wfList::iterate(IteratorFunction fn)
{
	struct wfListElement *tmp = head;
	if (!fn) return(GENERIC_ERROR);
	if (!tmp) return(GENERIC_ERROR);
	
	while (tmp)
	  {
		void *item = tmp->item;
		struct wfListElement *next = tmp->next;
		
		ERROR_CODE ret = (*fn)(item);
		switch (ret) {
		case GENERIC_ERROR:
		  return (ret);
		case SUCCESS:
		default:
		  break;
		}
		
		tmp = next;
	  }
	return (SUCCESS);
}

int
wfList::count(void)
{
	wfListElement *tmp = head;
	int n = 0;
    for (; tmp; tmp = tmp->next)
	  {
		n++;
	  }
	return (n);
}

//-----------------------------------//
// Implementation of Private methods //
//-----------------------------------//

wfList::ERROR_CODE
wfList::listAdd(struct wfListElement *ele)
{
	if (isEmpty() == WF_TRUE)
	  {
		// Empty list
		head = tail = ele;
	  }
	else
	  {
		// Add at the tail
		tail->next = ele;
		ele->prev = tail;
		tail = ele;
	  }
	return (SUCCESS);
}


wfList::ERROR_CODE
wfList::listRemove (struct wfListElement *ele)
{
	if (ele->prev)
	  {
		// Not the first element
		ele->prev->next = ele->next;
	  }
	else
	  {
		// First element
		head = ele->next;
	  }
	
	if (ele->next)
	  {
		// Not the last element
		ele->next->prev = ele->prev;
	  }
	else
	  {
		// Last element
		tail = ele->prev;
	  }
	return (SUCCESS);
}

void
wfList::listDeleteItem(void *item)
{
	if (freeItemFunc != NULL)
	{
		(*freeItemFunc)(this, item);
	}
	return;
}


struct wfListElement *
wfList::find(void *item)
{
	wfListElement *tmp = head;
	while (tmp && tmp->item != item) tmp = tmp->next;
	return (tmp);
}


#ifdef TEST
wfList::ERROR_CODE
print(void *item)
{
	cout << "--> " << (int)item << " ";
	return (wfList::SUCCESS);
}

main()
{
	wfList l;

	cout << "isEmpty() : " << l.isEmpty() << "\n";
	cout << "isExist(5): " << l.isExist((void *)5) << "\n";
	cout << "remove(5) : " << l.remove((void *)5) << "\n";
	cout << "count() : " << l.count() << "\n";
	l.iterate(print); cout << "\n";

	cout << "add(5) : " << l.add((void *)5) << "\n";
	cout << "add(6) : " << l.add((void *)6) << "\n";
	cout << "add(7) : " << l.add((void *)7) << "\n";
	cout << "add(8) : " << l.add((void *)8) << "\n";
	cout << "add(9) : " << l.add((void *)9) << "\n";
	cout << "isEmpty() : " << l.isEmpty() << "\n";
	cout << "count() : " << l.count() << "\n";

	l.iterate(print); cout << "\n";

	cout << "isExist(5): " << l.isExist((void *)5) << "\n";
	cout << "isExist(8): " << l.isExist((void *)8) << "\n";
	cout << "isExist(0xCAFE): " << l.isExist((void *)0xCAFE) << "\n";
	cout << "isExist(9): " << l.isExist((void *)9) << "\n";

	cout << "remove(5) : " << l.remove((void *)5) << "\n";
	l.iterate(print); cout << "\n";
	cout << "remove(8) : " << l.remove((void *)8) << "\n";
	l.iterate(print); cout << "\n";
	cout << "remove(0xCAFE) : " << l.remove((void *)0xCAFE) << "\n";
	l.iterate(print); cout << "\n";
	cout << "remove(9) : " << l.remove((void *)9) << "\n";
	cout << "count() : " << l.count() << "\n";
	l.iterate(print); cout << "\n";

	return(0);
}

#if 0
// The above test should yeild this result
isEmpty() : 1
isExist(5): 0
remove(5) : 4

add(5) : 2
add(6) : 2
add(7) : 2
add(8) : 2
add(9) : 2
isEmpty() : 0
--> 5 --> 6 --> 7 --> 8 --> 9 
isExist(5): 1
isExist(8): 1
isExist(0xCAFE): 0
isExist(9): 1
remove(5) : 2
--> 6 --> 7 --> 8 --> 9 
remove(8) : 2
--> 6 --> 7 --> 9 
remove(0xCAFE) : 4
--> 6 --> 7 --> 9 
remove(9) : 2
--> 6 --> 7 
wfList::delete (deleting 6...)
wfList::delete (deleting 7...)
#endif /* 0 */
#endif /* TEST */

