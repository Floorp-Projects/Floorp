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

#include "primpl.h"

void _PR_InitSegs(void)
{
	_PR_MD_INIT_SEGS();
}

/*
** Allocate a memory segment. The size value is rounded up to the native
** system page size and a page aligned portion of memory is returned.
** This memory is not part of the malloc heap. If "vaddr" is not NULL
** then PR tries to allocate the segment at the desired virtual address.
*/
PRSegment* _PR_NewSegment(PRUint32 size, void *vaddr)
{
    PRSegment *seg;

	/* calloc the data structure for the segment */
    seg = PR_NEWZAP(PRSegment);

    if (seg) {
	    size = ((size + _pr_pageSize - 1) >> _pr_pageShift) << _pr_pageShift;
		/*
		**	Now, allocate the actual segment memory (or map under some OS)
		**	The OS specific code decides from where or how to allocate memory.
		*/
	    if (_PR_MD_ALLOC_SEGMENT(seg, size, vaddr) != PR_SUCCESS) {
			PR_DELETE(seg);
			return NULL;
    	}
	}

    return seg;
}

/*
** Free a memory segment.
*/
void _PR_DestroySegment(PRSegment *seg)
{
	_PR_MD_FREE_SEGMENT(seg);
    PR_DELETE(seg);
}
