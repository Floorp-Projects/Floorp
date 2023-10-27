/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsStreamLoader_h__
#define nsStreamLoader_h__

#include "nsIThreadRetargetableStreamListener.h"
#include "nsIStreamLoader.h"
#include "nsCOMPtr.h"
#include "mozilla/Attributes.h"
#include "mozilla/Vector.h"

class nsIRequest;

namespace mozilla {
namespace net {

class nsStreamLoader final : public nsIStreamLoader {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISTREAMLOADER
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSITHREADRETARGETABLESTREAMLISTENER

  nsStreamLoader();

  static nsresult Create(REFNSIID aIID, void** aResult);

 protected:
  ~nsStreamLoader() = default;

  static nsresult WriteSegmentFun(nsIInputStream*, void*, const char*, uint32_t,
                                  uint32_t, uint32_t*);

  // Utility method to free mData, if present, and update other state to
  // reflect that no data has been allocated.
  void ReleaseData();

  nsCOMPtr<nsIStreamLoaderObserver> mObserver;
  nsCOMPtr<nsISupports> mContext;  // the observer's context
  nsCOMPtr<nsIRequest> mRequest;
  nsCOMPtr<nsIRequestObserver> mRequestObserver;

  mozilla::Atomic<uint32_t, mozilla::MemoryOrdering::Relaxed> mBytesRead;

  // Buffer to accumulate incoming data. We preallocate if contentSize is
  // available.
  mozilla::Vector<uint8_t, 0> mData;
};

}  // namespace net
}  // namespace mozilla

#endif  // nsStreamLoader_h__
