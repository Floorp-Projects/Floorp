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

private:
  PRBool mInitialized;
};

#endif // nsArena_h__
