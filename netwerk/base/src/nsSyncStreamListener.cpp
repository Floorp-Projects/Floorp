/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsSyncStreamListener.h"
#include "nsCRT.h"
#include "nsIPipe.h"

////////////////////////////////////////////////////////////////////////////////

nsresult 
nsSyncStreamListener::Init(nsIInputStream* *inStr, nsIOutputStream* *outStr)
{
    nsresult rv;
    nsCOMPtr<nsIInputStream> in;

    rv = NS_NewPipe(getter_AddRefs(in),
                    getter_AddRefs(mOutputStream),
                    NS_SYNC_STREAM_LISTENER_SEGMENT_SIZE,
                    NS_SYNC_STREAM_LISTENER_BUFFER_SIZE);
    if (NS_FAILED(rv)) return rv;

    *inStr = in.get();
    *outStr = mOutputStream.get();
    NS_ADDREF(*inStr);
    NS_ADDREF(*outStr);
    return NS_OK;
}

nsSyncStreamListener::~nsSyncStreamListener()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS3(nsSyncStreamListener, 
                              nsISyncStreamListener,
                              nsIStreamListener,
                              nsIStreamObserver)

NS_IMETHODIMP 
nsSyncStreamListener::OnStartRequest(nsIChannel* channel, nsISupports* context)
{
    return NS_OK;
}

NS_IMETHODIMP 
nsSyncStreamListener::OnStopRequest(nsIChannel* channel, nsISupports* context,
                                    nsresult aStatus, const PRUnichar* aStatusArg)
{
    // XXX what do we do with the status and error message?
    return mOutputStream->Close();
}

NS_IMETHODIMP 
nsSyncStreamListener::OnDataAvailable(nsIChannel* channel,
                                      nsISupports* context, 
                                      nsIInputStream *aIStream, 
                                      PRUint32 aSourceOffset,
                                      PRUint32 aLength)
{
    nsresult rv;
    PRUint32 amt;
    PRInt32 count = (PRInt32)aLength;
    while (count > 0) {   // this should only go around once since the output stream is blocking
        rv = mOutputStream->WriteFrom(aIStream, count, &amt);
        if (NS_FAILED(rv)) return rv;
        count -= amt;
    }
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

NS_METHOD
nsSyncStreamListener::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;
    nsSyncStreamListener* l = new nsSyncStreamListener();
    if (l == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(l);
    nsresult rv = l->QueryInterface(aIID, aResult);
    NS_RELEASE(l);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
