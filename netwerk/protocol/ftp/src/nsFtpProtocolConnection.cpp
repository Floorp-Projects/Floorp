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
#include "nsIByteBufferInputStream.h"

#include "prprf.h" // PR_sscanf

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
    mServerType(FTP_GENERIC_TYPE), mConnected(FALSE),
    mResponseCode(0) {

    mEventQueue = PL_CreateEventQueue("FTP Event Queue", PR_CurrentThread());
}

nsFtpProtocolConnection::~nsFtpProtocolConnection() {
    NS_IF_RELEASE(mUrl);
    NS_IF_RELEASE(mEventSink);
    NS_IF_RELEASE(mCPipe);
    NS_IF_RELEASE(mDPipe);
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
nsFtpProtocolConnection::Init(nsIUrl* aUrl, nsISupports* aEventSink, PLEventQueue* aEventQueue) {
    nsresult rv;

    if (mConnected)
        return NS_ERROR_NOT_IMPLEMENTED;

    mUrl = aUrl;
    NS_ADDREF(mUrl);

    rv = aEventSink->QueryInterface(nsIFtpEventSink::GetIID(), (void**)&mEventSink);

    mEventQueue = aEventQueue;

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
    const char *host;
    const PRInt32 port = 0;
    rv = mUrl->GetHost(&host);
    if (NS_FAILED(rv)) return rv;
    rv = mUrl->GetPort(port);
	if (NS_FAILED(rv)) return rv;
    rv = sts->CreateTransport(host, port, &mCPipe); // the command channel
    if (NS_FAILED(rv)) return rv;

    // XXX the underlying PR_Connect() has not occured, but we
    // XXX don't know when the transport does that so, we're going to
    // XXX consider ourselves conencted.
	mConnected = TRUE;
	mState = FTP_S_USER;

    return NS_OK;
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
    nsresult rv;
    char *buffer = nsnull;                          // the buffer to be sent to the server
    nsIByteBufferInputStream* stream = nsnull;      // stream to be filled, with buffer, then written to the server
    PRUint32 bytesWritten = 0;

    // each hunk of data that comes in is evaluated, appropriate action 
    // is taken and the state is incremented.
    switch(mState) {
        case FTP_CONNECT:

        case FTP_S_USER:
            rv = NS_NewByteBufferInputStream(&stream);
            if (NS_FAILED(rv)) return rv;

            buffer = "USER anonymous";
            stream->Fill(buffer, strlen(buffer), &bytesWritten);

            // Send off the command
            mCPipe->AsyncWrite(stream, nsnull, mEventQueue, NS_STATIC_CAST(nsIStreamObserver*, this));
            mState = FTP_R_USER;
            break;

        case FTP_R_USER:
            // start the async read.
            mCPipe->AsyncRead(nsnull, mEventQueue, NS_STATIC_CAST(nsIStreamListener*, this));
            break;

        case FTP_S_PASV:
            if (!mPassword) {
                // XXX we need to prompt the user to enter a password.

                // sendEventToUIThreadToPostADialog(&mPassword);
            }
            rv = NS_NewByteBufferInputStream(&stream);
            if (NS_FAILED(rv)) return rv;

            buffer = "PASS guest\r\n";
            // PR_smprintf(buffer, "PASS %.256s\r\n", mPassword);
            stream->Fill(buffer, strlen(buffer), &bytesWritten);
            
            // send off the command
            mCPipe->AsyncWrite(stream, nsnull, mEventQueue, NS_STATIC_CAST(nsIStreamListener*, this));
            mState = FTP_R_PASV;
            break;

        case FTP_R_PASV:
            // start the async read
            mCPipe->AsyncRead(nsnull, mEventQueue, NS_STATIC_CAST(nsIStreamListener*, this));
            break;

        case FTP_S_PORT:
        case FTP_R_PORT:
        case FTP_COMPLETE:
        default:
            ;
    }    


    // up call OnStopBinding
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
// nsIStreamListener methods:

// this is the command channel's OnDataAvailable(). It's job is to processes AsyncReads.
// It absorbs the data from the stream, parses it, and passes the data onto the appropriate
// routine/logic for handling.
NS_IMETHODIMP
nsFtpProtocolConnection::OnDataAvailable(nsISupports* context,
                                          nsIInputStream *aIStream, 
                                          PRUint32 aLength) {
    nsresult rv;

    // we've got some data. suck it out of the inputSteam.
    char *buffer = new char[aLength+1];
    // XXX maybe use an nsString
    PRUint32 read = 0;
    rv = aIStream->Read(buffer, aLength, &read);
    if (NS_FAILED(rv)) return rv;

    PR_sscanf(buffer, "%d", &mResponseCode);

    // these are states in which data is coming back to us. read states.
    // This switch is a state incrementor using the server response to
    // determine what the next state shall be.
    switch(mState) {
        case FTP_R_USER:
            if (mResponseCode == 3) {
                // send off the password
                mState = FTP_S_PASV;
                OnStopBinding(nsnull, rv, nsnull);
            } else if (mResponseCode == 2) {
                // no password required, we're already logged in
            }
            break;

        case FTP_R_PASV:
            if (mResponseCode == 3) {
                // send account info
                mState = FTP_S_ACCT;
            } else if (mResponseCode == 2) {
                // logged in
                // XXX next state?
            }
            OnStopBinding(nsnull, rv, nsnull);
        case FTP_R_PORT:
        case FTP_COMPLETE:
        default:
            ;
    }

    delete [] buffer;
    return NS_OK;
}
////////////////////////////////////////////////////////////////////////////////
