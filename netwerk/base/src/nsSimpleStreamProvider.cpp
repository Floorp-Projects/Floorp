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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com> (original author)
 */

#include "nsSimpleStreamProvider.h"
#include "nsIOutputStream.h"

//
//----------------------------------------------------------------------------
// nsISupports implementation...
//----------------------------------------------------------------------------
//
NS_IMPL_THREADSAFE_ISUPPORTS3(nsSimpleStreamProvider,
                              nsISimpleStreamProvider,
                              nsIStreamProvider,
                              nsIRequestObserver)

//
//----------------------------------------------------------------------------
// nsIRequestObserver implementation...
//----------------------------------------------------------------------------
//
NS_IMETHODIMP
nsSimpleStreamProvider::OnStartRequest(nsIRequest *request,
                                       nsISupports *aContext)
{
    return mObserver ?
        mObserver->OnStartRequest(request, aContext) : NS_OK;
}

NS_IMETHODIMP
nsSimpleStreamProvider::OnStopRequest(nsIRequest* request,
                                      nsISupports *aContext,
                                      nsresult aStatus)
{
    return mObserver ?
        mObserver->OnStopRequest(request, aContext, aStatus) : NS_OK;
}

//
//----------------------------------------------------------------------------
// nsIStreamProvider implementation...
//----------------------------------------------------------------------------
//
NS_IMETHODIMP
nsSimpleStreamProvider::OnDataWritable(nsIRequest *request,
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
                             nsIRequestObserver *aObserver)
{
    NS_PRECONDITION(aSource, "null input stream");

    mSource = aSource;
    mObserver = aObserver;

    return NS_OK;
}
