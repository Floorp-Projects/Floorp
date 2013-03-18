/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsStreamLoader.h"
#include "nsIInputStream.h"
#include "nsIChannel.h"
#include "nsError.h"
#include "GeckoProfiler.h"

nsStreamLoader::nsStreamLoader()
  : mData(nullptr),
    mAllocated(0),
    mLength(0)
{
}

nsStreamLoader::~nsStreamLoader()
{
  if (mData) {
    NS_Free(mData);
  }
}

NS_IMETHODIMP
nsStreamLoader::Init(nsIStreamLoaderObserver* observer)
{
  NS_ENSURE_ARG_POINTER(observer);
  mObserver = observer;
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

NS_IMPL_ISUPPORTS3(nsStreamLoader, nsIStreamLoader,
                   nsIRequestObserver, nsIStreamListener)

NS_IMETHODIMP 
nsStreamLoader::GetNumBytesRead(uint32_t* aNumBytes)
{
  *aNumBytes = mLength;
  return NS_OK;
}

/* readonly attribute nsIRequest request; */
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
      if (contentLength > UINT32_MAX) {
        // Too big to fit into uint32, so let's bail.
        // XXX we should really make mAllocated and mLength 64-bit instead.
        return NS_ERROR_OUT_OF_MEMORY;
      }
      uint32_t contentLength32 = uint32_t(contentLength);
      // preallocate buffer
      mData = static_cast<uint8_t*>(NS_Alloc(contentLength32));
      if (!mData) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      mAllocated = contentLength32;
    }
  }
  mContext = ctxt;
  return NS_OK;
}

NS_IMETHODIMP 
nsStreamLoader::OnStopRequest(nsIRequest* request, nsISupports *ctxt,
                              nsresult aStatus)
{
  PROFILER_LABEL("network", "nsStreamLoader::OnStopRequest");
  if (mObserver) {
    // provide nsIStreamLoader::request during call to OnStreamComplete
    mRequest = request;
    nsresult rv = mObserver->OnStreamComplete(this, mContext, aStatus,
                                              mLength, mData);
    if (rv == NS_SUCCESS_ADOPTED_DATA) {
      // the observer now owns the data buffer, and the loader must
      // not deallocate it
      mData = nullptr;
      mLength = 0;
      mAllocated = 0;
    }
    // done.. cleanup
    mRequest = 0;
    mObserver = 0;
    mContext = 0;
  }
  return NS_OK;
}

NS_METHOD
nsStreamLoader::WriteSegmentFun(nsIInputStream *inStr,
                                void *closure,
                                const char *fromSegment,
                                uint32_t toOffset,
                                uint32_t count,
                                uint32_t *writeCount)
{
  nsStreamLoader *self = (nsStreamLoader *) closure;

  if (count > UINT32_MAX - self->mLength) {
    return NS_ERROR_ILLEGAL_VALUE; // is there a better error to use here?
  }

  if (self->mLength + count > self->mAllocated) {
    self->mData = static_cast<uint8_t*>(NS_Realloc(self->mData,
                                                   self->mLength + count));
    if (!self->mData) {
      self->mLength = 0;
      self->mAllocated = 0;
      return NS_ERROR_OUT_OF_MEMORY;
    }
    self->mAllocated = self->mLength + count;
  }

  ::memcpy(self->mData + self->mLength, fromSegment, count);
  self->mLength += count;

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
