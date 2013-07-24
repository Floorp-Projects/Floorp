/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPreloadedStream.h"
#include "nsIRunnable.h"

#include "nsThreadUtils.h"
#include "nsAlgorithm.h"
#include <algorithm>
   
namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS2(nsPreloadedStream,
                   nsIInputStream,
                   nsIAsyncInputStream)

nsPreloadedStream::nsPreloadedStream(nsIAsyncInputStream *aStream,
                                     const char *data, uint32_t datalen)
    : mStream(aStream),
      mOffset(0),
      mLen(datalen)
{
    mBuf = (char *) moz_xmalloc(datalen);
    memcpy(mBuf, data, datalen);
}

nsPreloadedStream::~nsPreloadedStream()
{
    moz_free(mBuf);
}

NS_IMETHODIMP
nsPreloadedStream::Close()
{
    mLen = 0;
    return mStream->Close();
}


NS_IMETHODIMP
nsPreloadedStream::Available(uint64_t *_retval)
{
    uint64_t avail = 0;
    
    nsresult rv = mStream->Available(&avail);
    if (NS_FAILED(rv))
        return rv;
    *_retval = avail + mLen;
    return NS_OK;
}

NS_IMETHODIMP
nsPreloadedStream::Read(char *aBuf, uint32_t aCount,
                        uint32_t *_retval)
{
    if (!mLen)
        return mStream->Read(aBuf, aCount, _retval);
    
    uint32_t toRead = std::min(mLen, aCount);
    memcpy(aBuf, mBuf + mOffset, toRead);
    mOffset += toRead;
    mLen -= toRead;
    *_retval = toRead;
    return NS_OK;
}

NS_IMETHODIMP
nsPreloadedStream::ReadSegments(nsWriteSegmentFun aWriter,
                                void *aClosure, uint32_t aCount,
                                uint32_t *result)
{
    if (!mLen)
        return mStream->ReadSegments(aWriter, aClosure, aCount, result);

    *result = 0;
    while (mLen > 0 && aCount > 0) {
        uint32_t toRead = std::min(mLen, aCount);
        uint32_t didRead = 0;
        nsresult rv;

        rv = aWriter(this, aClosure, mBuf + mOffset, *result, toRead, &didRead);

        if (NS_FAILED(rv))
            return NS_OK;

        *result += didRead;
        mOffset += didRead;
        mLen -= didRead;
        aCount -= didRead;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsPreloadedStream::IsNonBlocking(bool *_retval)
{
    return mStream->IsNonBlocking(_retval);
}

NS_IMETHODIMP
nsPreloadedStream::CloseWithStatus(nsresult aStatus)
{
    mLen = 0;
    return mStream->CloseWithStatus(aStatus);
}

class RunOnThread : public nsRunnable
{
public:
    RunOnThread(nsIAsyncInputStream *aStream,
                nsIInputStreamCallback *aCallback)
      : mStream(aStream),
        mCallback(aCallback) {}
    
    virtual ~RunOnThread() {}
    
    NS_IMETHOD Run()
    {
        mCallback->OnInputStreamReady(mStream);
        return NS_OK;
    }

private:
    nsCOMPtr<nsIAsyncInputStream>    mStream;
    nsCOMPtr<nsIInputStreamCallback> mCallback;
};

NS_IMETHODIMP
nsPreloadedStream::AsyncWait(nsIInputStreamCallback *aCallback,
                             uint32_t aFlags,
                             uint32_t aRequestedCount,
                             nsIEventTarget *aEventTarget)
{
    if (!mLen)
        return mStream->AsyncWait(aCallback, aFlags, aRequestedCount,
                                  aEventTarget);

    if (!aCallback)
        return NS_OK;

    if (!aEventTarget)
        return aCallback->OnInputStreamReady(this);

    nsCOMPtr<nsIRunnable> event =
        new RunOnThread(this, aCallback);
    return aEventTarget->Dispatch(event, nsIEventTarget::DISPATCH_NORMAL);
}

} // namespace net
} // namespace mozilla
