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

#include "nsSimpleStreamProvider.h"
#include "nsIOutputStream.h"

//
//----------------------------------------------------------------------------
// nsISupports implementation...
//----------------------------------------------------------------------------
//
NS_IMPL_ISUPPORTS3(nsSimpleStreamProvider,
                   nsISimpleStreamProvider,
                   nsIStreamProvider,
                   nsIStreamObserver)

//
//----------------------------------------------------------------------------
// nsIStreamObserver implementation...
//----------------------------------------------------------------------------
//
NS_IMETHODIMP
nsSimpleStreamProvider::OnStartRequest(nsIChannel *aChannel,
                                       nsISupports *aContext)
{
    return mObserver ?
        mObserver->OnStartRequest(aChannel, aContext) : NS_OK;
}

NS_IMETHODIMP
nsSimpleStreamProvider::OnStopRequest(nsIChannel *aChannel,
                                      nsISupports *aContext,
                                      nsresult aStatus,
                                      const PRUnichar *aStatusText)
{
    return mObserver ?
        mObserver->OnStopRequest(aChannel, aContext, aStatus, aStatusText) : NS_OK;
}

//
//----------------------------------------------------------------------------
// nsIStreamProvider implementation...
//----------------------------------------------------------------------------
//
NS_IMETHODIMP
nsSimpleStreamProvider::OnDataWritable(nsIChannel *aChannel,
                                       nsISupports *aContext,
                                       nsIOutputStream *aOutput,
                                       PRUint32 aOffset,
                                       PRUint32 aCount)
{
    PRUint32 writeCount;
    nsresult rv = aOutput->WriteFrom(mSource, aCount, &writeCount);
    //
    // Equate zero bytes written and NS_SUCCEEDED to EOF
    //
    if (NS_SUCCEEDED(rv) && (writeCount == 0))
        return NS_BASE_STREAM_CLOSED;
    return rv;
}

//
//----------------------------------------------------------------------------
// nsISimpleStreamProvider implementation...
//----------------------------------------------------------------------------
//
NS_IMETHODIMP
nsSimpleStreamProvider::Init(nsIInputStream *aSource,
                             nsIStreamObserver *aObserver)
{
    NS_PRECONDITION(aSource, "null input stream");

    mSource = aSource;
    mObserver = aObserver;

    return NS_OK;
}
