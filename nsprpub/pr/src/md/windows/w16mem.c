/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 * 
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*******************************************************************
** w16mem.c -- Implement memory segment functions.
**
** 
********************************************************************
*/
#include "primpl.h"


/*
**	Allocate a new memory segment.
**	
**	Return the segment's access rights and size.  
*/
PRStatus _MD_AllocSegment(PRSegment *seg, PRUint32 size, void *vaddr)
{
	PR_ASSERT(seg != 0);
	PR_ASSERT(size != 0);
	PR_ASSERT(vaddr == 0);

	/*	
	** Take the actual memory for the segment out of our Figment heap.
	*/

	seg->vaddr = (char *)malloc(size);

	if (seg->vaddr == NULL) {
		return PR_FAILURE;
	}

	seg->size = size;	

	return PR_SUCCESS;
} /* --- end _MD_AllocSegment() --- */


/*
**	Free previously allocated memory segment.
*/
void _MD_FreeSegment(PRSegment *seg)
{
	PR_ASSERT((seg->flags & _PR_SEG_VM) == 0);

	if (seg->vaddr != NULL)
		free( seg->vaddr );
    return;
} /* --- end _MD_FreeSegment() --- */
