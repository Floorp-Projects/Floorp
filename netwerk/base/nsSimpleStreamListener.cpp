/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSimpleStreamListener.h"

namespace mozilla {
namespace net {

//
//----------------------------------------------------------------------------
// nsISupports implementation...
//----------------------------------------------------------------------------
//
NS_IMPL_ISUPPORTS(nsSimpleStreamListener, nsISimpleStreamListener,
                  nsIStreamListener, nsIRequestObserver)

//
//----------------------------------------------------------------------------
// nsIRequestObserver implementation...
//----------------------------------------------------------------------------
//
NS_IMETHODIMP
nsSimpleStreamListener::OnStartRequest(nsIRequest* aRequest) {
  return mObserver ? mObserver->OnStartRequest(aRequest) : NS_OK;
}

NS_IMETHODIMP
nsSimpleStreamListener::OnStopRequest(nsIRequest* request, nsresult aStatus) {
  return mObserver ? mObserver->OnStopRequest(request, aStatus) : NS_OK;
}

//
//----------------------------------------------------------------------------
// nsIStreamListener implementation...
//----------------------------------------------------------------------------
//
NS_IMETHODIMP
nsSimpleStreamListener::OnDataAvailable(nsIRequest* request,
                                        nsIInputStream* aSource,
                                        uint64_t aOffset, uint32_t aCount) {
  uint32_t writeCount;
  nsresult rv = mSink->WriteFrom(aSource, aCount, &writeCount);
  //
  // Equate zero bytes read and NS_SUCCEEDED to stopping the read.
  //
  if (NS_SUCCEEDED(rv) && (writeCount == 0)) return NS_BASE_STREAM_CLOSED;
  return rv;
}

//
//----------------------------------------------------------------------------
// nsISimpleStreamListener implementation...
//----------------------------------------------------------------------------
//
NS_IMETHODIMP
nsSimpleStreamListener::Init(nsIOutputStream* aSink,
                             nsIRequestObserver* aObserver) {
  MOZ_ASSERT(aSink, "null output stream");

  mSink = aSink;
  mObserver = aObserver;

  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
