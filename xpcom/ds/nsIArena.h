/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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

/** Interface to a memory arena abstraction. Arena's use large blocks
 * of memory to allocate smaller objects. Arena's provide no free
 * operator; instead, all of the objects in the arena are deallocated
 * by deallocating the arena (e.g. when it's reference count goes to
 * zero)
 */
class nsIArena : public nsISupports {
public:
  virtual void* Alloc(PRInt32 size) = 0;
};

/**
 * Create a new arena using the desired block size for allocating the
 * underlying memory blocks. The underlying memory blocks are allocated
 * using the PR heap.
 */
extern NS_BASE nsresult NS_NewHeapArena(nsIArena** aInstancePtrResult,
                                        PRInt32 aArenaBlockSize = 0);

#endif /* nsIArena_h___ */
