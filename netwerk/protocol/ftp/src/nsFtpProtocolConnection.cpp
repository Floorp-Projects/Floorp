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

// ftp implementation

#include "nsFtpProtocolConnection.h"
#include "nscore.h"
#include "nsIUrl.h"
#include "nsIFtpEventSink.h"
#include "nsISocketTransportService.h"
#include "nsIServiceManager.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);

// There are actually two transport connections established for an 
// ftp connection. One is used for the command channel , and
// the other for the data channel. The command channel is the first
// connection made and is used to negotiate the second, data, channel.
// The data channel is driven by the command channel and is either
// initiated by the server (PORT command) or by the client (PASV command).
// Client initiation is the most command case and is attempted first.

nsFtpProtocolConnection::nsFtpProtocolConnection()
    : mUrl(nsnull), mEventSink(nsnull), mPasv(TRUE),
    mServerType(FTP_GENERIC_TYPE), mConnected(FALSE) {
}

nsFtpProtocolConnection::~nsFtpProtocolConnection() {
    NS_IF_RELEASE(mUrl);
    NS_IF_RELEASE(mEventSink);
}

NS_IMPL_ADDREF(nsFtpProtocolConnection);
NS_IMPL_RELEASE(nsFtpProtocolConnection);

NS_IMETHODIMP
nsFtpProtocolConnection::QueryInterface(const nsIID& aIID, void** aInstancePtr) {
    NS_ASSERTION(aInstancePtr, "no instance pointer");
    if (aIID.Equals(nsIFtpProtocolConnection::GetIID()) ||
        aIID.Equals(nsIProtocolConnection::GetIID()) ||
        aIID.Equals(kISupportsIID) ) {
        *aInstancePtr = NS_STATIC_CAST(nsIFtpProtocolConnection*, this);
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
nsFtpProtocolConnection::Init(nsIUrl* url, nsISupports* eventSink,
                              PLEventQueue* eventQueue)
{
    nsresult rv;

    if (mConnected)
        return NS_ERROR_NOT_IMPLEMENTED;

    mUrl = url;
    NS_ADDREF(mUrl);

    rv = eventSink->QueryInterface(nsIFtpEventSink::GetIID(), (void**)&mEventSink);

    mEventQueue = eventQueue;

    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsICancelable methods:

NS_IMETHODIMP
nsFtpProtocolConnection::Cancel(void) {
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFtpProtocolConnection::Suspend(void) {
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFtpProtocolConnection::Resume(void) {
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
// nsIProtocolConnection methods:


// establishes the connection and initiates the file transfer.
NS_IMETHODIMP
nsFtpProtocolConnection::Open(void) {
    nsresult rv;

    mState = FTP_CONNECT;

    NS_WITH_SERVICE(nsISocketTransportService, sts, kSocketTransportServiceCID, &rv);
    if(NS_FAILED(rv)) return rv;

    // Create the command channel transport

    // const char *host;
    // const PRInt32 port;
    // rv = url->GetHost(&host);
    // if (NS_FAILED(rv)) return rv;
    // port = url->GetPort();
    // nsITransport *cPipe; // the command channel
    // rv = sts->CreateTransport(host, port, *cPipe);
    // if(NS_FAILED(rv)) return rv;

    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFtpProtocolConnection::GetContentType(char* *contentType) {
    
    // XXX for ftp we need to do a file extension-to-type mapping lookup
    // XXX in some hash table/registry of mime-types
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFtpProtocolConnection::GetInputStream(nsIInputStream* *result) {
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFtpProtocolConnection::GetOutputStream(nsIOutputStream* *result) {
    return NS_ERROR_NOT_IMPLEMENTED;
}


////////////////////////////////////////////////////////////////////////////////
// nsIFtpProtocolConnection methods:

NS_IMETHODIMP
nsFtpProtocolConnection::Get(void) {
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFtpProtocolConnection::Put(void) {
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFtpProtocolConnection::UsePASV(PRBool aComm) {
    if (mConnected)
        return NS_ERROR_NOT_IMPLEMENTED;
    mPasv = aComm;
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
// nsIStreamObserver methods:

NS_IMETHODIMP
nsFtpProtocolConnection::OnStartBinding(nsISupports* context) {
    // up call OnStartBinding
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFtpProtocolConnection::OnStopBinding(nsISupports* context,
                                        nsresult aStatus,
                                        nsIString* aMsg) {
    // up call OnStopBinding
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
// nsIStreamListener methods:

// state machine
NS_IMETHODIMP
nsFtpProtocolConnection::OnDataAvailable(nsISupports* context,
                                          nsIInputStream *aIStream, 
                                          PRUint32 aLength) {
    // each hunk of data that comes in is evaluated, appropriate action 
    // is taken and the state is incremented.
    switch(mState) {
        case FTP_CONNECT:
        case FTP_S_PASV:
        case FTP_R_PASV:
        case FTP_S_PORT:
        case FTP_R_PORT:
        case FTP_COMPLETE:
        default:
            ;
    }

    return NS_ERROR_NOT_IMPLEMENTED;
}
////////////////////////////////////////////////////////////////////////////////
