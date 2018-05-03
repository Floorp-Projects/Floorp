/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsStreamLoader.h"
#include "nsIInputStream.h"
#include "nsIChannel.h"
#include "nsError.h"
#include "GeckoProfiler.h"

#include <limits>

namespace mozilla {
namespace net {

nsStreamLoader::nsStreamLoader()
  : mData()
{
}

NS_IMETHODIMP
nsStreamLoader::Init(nsIStreamLoaderObserver* aStreamObserver,
                     nsIRequestObserver* aRequestObserver)
{
  NS_ENSURE_ARG_POINTER(aStreamObserver);
  mObserver = aStreamObserver;
  mRequestObserver = aRequestObserver;
  return NS_OK;
}

nsresult
nsStreamLoader::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  if (aOuter) return NS_ERROR_NO_AGGREGATION;

  nsStreamLoader* it = new nsStreamLoader();
  if (it == nullptr)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(it);
  nsresult rv = it->QueryInterface(aIID, aResult);
  NS_RELEASE(it);
  return rv;
}

NS_IMPL_ISUPPORTS(nsStreamLoader, nsIStreamLoader,
                  nsIRequestObserver, nsIStreamListener,
                  nsIThreadRetargetableStreamListener)

NS_IMETHODIMP
nsStreamLoader::GetNumBytesRead(uint32_t* aNumBytes)
{
  *aNumBytes = mData.length();
  return NS_OK;
}

NS_IMETHODIMP
nsStreamLoader::GetRequest(nsIRequest **aRequest)
{
  NS_IF_ADDREF(*aRequest = mRequest);
  return NS_OK;
}

NS_IMETHODIMP
nsStreamLoader::OnStartRequest(nsIRequest* request, nsISupports *ctxt)
{
  nsCOMPtr<nsIChannel> chan( do_QueryInterface(request) );
  if (chan) {
    int64_t contentLength = -1;
    chan->GetContentLength(&contentLength);
    if (contentLength >= 0) {
      // On 64bit platforms size of uint64_t coincides with the size of size_t,
      // so we want to compare with the minimum from size_t and int64_t.
      if (static_cast<uint64_t>(contentLength) >
          std::min(std::numeric_limits<size_t>::max(),
                   static_cast<size_t>(std::numeric_limits<int64_t>::max()))) {
        // Too big to fit into size_t, so let's bail.
        return NS_ERROR_OUT_OF_MEMORY;
      }
      // preallocate buffer
      if (!mData.initCapacity(contentLength)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
  }
  mContext = ctxt;
  if (mRequestObserver) {
    mRequestObserver->OnStartRequest(request, ctxt);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsStreamLoader::OnStopRequest(nsIRequest* request, nsISupports *ctxt,
                              nsresult aStatus)
{
  AUTO_PROFILER_LABEL("nsStreamLoader::OnStopRequest", NETWORK);

  if (mObserver) {
    // provide nsIStreamLoader::request during call to OnStreamComplete
    mRequest = request;
    size_t length = mData.length();
    uint8_t* elems = mData.extractOrCopyRawBuffer();
    nsresult rv = mObserver->OnStreamComplete(this, mContext, aStatus,
                                              length, elems);
    if (rv != NS_SUCCESS_ADOPTED_DATA) {
      // The observer didn't take ownership of the extracted data buffer, so
      // put it back into mData.
      mData.replaceRawBuffer(elems, length);
    }
    // done.. cleanup
    ReleaseData();
    mRequest = nullptr;
    mObserver = nullptr;
    mContext = nullptr;
  }

  if (mRequestObserver) {
    mRequestObserver->OnStopRequest(request, ctxt, aStatus);
    mRequestObserver = nullptr;
  }

  return NS_OK;
}

nsresult
nsStreamLoader::WriteSegmentFun(nsIInputStream *inStr,
                                void *closure,
                                const char *fromSegment,
                                uint32_t toOffset,
                                uint32_t count,
                                uint32_t *writeCount)
{
  nsStreamLoader *self = (nsStreamLoader *) closure;

  if (!self->mData.append(fromSegment, count)) {
    self->mData.clearAndFree();
    return NS_ERROR_OUT_OF_MEMORY;
  }

  *writeCount = count;

  return NS_OK;
}

NS_IMETHODIMP
nsStreamLoader::OnDataAvailable(nsIRequest* request, nsISupports *ctxt,
                                nsIInputStream *inStr,
                                uint64_t sourceOffset, uint32_t count)
{
  uint32_t countRead;
  return inStr->ReadSegments(WriteSegmentFun, this, count, &countRead);
}

void
nsStreamLoader::ReleaseData()
{
  mData.clearAndFree();
}

NS_IMETHODIMP
nsStreamLoader::CheckListenerChain()
{
  return NS_OK;
}

} // namespace net
} // namespace mozilla
