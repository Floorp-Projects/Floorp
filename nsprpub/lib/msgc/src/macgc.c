/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Netscape Portable Runtime (NSPR).
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
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

#include "primpl.h"
#include "MacMemAllocator.h"

void _MD_InitGC() {}

void *_MD_GrowGCHeap(size_t *sizep)
{
	void			*heapPtr = NULL;
	size_t			heapSize = *sizep;
	
	// In previous versions of this code we tried to allocate GC heaps from the application
	// heap.  In the 4.0 application, we try to keep our app heap allications to a minimum
	// and instead go through our own memory allocation routines.
	heapPtr = malloc(heapSize);
	
	if (heapPtr == NULL) {		
		FreeMemoryStats		stats;
		
		memtotal(heapSize, &stats);		// How much can we allcoate?
		
		if (stats.maxBlockSize < heapSize)
			heapSize = stats.maxBlockSize;
		
		heapPtr = malloc(heapSize);
		
		if (heapPtr == NULL) 			// Now we're hurting
			heapSize = 0;
	}
	
	*sizep = heapSize;
	return heapPtr;
}


void _MD_FreeGCSegment(void *base, int32 /* len */)
{
	free(base);
}
