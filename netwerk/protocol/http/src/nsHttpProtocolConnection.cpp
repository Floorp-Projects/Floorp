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

#include "nsHttpProtocolConnection.h"
#include "nscore.h"
#include "nsIUrl.h"
#include "nsIHttpEventSink.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsISocketTransportService.h"
#include "nsHttpProtocolHandler.h"
#include "nsITransport.h"
#include "nsIByteBufferInputStream.h"
#include "nsCRT.h"

static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

////////////////////////////////////////////////////////////////////////////////

nsHttpProtocolConnection::nsHttpProtocolConnection()
    : mHandler(nsnull), mUrl(nsnull), mEventSink(nsnull), mState(UNCONNECTED),
      mEventQueue(nsnull)
{
}

nsHttpProtocolConnection::~nsHttpProtocolConnection()
{
    NS_IF_RELEASE(mHandler);
    NS_IF_RELEASE(mUrl);
    NS_IF_RELEASE(mEventSink);
}

NS_IMPL_ADDREF(nsHttpProtocolConnection);
NS_IMPL_RELEASE(nsHttpProtocolConnection);

NS_IMETHODIMP
nsHttpProtocolConnection::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    NS_ASSERTION(aInstancePtr, "no instance pointer");
    if (aIID.Equals(nsIHttpProtocolConnection::GetIID()) ||
        aIID.Equals(nsIProtocolConnection::GetIID()) ||
        aIID.Equals(kISupportsIID)) {
        *aInstancePtr = NS_STATIC_CAST(nsIHttpProtocolConnection*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(nsIStreamListener::GetIID()) ||
        aIID.Equals(nsIStreamObserver::GetIID())) {
        *aInstancePtr = NS_STATIC_CAST(nsIStreamListener*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE; 
}

nsresult 
nsHttpProtocolConnection::Init(nsIUrl* url, nsISupports* eventSink, 
                               nsHttpProtocolHandler* handler,
                               PLEventQueue* eventQueue)
{
    nsresult rv;

    mHandler = handler;
    NS_ADDREF(mHandler);

    mUrl = url;
    NS_ADDREF(mUrl);

    rv = eventSink->QueryInterface(nsIHttpEventSink::GetIID(), (void**)&mEventSink);
    if (NS_FAILED(rv)) return rv;

    mEventQueue = eventQueue;
    
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsICancelable methods:

NS_IMETHODIMP
nsHttpProtocolConnection::Cancel(void)
{
    switch (mState) {
      case CONNECTED:
        break;
      case POSTING:
        break;
      default:
        return NS_ERROR_NOT_CONNECTED;
    }
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHttpProtocolConnection::Suspend(void)
{
    switch (mState) {
      case CONNECTED:
        break;
      case POSTING:
        break;
      default:
        return NS_ERROR_NOT_CONNECTED;
    }
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHttpProtocolConnection::Resume(void)
{
    switch (mState) {
      case CONNECTED:
        break;
      case POSTING:
        break;
      default:
        return NS_ERROR_NOT_CONNECTED;
    }
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
// nsIProtocolConnection methods:

NS_IMETHODIMP
nsHttpProtocolConnection::Open(void)
{
    nsresult rv = NS_OK;

    const char* host;
    rv = mUrl->GetHost(&host);
    if (NS_FAILED(rv)) return rv;

    PRInt32 port;
    rv = mUrl->GetPort(&port);
    if (NS_FAILED(rv)) return rv;
    
    rv = mHandler->GetTransport(host, port, &mTransport);
    mState = CONNECTED;
    return rv;
}

NS_IMETHODIMP
nsHttpProtocolConnection::GetContentType(char* *contentType)
{
    if (mState != CONNECTED)
        return NS_ERROR_NOT_CONNECTED;
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHttpProtocolConnection::GetInputStream(nsIInputStream* *result)
{
    if (mState != CONNECTED)
        return NS_ERROR_NOT_CONNECTED;
    return mTransport->OpenInputStream(result);
}

NS_IMETHODIMP
nsHttpProtocolConnection::GetOutputStream(nsIOutputStream* *result)
{
    if (mState != CONNECTED)
        return NS_ERROR_NOT_CONNECTED;
    return mTransport->OpenOutputStream(result);
}

////////////////////////////////////////////////////////////////////////////////
// nsIHttpProtocolConnection methods:

static nsresult
Write(nsIByteBufferInputStream* in, const char* str, 
      PRInt32 len = -1)
{
    if (len == -1)
        len = nsCRT::strlen(str);
    PRUint32 writeCnt;
    nsresult rv = in->Fill(str, (PRUint32)len, &writeCnt);
    NS_ASSERTION((PRInt32)writeCnt == len, "nsIByteBufferInputStream failed");
    return rv;
}

NS_IMETHODIMP
nsHttpProtocolConnection::GetHeader(const char* header, char* *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHttpProtocolConnection::AddHeader(const char* header, const char* value)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHttpProtocolConnection::RemoveHeader(const char* header)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHttpProtocolConnection::Get(void)
{
    nsresult rv;

    if (mState != CONNECTED)
        return NS_ERROR_NOT_CONNECTED;

    // build up the http request:
    nsIByteBufferInputStream* in;
    rv = NS_NewByteBufferInputStream(&in);
    if (NS_FAILED(rv)) return rv;

    rv = Write(in, "GET ", 4);
    if (NS_FAILED(rv)) goto done;
    const char* path;
    rv = mUrl->GetPath(&path);
    if (NS_FAILED(rv)) goto done;
    rv = Write(in, path);
    if (NS_FAILED(rv)) goto done;
    rv = Write(in, " ", 1);
    if (NS_FAILED(rv)) goto done;
    rv = Write(in, HTTP_DEFAULT_VERSION_STRING, HTTP_VERSION_STRING_LENGTH);
    if (NS_FAILED(rv)) goto done;
    rv = Write(in, CRLF, 2);
    if (NS_FAILED(rv)) goto done;

    // blank line to end the request
    rv = Write(in, CRLF, 2);
    if (NS_FAILED(rv)) goto done;

    // send it to the server:
    rv = mTransport->AsyncWrite(in,
                                NS_STATIC_CAST(nsIProtocolConnection*, this),
                                mEventQueue, this);
  done:
    NS_RELEASE(in);
    return rv;
}

NS_IMETHODIMP
nsHttpProtocolConnection::GetByteRange(PRUint32 from, PRUint32 to)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHttpProtocolConnection::Put(void)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHttpProtocolConnection::Post(void)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
// nsIStreamObserver methods:

NS_IMETHODIMP
nsHttpProtocolConnection::OnStartBinding(nsISupports* context)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHttpProtocolConnection::OnStopBinding(nsISupports* context,
                                        nsresult aStatus,
                                        nsIString* aMsg)
{
    nsresult rv = NS_OK;
    switch (mState) {
      case CONNECTED:
        if (NS_SUCCEEDED(aStatus)) {
            // read the reply
            mState = WAITING_REPLY;
            rv = mTransport->AsyncRead(NS_STATIC_CAST(nsIProtocolConnection*, this),
                                       mEventQueue, this);
            if (NS_FAILED(rv)) return rv;
        }
        break;
        
      case WAITING_REPLY:
        // reply finished

//        rv = mHandler->ReleaseTransport(mTransport);
//        if (NS_FAILED(rv)) return rv;

        mState = UNCONNECTED;

        break;
      default:
        NS_NOTREACHED("bad state");
        break;
    }
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIStreamListener methods:

NS_IMETHODIMP
nsHttpProtocolConnection::OnDataAvailable(nsISupports* context,
                                          nsIInputStream *aIStream, 
                                          PRUint32 aSourceOffset,
                                          PRUint32 aLength)
{
    nsresult rv = NS_OK;
    switch (mState) {
      case WAITING_REPLY:
        // data coming in for our request

        // XXX do some header parsing here

//        mEventSink->OnProgress();
        rv = mEventSink->OnDataAvailable(context, aIStream, aSourceOffset, aLength);

        break;
      default:
        NS_NOTREACHED("bad state");
        break;
    }
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
