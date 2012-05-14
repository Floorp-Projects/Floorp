/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "primpl.h"

#if defined(_PR_PTHREADS)

/*
** The pthreads version doesn't use these functions.
*/
void _PR_InitSegs(void)
{
}

#else /* _PR_PTHREADS */

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

#endif /* _PR_PTHREADS */
