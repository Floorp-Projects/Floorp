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
