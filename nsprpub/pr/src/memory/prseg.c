/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
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
