/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 */

#include "nsSimpleStreamListener.h"

//
//----------------------------------------------------------------------------
// nsISupports implementation...
//----------------------------------------------------------------------------
//
NS_IMPL_ISUPPORTS3(nsSimpleStreamListener,
                   nsISimpleStreamListener,
                   nsIStreamListener,
                   nsIStreamObserver)

//
//----------------------------------------------------------------------------
// nsIStreamObserver implementation...
//----------------------------------------------------------------------------
//
NS_IMETHODIMP
nsSimpleStreamListener::OnStartRequest(nsIChannel *aChannel,
                                       nsISupports *aContext)
{
    return mObserver ?
        mObserver->OnStartRequest(aChannel, aContext) : NS_OK;
}

NS_IMETHODIMP
nsSimpleStreamListener::OnStopRequest(nsIChannel *aChannel,
                                      nsISupports *aContext,
                                      nsresult aStatus,
                                      const PRUnichar *aStatusText)
{
    return mObserver ?
        mObserver->OnStopRequest(aChannel, aContext, aStatus, aStatusText) : NS_OK;
}

//
//----------------------------------------------------------------------------
// nsIStreamListener implementation...
//----------------------------------------------------------------------------
//
NS_IMETHODIMP
nsSimpleStreamListener::OnDataAvailable(nsIChannel *aChannel,
                                        nsISupports *aContext,
                                        nsIInputStream *aSource,
                                        PRUint32 aOffset,
                                        PRUint32 aCount)
{
    PRUint32 writeCount;
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
                             nsIStreamObserver *aObserver)
{
    NS_PRECONDITION(aSink, "null output stream");

    mSink = aSink;
    mObserver = aObserver;

    return NS_OK;
}
