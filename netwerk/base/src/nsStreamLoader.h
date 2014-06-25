/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsStreamLoader_h__
#define nsStreamLoader_h__

#include "nsIStreamLoader.h"
#include "nsCOMPtr.h"
#include "mozilla/Attributes.h"

class nsIRequest;

class nsStreamLoader MOZ_FINAL : public nsIStreamLoader
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLOADER
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER

  nsStreamLoader();

  static nsresult
  Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

protected:
  ~nsStreamLoader();

  static NS_METHOD WriteSegmentFun(nsIInputStream *, void *, const char *,
                                   uint32_t, uint32_t, uint32_t *);

  // Utility method to free mData, if present, and update other state to
  // reflect that no data has been allocated.
  void ReleaseData();

  nsCOMPtr<nsIStreamLoaderObserver> mObserver;
  nsCOMPtr<nsISupports>             mContext;  // the observer's context
  nsCOMPtr<nsIRequest>              mRequest;

  uint8_t  *mData;      // buffer to accumulate incoming data
  uint32_t  mAllocated; // allocated size of data buffer (we preallocate if
                        //   contentSize is available)
  uint32_t  mLength;    // actual length of data in buffer
                        //   (must be <= mAllocated)
};

#endif // nsStreamLoader_h__
