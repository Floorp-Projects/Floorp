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

#ifndef LINKLIST_H
#define LINKLIST_H

/*
 * linklist.h
 * @author derekt@netscape.com
 * @version 1.0
 */

typedef struct LinkedList
{
	void * pData;
	struct LinkedList * next;
} LinkedList_t;

int AddElement( LinkedList_t ** in_ppLinkedList, void * in_pData );
void RemoveAllElements( LinkedList_t ** in_ppLinkedList );
int size( LinkedList_t * in_pLinkedList );

#endif /* LINKLIST_H */
