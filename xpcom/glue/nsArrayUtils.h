/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsArrayUtils_h__
#define nsArrayUtils_h__

#include "nsCOMPtr.h"
#include "nsIArray.h"

// helper class for do_QueryElementAt
class nsQueryArrayElementAt : public nsCOMPtr_helper
{
public:
  nsQueryArrayElementAt(nsIArray* aArray, uint32_t aIndex,
                        nsresult* aErrorPtr)
    : mArray(aArray)
    , mIndex(aIndex)
    , mErrorPtr(aErrorPtr)
  {
  }

  virtual nsresult NS_FASTCALL operator()(const nsIID& aIID, void**) const;

private:
  nsIArray*  mArray;
  uint32_t   mIndex;
  nsresult*  mErrorPtr;
};

inline const nsQueryArrayElementAt
do_QueryElementAt(nsIArray* aArray, uint32_t aIndex, nsresult* aErrorPtr = 0)
{
  return nsQueryArrayElementAt(aArray, aIndex, aErrorPtr);
}

#endif // nsArrayUtils_h__
