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

#ifndef nsArena_h__
#define nsArena_h__

#include "nsIArena.h"

#define PL_ARENA_CONST_ALIGN_MASK 7
#include "plarena.h"

// Simple arena implementation layered on plarena
class ArenaImpl : public nsIArena {
public:
  ArenaImpl(void);
  virtual ~ArenaImpl();

  NS_DECL_ISUPPORTS

  static NS_METHOD
  Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

  NS_IMETHOD Init(PRUint32 arenaBlockSize);

  NS_IMETHOD_(void*) Alloc(PRUint32 size);

protected:
  PLArenaPool mPool;
  PRUint32 mBlockSize;
};

#endif // nsArena_h__
