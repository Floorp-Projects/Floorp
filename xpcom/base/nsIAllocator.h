/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#define NS_ALLOCATOR_CONTRACTID NS_MEMORY_CONTRACTID
#define NS_ALLOCATOR_CID        NS_MEMORY_CID

#endif /* nsIAllocator_h___ */
