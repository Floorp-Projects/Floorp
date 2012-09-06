/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSimpleStreamListener.h"

//
//----------------------------------------------------------------------------
// nsISupports implementation...
//----------------------------------------------------------------------------
//
NS_IMPL_ISUPPORTS3(nsSimpleStreamListener,
                   nsISimpleStreamListener,
                   nsIStreamListener,
                   nsIRequestObserver)

//
//----------------------------------------------------------------------------
// nsIRequestObserver implementation...
//----------------------------------------------------------------------------
//
NS_IMETHODIMP
nsSimpleStreamListener::OnStartRequest(nsIRequest *aRequest,
                                       nsISupports *aContext)
{
    return mObserver ?
        mObserver->OnStartRequest(aRequest, aContext) : NS_OK;
}

NS_IMETHODIMP
nsSimpleStreamListener::OnStopRequest(nsIRequest* request,
                                      nsISupports *aContext,
                                      nsresult aStatus)
{
    return mObserver ?
        mObserver->OnStopRequest(request, aContext, aStatus) : NS_OK;
}

//
//----------------------------------------------------------------------------
// nsIStreamListener implementation...
//----------------------------------------------------------------------------
//
NS_IMETHODIMP
nsSimpleStreamListener::OnDataAvailable(nsIRequest* request,
                                        nsISupports *aContext,
                                        nsIInputStream *aSource,
                                        uint64_t aOffset,
                                        uint32_t aCount)
{
    uint32_t writeCount;
    nsresult rv = mSink->WriteFrom(aSource, aCount, &writeCount);
    //
    // Equate zero bytes read and NS_SUCCEEDED to stopping the read.
    //
    if (NS_SUCCEEDED(rv) && (writeCount == 0))
        return NS_BASE_STREAM_CLOSED;
    return rv;
}

//
//----------------------------------------------------------------------------
// nsISimpleStreamListener implementation...
//----------------------------------------------------------------------------
//
NS_IMETHODIMP
nsSimpleStreamListener::Init(nsIOutputStream *aSink,
                             nsIRequestObserver *aObserver)
{
    NS_PRECONDITION(aSink, "null output stream");

    mSink = aSink;
    mObserver = aObserver;

    return NS_OK;
}
