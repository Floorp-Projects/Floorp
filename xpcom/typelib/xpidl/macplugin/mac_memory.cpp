/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/*
	mac_memory.cpp
 */

#include <new.h>
#include <stdlib.h>

#include <Files.h>
#include <Memory.h>

#include "DropInCompilerLinker.h"
#include "CompilerMapping.h"
#include "CWPluginErrors.h"

extern CWPluginContext gPluginContext;

/**
 * Note: memory allocated by these operators will automatically be freed after the
 * current call into xpidl_compiler completes. This should be fine in most cases,
 * as we are also having the compiler be reloaded for every request to reinitialize
 * global data. Just be careful out there!
 */

const bool kTemporaryAllocation = false;

void* operator new(size_t size)
{
	void* ptr = NULL;
	if (CWAllocateMemory(gPluginContext, size, kTemporaryAllocation, &ptr) == cwNoErr)
		return ptr;
	return NULL;
}

void operator delete(void* ptr)
{
	if (ptr != NULL)
		CWFreeMemory(gPluginContext, ptr, kTemporaryAllocation);
}

void* operator new[] (size_t size)
{
	void* ptr = NULL;
	if (CWAllocateMemory(gPluginContext, size, kTemporaryAllocation, &ptr) == cwNoErr)
		return ptr;
	return NULL;
}

void operator delete[](void* ptr)
{
	if (ptr != NULL)
		CWFreeMemory(gPluginContext, ptr, kTemporaryAllocation);
}

namespace std {

#define TRACK_ALLOCATION
#define kTrackedCookie 'TRKD'

void* malloc(size_t size)
{
#if defined(TRACK_ALLOCATION)
	OSType* ptr = (OSType*) new char[sizeof(OSType) + size];
	if (ptr != NULL)
		*ptr++ = kTrackedCookie;
	return ptr;
#else
	return new char[size];
#endif
}

void free(void *ptr)
{
#if defined(TRACK_ALLOCATION)
	OSType* type = (OSType*)ptr;
	if (*--type == kTrackedCookie)
		delete[] (char*) type;
	else
		DebugStr("\pillegal block passed to free.");
#else
	delete[] (char*) ptr;
#endif
}

void* calloc(size_t nmemb, size_t size)
{
	size *= nmemb;
	void* ptr = malloc(size);
	if (ptr != NULL) {
		BlockZero(ptr, size);
	}
	return ptr;
}

void* realloc(void * ptr, size_t size)
{
	void* newptr = NULL;

	if (size > 0)
		newptr = malloc(size);

	if (ptr != NULL && newptr != NULL)
		BlockMoveData(ptr, newptr, size);

	if (ptr != NULL)
		free(ptr);

	return newptr;
}

}
