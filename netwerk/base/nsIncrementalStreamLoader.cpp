/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIncrementalStreamLoader.h"
#include "nsIInputStream.h"
#include "nsIChannel.h"
#include "nsError.h"
#include "GeckoProfiler.h"

#include <limits>

nsIncrementalStreamLoader::nsIncrementalStreamLoader()
  : mData(), mBytesConsumed(0)
{
}

nsIncrementalStreamLoader::~nsIncrementalStreamLoader()
{
}

NS_IMETHODIMP
nsIncrementalStreamLoader::Init(nsIIncrementalStreamLoaderObserver* observer)
{
  NS_ENSURE_ARG_POINTER(observer);
  mObserver = observer;
  return NS_OK;
}

nsresult
nsIncrementalStreamLoader::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  if (aOuter) return NS_ERROR_NO_AGGREGATION;

  nsIncrementalStreamLoader* it = new nsIncrementalStreamLoader();
  if (it == nullptr)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(it);
  nsresult rv = it->QueryInterface(aIID, aResult);
  NS_RELEASE(it);
  return rv;
}

NS_IMPL_ISUPPORTS(nsIncrementalStreamLoader, nsIIncrementalStreamLoader,
                  nsIRequestObserver, nsIStreamListener,
                  nsIThreadRetargetableStreamListener)

NS_IMETHODIMP
nsIncrementalStreamLoader::GetNumBytesRead(uint32_t* aNumBytes)
{
  *aNumBytes = mBytesConsumed + mData.length();
  return NS_OK;
}

/* readonly attribute nsIRequest request; */
NS_IMETHODIMP
nsIncrementalStreamLoader::GetRequest(nsIRequest **aRequest)
{
  NS_IF_ADDREF(*aRequest = mRequest);
  return NS_OK;
}

NS_IMETHODIMP
nsIncrementalStreamLoader::OnStartRequest(nsIRequest* request, nsISupports *ctxt)
{
  nsCOMPtr<nsIChannel> chan( do_QueryInterface(request) );
  if (chan) {
    int64_t contentLength = -1;
    chan->GetContentLength(&contentLength);
    if (contentLength >= 0) {
      if (uint64_t(contentLength) > std::numeric_limits<size_t>::max()) {
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
  return NS_OK;
}

NS_IMETHODIMP
nsIncrementalStreamLoader::OnStopRequest(nsIRequest* request, nsISupports *ctxt,
                                         nsresult aStatus)
{
  PROFILER_LABEL("nsIncrementalStreamLoader", "OnStopRequest",
    js::ProfileEntry::Category::NETWORK);

  if (mObserver) {
    // provide nsIIncrementalStreamLoader::request during call to OnStreamComplete
    mRequest = request;
    size_t length = mData.length();
    uint8_t* elems = mData.extractRawBuffer();
    nsresult rv = mObserver->OnStreamComplete(this, mContext, aStatus,
                                              length, elems);
    if (rv != NS_SUCCESS_ADOPTED_DATA) {
      // The observer didn't take ownership of the extracted data buffer, so
      // put it back into mData.
      mData.replaceRawBuffer(elems, length);
    }
    // done.. cleanup
    ReleaseData();
    mRequest = 0;
    mObserver = 0;
    mContext = 0;
  }
  return NS_OK;
}

NS_METHOD
nsIncrementalStreamLoader::WriteSegmentFun(nsIInputStream *inStr,
                                           void *closure,
                                           const char *fromSegment,
                                           uint32_t toOffset,
                                           uint32_t count,
                                           uint32_t *writeCount)
{
  nsIncrementalStreamLoader *self = (nsIncrementalStreamLoader *) closure;

  const uint8_t *data = reinterpret_cast<const uint8_t *>(fromSegment);
  uint32_t consumedCount = 0;
  nsresult rv;
  if (self->mData.empty()) {
    // Shortcut when observer wants to keep the listener's buffer empty.
    rv = self->mObserver->OnIncrementalData(self, self->mContext,
                                            count, data, &consumedCount);

    if (rv != NS_OK) {
      return rv;
    }

    if (consumedCount > count) {
      return NS_ERROR_INVALID_ARG;
    }

    if (consumedCount < count) {
      if (!self->mData.append(fromSegment + consumedCount,
                              count - consumedCount)) {
        self->mData.clearAndFree();
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
  } else {
    // We have some non-consumed data from previous OnIncrementalData call,
    // appending new data and reporting combined data.
    if (!self->mData.append(fromSegment, count)) {
      self->mData.clearAndFree();
      return NS_ERROR_OUT_OF_MEMORY;
    }
    size_t length = self->mData.length();
    uint32_t reportCount = length > UINT32_MAX ? UINT32_MAX : (uint32_t)length;
    uint8_t* elems = self->mData.extractRawBuffer();

    rv = self->mObserver->OnIncrementalData(self, self->mContext,
                                            reportCount, elems, &consumedCount);

    // We still own elems, freeing its memory when exiting scope.
    if (rv != NS_OK) {
      free(elems);
      return rv;
    }

    if (consumedCount > reportCount) {
      free(elems);
      return NS_ERROR_INVALID_ARG;
    }

    if (consumedCount == length) {
      free(elems); // good case -- fully consumed data
    } else {
      // Adopting elems back (at least its portion).
      self->mData.replaceRawBuffer(elems, length);
      if (consumedCount > 0) {
        self->mData.erase(self->mData.begin() + consumedCount);
      }
    }
  }

  self->mBytesConsumed += consumedCount;
  *writeCount = count;

  return NS_OK;
}

NS_IMETHODIMP
nsIncrementalStreamLoader::OnDataAvailable(nsIRequest* request, nsISupports *ctxt,
                                nsIInputStream *inStr,
                                uint64_t sourceOffset, uint32_t count)
{
  if (mObserver) {
    // provide nsIIncrementalStreamLoader::request during call to OnStreamComplete
    mRequest = request;
  }
  uint32_t countRead;
  nsresult rv = inStr->ReadSegments(WriteSegmentFun, this, count, &countRead);
  mRequest = 0;
  return rv;
}

void
nsIncrementalStreamLoader::ReleaseData()
{
  mData.clearAndFree();
}

NS_IMETHODIMP
nsIncrementalStreamLoader::CheckListenerChain()
{
  return NS_OK;
}
