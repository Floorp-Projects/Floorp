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

/**
 * XXX This file is obsolete. Use nsIMemory.idl or nsMemory.h instead.
 */

#ifndef nsIAllocator_h___
#define nsIAllocator_h___

#include "nsMemory.h"

#define nsIAllocator            nsIMemory
#define nsAllocator             nsMemory
#define GetGlobalAllocator      GetGlobalMemoryService

#define NS_IALLOCATOR_IID       NS_GET_IID(nsIMemory)
#define NS_ALLOCATOR_CONTRACTID     NS_MEMORY_CONTRACTID
#define NS_ALLOCATOR_CLASSNAME  NS_MEMORY_CLASSNAME
#define NS_ALLOCATOR_CID        NS_MEMORY_CID

#endif /* nsIAllocator_h___ */
