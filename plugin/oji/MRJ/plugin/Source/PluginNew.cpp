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

/*
	PluginNew.cpp
	
	new & delete operators for plugins.
	
	by Patrick C. Beard.
 */

#include <new.h>

#include "jni.h"
#include "nsIMalloc.h"

// Warning:  this forces all C++ allocation to go through Navigator's memory allocation
// Routines. As such, static constructors that use operator new may not work. This can
// be fixed if we delay static construction (see the call to __InitCode__() in npmac.cpp).

extern nsIMalloc* theMemoryAllocator;

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
