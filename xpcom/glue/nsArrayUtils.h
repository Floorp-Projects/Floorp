/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsArrayUtils_h__
#define nsArrayUtils_h__

#include "nsCOMPtr.h"
#include "nsIArray.h"

// helper class for do_QueryElementAt
class NS_COM_GLUE nsQueryArrayElementAt : public nsCOMPtr_helper
  {
    public:
      nsQueryArrayElementAt(nsIArray* aArray, PRUint32 aIndex,
                            nsresult* aErrorPtr)
          : mArray(aArray),
            mIndex(aIndex),
            mErrorPtr(aErrorPtr)
        {
          // nothing else to do here
        }

      virtual nsresult NS_FASTCALL operator()(const nsIID& aIID, void**) const;

    private:
      nsIArray*  mArray;
      PRUint32   mIndex;
      nsresult*  mErrorPtr;
  };

inline
const nsQueryArrayElementAt
do_QueryElementAt(nsIArray* aArray, PRUint32 aIndex, nsresult* aErrorPtr = 0)
  {
    return nsQueryArrayElementAt(aArray, aIndex, aErrorPtr);
  }

#endif // nsArrayUtils_h__
