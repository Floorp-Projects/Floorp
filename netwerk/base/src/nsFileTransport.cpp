/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsFileTransport.h"
#include "nsFileTransportService.h"
#include "nsIStreamListener.h"
#include "nsIProtocolConnection.h"
#include "nsCRT.h"
#include "nscore.h"
#include "nsIFileStream.h"
#include "nsFileSpec.h"
#include "nsIByteBufferInputStream.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

////////////////////////////////////////////////////////////////////////////////
// nsFileTransport methods:

nsFileTransport::nsFileTransport()
    : mPath(nsnull), mListener(nsnull), mCanceled(PR_FALSE)
{
    NS_INIT_REFCNT();
}

nsFileTransport::~nsFileTransport()
{
    if (mPath)
        delete mPath;
    NS_IF_RELEASE(mListener);
}

nsresult
nsFileTransport::Init(const char* path,
                      nsIStreamListener* listener,
                      PLEventQueue* appEventQueue,
                      nsIProtocolConnection* connection)
{
    nsresult rv;
    mPath = nsCRT::strdup(path);
    if (mPath == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    
    mConnection = connection;
    NS_ADDREF(mConnection);

    rv = NS_NewMarshalingStreamListener(appEventQueue, listener, &mListener);
    if (NS_FAILED(rv)) return rv;

    return rv;
}

NS_IMPL_ADDREF(nsFileTransport);
NS_IMPL_RELEASE(nsFileTransport);

NS_IMETHODIMP
nsFileTransport::QueryInterface(const nsIID& aIID, void* *aInstancePtr)
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER; 
    } 
    if (aIID.Equals(nsITransport::GetIID()) ||
        aIID.Equals(nsICancelable::GetIID()) ||
        aIID.Equals(kISupportsIID)) {
        *aInstancePtr = NS_STATIC_CAST(nsITransport*, this); 
        NS_ADDREF_THIS(); 
        return NS_OK; 
    } 
    if (aIID.Equals(nsIRunnable::GetIID())) {
        *aInstancePtr = NS_STATIC_CAST(nsIRunnable*, this); 
        NS_ADDREF_THIS(); 
        return NS_OK; 
    } 
    return NS_NOINTERFACE; 
}

////////////////////////////////////////////////////////////////////////////////
// nsICancelable methods:

NS_IMETHODIMP
nsFileTransport::Cancel(void)
{
    mCanceled = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsFileTransport::Suspend(void)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileTransport::Resume(void)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileTransport::Run(void)
{
    nsresult rv;
    nsISupports* fs;
    nsFileSpec spec(mPath);
    nsIInputStream* fileStr = nsnull;
    nsIByteBufferInputStream* bufStr = nsnull;

    rv = mListener->OnStartBinding(mConnection);  // always send the start notification

    rv = NS_NewTypicalInputFileStream(&fs, spec);
    if (NS_FAILED(rv)) goto done;
    rv = fs->QueryInterface(nsIInputStream::GetIID(), (void**)&fileStr);
    NS_RELEASE(fs);
    if (NS_FAILED(rv)) goto done;

    rv = NS_NewByteBufferInputStream(NS_FILE_TRANSPORT_BUFFER_SIZE, &bufStr);
    if (NS_FAILED(rv)) goto done;

    while (PR_TRUE) {
        PRUint32 amt;
        rv = bufStr->Fill(fileStr, &amt);
        if (rv == NS_BASE_STREAM_EOF || amt == 0) {
            rv = NS_OK;
            break;
        }
        if (NS_FAILED(rv)) break;

        // and feed the buffer to the application via the byte buffer stream:
        rv = mListener->OnDataAvailable(mConnection, bufStr, amt);      // XXX maybe amt should be bufStr->GetLength()
        if (NS_FAILED(rv)) break;
    }

  done:
    NS_IF_RELEASE(bufStr);
    NS_IF_RELEASE(fileStr);

    // XXX where do we get the error message?
    rv = mListener->OnStopBinding(mConnection, rv, nsnull);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
