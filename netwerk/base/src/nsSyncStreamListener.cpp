/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIOService.h"
#include "nsSyncStreamListener.h"
#include "nsIPipe.h"
#include "nsThreadUtils.h"
#include <algorithm>

nsresult
nsSyncStreamListener::Init()
{
    return NS_NewPipe(getter_AddRefs(mPipeIn),
                      getter_AddRefs(mPipeOut),
                      nsIOService::gDefaultSegmentSize,
                      UINT32_MAX, // no size limit
                      false,
                      false);
}

nsresult
nsSyncStreamListener::WaitForData()
{
    mKeepWaiting = true;

    while (mKeepWaiting)
        NS_ENSURE_STATE(NS_ProcessNextEvent(NS_GetCurrentThread()));

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsSyncStreamListener::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS4(nsSyncStreamListener,
                   nsIStreamListener,
                   nsIRequestObserver,
                   nsIInputStream,
                   nsISyncStreamListener)

//-----------------------------------------------------------------------------
// nsSyncStreamListener::nsISyncStreamListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsSyncStreamListener::GetInputStream(nsIInputStream **result)
{
    NS_ADDREF(*result = this);
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsSyncStreamListener::nsIStreamListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsSyncStreamListener::OnStartRequest(nsIRequest  *request,
                                     nsISupports *context)
{
    return NS_OK;
}

NS_IMETHODIMP
nsSyncStreamListener::OnDataAvailable(nsIRequest     *request,
                                      nsISupports    *context,
                                      nsIInputStream *stream,
                                      uint64_t        offset,
                                      uint32_t        count)
{
    uint32_t bytesWritten;

    nsresult rv = mPipeOut->WriteFrom(stream, count, &bytesWritten);

    // if we get an error, then return failure.  this will cause the
    // channel to be canceled, and as a result our OnStopRequest method
    // will be called immediately.  because of this we do not need to
    // set mStatus or mKeepWaiting here.
    if (NS_FAILED(rv))
        return rv;

    // we expect that all data will be written to the pipe because
    // the pipe was created to have "infinite" room.
    NS_ASSERTION(bytesWritten == count, "did not write all data"); 

    mKeepWaiting = false; // unblock Read
    return NS_OK;
}

NS_IMETHODIMP
nsSyncStreamListener::OnStopRequest(nsIRequest  *request,
                                    nsISupports *context,
                                    nsresult     status)
{
    mStatus = status;
    mKeepWaiting = false; // unblock Read
    mDone = true;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsSyncStreamListener::nsIInputStream
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsSyncStreamListener::Close()
{
    mStatus = NS_BASE_STREAM_CLOSED;
    mDone = true;

    // It'd be nice if we could explicitly cancel the request at this point,
    // but we don't have a reference to it, so the best we can do is close the
    // pipe so that the next OnDataAvailable event will fail.
    if (mPipeIn) {
        mPipeIn->Close();
        mPipeIn = nullptr;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsSyncStreamListener::Available(uint64_t *result)
{
    if (NS_FAILED(mStatus))
        return mStatus;

    mStatus = mPipeIn->Available(result);
    if (NS_SUCCEEDED(mStatus) && (*result == 0) && !mDone) {
        mStatus = WaitForData();
        if (NS_SUCCEEDED(mStatus))
            mStatus = mPipeIn->Available(result);
    }
    return mStatus;
}

NS_IMETHODIMP
nsSyncStreamListener::Read(char     *buf,
                           uint32_t  bufLen,
                           uint32_t *result)
{
    if (mStatus == NS_BASE_STREAM_CLOSED) {
        *result = 0;
        return NS_OK;
    }

    uint64_t avail64;
    if (NS_FAILED(Available(&avail64)))
        return mStatus;

    uint32_t avail = (uint32_t)std::min(avail64, (uint64_t)bufLen);
    mStatus = mPipeIn->Read(buf, avail, result);
    return mStatus;
}

NS_IMETHODIMP
nsSyncStreamListener::ReadSegments(nsWriteSegmentFun  writer,
                                   void              *closure,
                                   uint32_t           count,
                                   uint32_t          *result)
{
    if (mStatus == NS_BASE_STREAM_CLOSED) {
        *result = 0;
        return NS_OK;
    }

    uint64_t avail64;
    if (NS_FAILED(Available(&avail64)))
        return mStatus;

    uint32_t avail = (uint32_t)std::min(avail64, (uint64_t)count);
    mStatus = mPipeIn->ReadSegments(writer, closure, avail, result);
    return mStatus;
}

NS_IMETHODIMP
nsSyncStreamListener::IsNonBlocking(bool *result)
{
    *result = false;
    return NS_OK;
}
