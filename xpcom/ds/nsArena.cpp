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
#include "nsIArena.h"
#include "nsCRT.h"

#define PL_ARENA_CONST_ALIGN_MASK 7
#include "plarena.h"

static NS_DEFINE_IID(kArenaIID, NS_IARENA_IID);

// Simple arena implementation layered on plarena
class ArenaImpl : public nsIArena {
public:
  ArenaImpl(PRInt32 aBlockSize);

  NS_DECL_ISUPPORTS

  virtual void* Alloc(PRInt32 aSize);

protected:
  virtual ~ArenaImpl();

  PLArenaPool mPool;
  PRInt32 mBlockSize;
};

ArenaImpl::ArenaImpl(PRInt32 aBlockSize)
{
  NS_INIT_REFCNT();
  if (aBlockSize < NS_MIN_ARENA_BLOCK_SIZE) {
    aBlockSize = NS_DEFAULT_ARENA_BLOCK_SIZE;
  }
  PL_INIT_ARENA_POOL(&mPool, "nsIArena", aBlockSize);
  mBlockSize = aBlockSize;
}

NS_IMPL_ISUPPORTS(ArenaImpl,kArenaIID)

ArenaImpl::~ArenaImpl()
{
  PL_FinishArenaPool(&mPool);
}

void* ArenaImpl::Alloc(PRInt32 size)
{
  // Adjust size so that it's a multiple of sizeof(double)
  PRInt32 align = size & (sizeof(double) - 1);
  if (0 != align) {
    size += sizeof(double) - align;
  }

  void* p;
  PL_ARENA_ALLOCATE(p, &mPool, size);
  return p;
}

NS_BASE nsresult NS_NewHeapArena(nsIArena** aInstancePtrResult,
                                 PRInt32 aArenaBlockSize)
{
  ArenaImpl* it = new ArenaImpl(aArenaBlockSize);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kArenaIID, (void **) aInstancePtrResult);
}
