/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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

#ifndef nsMemory_h__
#define nsMemory_h__

#include "nsIMemory.h"

/**
 * Static helper routines to manage memory. These routines allow easy access
 * to xpcom's built-in (global) nsIMemory implementation, without needing
 * to go through the service manager to get it. However this requires clients
 * to link with the xpcom DLL. 
 */
class nsMemory
{
public:
    static NS_EXPORT void*      Alloc(size_t size);
    static NS_EXPORT void*      Realloc(void* ptr, size_t size);
    static NS_EXPORT void       Free(void* ptr);
    static NS_EXPORT nsresult   HeapMinimize();
    static NS_EXPORT nsresult   RegisterObserver(nsIMemoryPressureObserver* obs);
    static NS_EXPORT nsresult   UnregisterObserver(nsIMemoryPressureObserver* obs);
    static NS_EXPORT void*      Clone(const void* ptr, size_t size);
    static NS_EXPORT nsIMemory* GetGlobalMemoryService();       // AddRefs
};

// ContractID/CID for the global memory service:
#define NS_MEMORY_CONTRACTID        "@mozilla.org/xpcom/memory-service;1"
#define NS_MEMORY_CLASSNAME     "Global Memory Service"
#define NS_MEMORY_CID                                \
{ /* 30a04e40-38e7-11d4-8cf5-0060b0fc14a3 */         \
    0x30a04e40,                                      \
    0x38e7,                                          \
    0x11d4,                                          \
    {0x8c, 0xf5, 0x00, 0x60, 0xb0, 0xfc, 0x14, 0xa3} \
}

#endif // nsMemory_h__
