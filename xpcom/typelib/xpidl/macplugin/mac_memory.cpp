/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

const Boolean kTemporaryAllocation = false;

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
