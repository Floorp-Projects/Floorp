/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#ifndef nsIArena_h___
#define nsIArena_h___

#include "nscore.h"
#include "nsISupports.h"

#define NS_MIN_ARENA_BLOCK_SIZE 64
#define NS_DEFAULT_ARENA_BLOCK_SIZE 4096

/// Interface IID for nsIArena
#define NS_IARENA_IID         \
{ 0xa24fdad0, 0x93b4, 0x11d1, \
  {0x89, 0x5b, 0x00, 0x60, 0x08, 0x91, 0x1b, 0x81} }
#define NS_ARENA_CONTRACTID "@mozilla.org/arena;1"
#define NS_ARENA_CLASSNAME "Arena"

/** Interface to a memory arena abstraction. Arena's use large blocks
 * of memory to allocate smaller objects. Arena's provide no free
 * operator; instead, all of the objects in the arena are deallocated
 * by deallocating the arena (e.g. when it's reference count goes to
 * zero)
 */
class nsIArena : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IARENA_IID; return iid; }

  NS_IMETHOD Init(PRUint32 arenaBlockSize) = 0;

  NS_IMETHOD_(void*) Alloc(PRUint32 size) = 0;
};

/**
 * Create a new arena using the desired block size for allocating the
 * underlying memory blocks. The underlying memory blocks are allocated
 * using the PR heap.
 */
extern NS_COM nsresult NS_NewHeapArena(nsIArena** aInstancePtrResult,
                                        PRUint32 aArenaBlockSize = 0);

#define NS_ARENA_CID                                 \
{ /* 9832ec80-0d6b-11d3-9331-00104ba0fd40 */         \
    0x9832ec80,                                      \
    0x0d6b,                                          \
    0x11d3,                                          \
    {0x93, 0x31, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40} \
}

#endif /* nsIArena_h___ */


