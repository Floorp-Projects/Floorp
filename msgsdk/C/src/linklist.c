/*
 *	 The contents of this file are subject to the Netscape Public
 *	 License Version 1.1 (the "License"); you may not use this file
 *	 except in compliance with the License. You may obtain a copy of
 *	 the License at http://www.mozilla.org/NPL/
 *	
 *	 Software distributed under the License is distributed on an "AS
 *	 IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 *	 implied. See the License for the specific language governing
 *	 rights and limitations under the License.
 *	
 *	 The Original Code is the Netscape Messaging Access SDK Version 3.5 code, 
 *	released on or about June 15, 1998.  *	
 *	 The Initial Developer of the Original Code is Netscape Communications 
 *	 Corporation. Portions created by Netscape are
 *	 Copyright (C) 1998 Netscape Communications Corporation. All
 *	 Rights Reserved.
 *	
 *	 Contributor(s): ______________________________________. 
*/
  
/* 
 * Copyright (c) 1997 and 1998 Netscape Communications Corporation
 * (http://home.netscape.com/misc/trademarks.html) 
 */

/*
 * linklist.c
 * @author derekt@netscape.com
 * @version 1.0
 */

#include "nsmail.h"
#include "linklist.h"

#include <malloc.h>
#include <string.h>


int AddElement( LinkedList_t ** in_ppLinkedList, void * in_pData )
{
	LinkedList_t * l_node;
	LinkedList_t * l_temp;

	if ( in_pData == NULL || in_ppLinkedList == NULL )
	{
		return NSMAIL_ERR_INVALIDPARAM;
	}

	l_node = (LinkedList_t*)malloc(  sizeof( LinkedList_t ) );

    if ( l_node == NULL )
    {
        return NSMAIL_ERR_OUTOFMEMORY;
    }

	l_node->next = NULL;

	l_node->pData = in_pData;

	if ( *in_ppLinkedList == NULL )
	{
		*in_ppLinkedList = l_node;
	}
	else
	{
		l_temp = *in_ppLinkedList;

		while ( l_temp->next != NULL )
		{
			l_temp = l_temp->next;
		}

		l_temp->next = l_node;
	}

	return NSMAIL_OK;
}

void RemoveAllElements( LinkedList_t ** in_ppLinkedList )
{
	LinkedList_t * l_prevListPtr = *in_ppLinkedList;
	LinkedList_t * l_currListPtr = l_prevListPtr;

	while ( l_currListPtr != NULL )
	{
		l_currListPtr = l_currListPtr->next;

        free( l_prevListPtr );

		l_prevListPtr = l_currListPtr;
	}

	*in_ppLinkedList = NULL;
}

int size( LinkedList_t * in_ppLinkedList )
{
	int l_nCount = 0;

	while ( in_ppLinkedList != NULL )
	{
		l_nCount++;
		in_ppLinkedList = in_ppLinkedList->next;
	}

	return l_nCount;
}

