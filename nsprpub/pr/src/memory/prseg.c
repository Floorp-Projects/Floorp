/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
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
PR_IMPLEMENT(PRSegment*) PR_NewSegment(PRUint32 size, void *vaddr)
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
PR_IMPLEMENT(void) PR_DestroySegment(PRSegment *seg)
{
	_PR_MD_FREE_SEGMENT(seg);
    PR_DELETE(seg);
}

/* XXX Fix the following to make machine-independent */
#ifdef XP_UNIX
#include <sys/mman.h>
#endif

#ifndef PROT_NONE
#define PROT_NONE 0
#endif

extern	PRInt32 _pr_zero_fd;

/*
** Attempt to grow/shrink the given segment.
*/
PR_IMPLEMENT(PRUint32) PR_GrowSegment(PRSegment *seg, PRInt32 delta)
{
    char *oldend, *newend;
#if 0
#ifdef XP_UNIX
    int prot;
    void *rv = (void *) -1;
#endif
#endif /* 0 */

    if (!(seg->flags & _PR_SEG_VM)) {
        return 0;
    }

    oldend = (char*)seg->vaddr + seg->size;
    if (delta > 0) {
#if 0

		/*
		 * CANNOT use MAP_FIXED because it will replace any existing mappings
		 */
        /* Growing the segment */
        delta = ((delta + _pr_pageSize - 1) >> _pr_pageShift) << _pr_pageShift;
        newend = oldend + delta;
#ifdef XP_UNIX
        prot = PROT_READ|PROT_WRITE;
        rv = mmap(oldend, delta, prot,
#ifdef OSF1
                  /* XXX need to pick a vaddr to use */
                  MAP_PRIVATE|MAP_FIXED,
#else
                  MAP_SHARED|MAP_FIXED,
#endif
                  _pr_zero_fd, 0);
#endif	/* XP_UNIX */
        if (rv == (void*)-1) {
            /* Can't extend the heap */
            return 0;
        }
        seg->size = seg->size + delta;
#endif /* 0 */
    } else if (delta < 0) {
        /* Shrinking the segment */
        delta = -delta;
        delta = (delta >> _pr_pageShift) << _pr_pageShift;      /* trunc */
        if ((PRUint32)delta >= seg->size) {
            /* XXX what to do? */
            return 0;
        }
        newend = oldend - delta;
        if (newend < oldend) {
#ifdef XP_UNIX
            (void) munmap(oldend, newend - oldend);
            seg->size = seg->size + delta;
#endif
        }
    }
    return delta;
}

/*
** Change the mapping on a segment.
** 	"how" == PR_SEGMENT_NONE: the segment becomes unmapped
** 	"how" == PR_SEGMENT_RDONLY: the segment becomes mapped and readable
** 	"how" == PR_SEGMENT_RDWR: the segment becomes mapped read/write
**
** If a segment can be read then it is also possible to execute code in
** it.
*/
PR_IMPLEMENT(void) PR_MapSegment(PRSegment *seg, PRSegmentAccess how)
{
    if (seg->access == how) return;
    seg->access = how;

#ifdef XP_UNIX
#ifndef RHAPSODY
    if (seg->flags & _PR_SEG_VM) {
	int prot;
	switch (how) {
	  case PR_SEGMENT_NONE:
	    prot = PROT_NONE;
	    break;
	  case PR_SEGMENT_RDONLY:
	    prot = PROT_READ;
	    break;
	  case PR_SEGMENT_RDWR:
	    prot = PROT_READ|PROT_WRITE;
	    break;
	}
	(void) mprotect(seg->vaddr, seg->size, prot);
    }
#endif
#endif
}

/*
** Return the size of the segment
*/
PR_IMPLEMENT(size_t) PR_GetSegmentSize(PRSegment *seg)
{
    return seg->size;
}

/*
** Return the virtual address of the segment
*/
PR_IMPLEMENT(void*) PR_GetSegmentVaddr(PRSegment *seg)
{
    return seg->vaddr;
}

/*
** Return a segments current access rights
*/
PR_IMPLEMENT(PRSegmentAccess) PR_GetSegmentAccess(PRSegment *seg)
{
    return seg->access;
}
