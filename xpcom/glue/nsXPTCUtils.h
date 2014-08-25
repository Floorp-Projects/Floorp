/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXPTCUtils_h__
#define nsXPTCUtils_h__

#include "xptcall.h"
#include "mozilla/MemoryReporting.h"

/**
 * A helper class that initializes an xptcall helper at construction
 * and releases it at destruction.
 */
class nsAutoXPTCStub : protected nsIXPTCProxy
{
public:
  nsISomeInterface* mXPTCStub;

protected:
  nsAutoXPTCStub() : mXPTCStub(nullptr) {}

  nsresult
  InitStub(const nsIID& aIID)
  {
    return NS_GetXPTCallStub(aIID, this, &mXPTCStub);
  }

  ~nsAutoXPTCStub()
  {
    if (mXPTCStub) {
      NS_DestroyXPTCallStub(mXPTCStub);
    }
  }

  size_t
  SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
  {
    return mXPTCStub ? NS_SizeOfIncludingThisXPTCallStub(mXPTCStub, aMallocSizeOf) : 0;
  }
};

#endif // nsXPTCUtils_h__
