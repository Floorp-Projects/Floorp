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
	PluginNew.cpp
	
	new & delete operators for plugins.
	
	by Patrick C. Beard.
 */

#include <new.h>

#include "jni.h"
#include "nsIAllocator.h"

// Warning:  this forces all C++ allocation to go through Navigator's memory allocation
// Routines. As such, static constructors that use operator new may not work. This can
// be fixed if we delay static construction (see the call to __InitCode__() in npmac.cpp).

extern nsIAllocator* theMemoryAllocator;

void* operator new(size_t size)
{
	if (theMemoryAllocator != NULL)
		return theMemoryAllocator->Alloc(size);
	return NULL;
}

void operator delete(void* ptr)
{
	if (ptr != NULL && theMemoryAllocator != NULL)
		theMemoryAllocator->Free(ptr);
}
