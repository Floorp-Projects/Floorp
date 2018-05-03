/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIncrementalStreamLoader_h__
#define nsIncrementalStreamLoader_h__

#include "nsIThreadRetargetableStreamListener.h"
#include "nsIIncrementalStreamLoader.h"
#include "nsCOMPtr.h"
#include "mozilla/Attributes.h"
#include "mozilla/Vector.h"

class nsIRequest;

class nsIncrementalStreamLoader final : public nsIIncrementalStreamLoader
                                      , public nsIThreadRetargetableStreamListener
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINCREMENTALSTREAMLOADER
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSITHREADRETARGETABLESTREAMLISTENER

  nsIncrementalStreamLoader();

  static nsresult
  Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

protected:
  ~nsIncrementalStreamLoader() = default;

  static nsresult WriteSegmentFun(nsIInputStream *, void *, const char *,
                                  uint32_t, uint32_t, uint32_t *);

  // Utility method to free mData, if present, and update other state to
  // reflect that no data has been allocated.
  void ReleaseData();

  nsCOMPtr<nsIIncrementalStreamLoaderObserver> mObserver;
  nsCOMPtr<nsISupports>             mContext;  // the observer's context
  nsCOMPtr<nsIRequest>              mRequest;

  // Buffer to accumulate incoming data. We preallocate if contentSize is
  // available.
  mozilla::Vector<uint8_t, 0> mData;

  // Number of consumed bytes from the mData.
  size_t  mBytesConsumed;
};

#endif // nsIncrementalStreamLoader_h__
