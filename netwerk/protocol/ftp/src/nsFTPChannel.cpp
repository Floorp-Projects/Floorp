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

#include "nsFTPChannel.h"
#include "nscore.h"
#include "nsIServiceManager.h"
#include "nsIBufferInputStream.h"
#include "nsFtpConnectionThread.h"
#include "nsIEventQueueService.h"
#include "nsIProgressEventSink.h"
#include "nsIEventSinkGetter.h"
#include "nsIIOService.h"
#include "nsIPipe.h"
#include "nsILoadGroup.h"
#include "nsIFTPContext.h"
#include "nsIMIMEService.h"
#include "nsIPrincipal.h"

static NS_DEFINE_CID(kMIMEServiceCID, NS_MIMESERVICE_CID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);


#include "prprf.h" // PR_sscanf

#if defined(PR_LOGGING)
extern PRLogModuleInfo* gFTPLog;
#endif /* PR_LOGGING */


static NS_DEFINE_CID(kEventQueueService, NS_EVENTQUEUESERVICE_CID);

// There are actually two transport connections established for an 
// ftp connection. One is used for the command channel , and
// the other for the data channel. The command channel is the first
// connection made and is used to negotiate the second, data, channel.
// The data channel is driven by the command channel and is either
// initiated by the server (PORT command) or by the client (PASV command).
// Client initiation is the most command case and is attempted first.

nsFTPChannel::nsFTPChannel() {
    NS_INIT_REFCNT();

    mUrl = nsnull;
    mEventQueue = nsnull;
    mEventSink = nsnull;
    mConnected = PR_FALSE;
    mListener = nsnull;
    mContext = nsnull;
    mLoadAttributes = LOAD_NORMAL;
    mBufferInputStream = nsnull;
    mBufferOutputStream = nsnull;
    mSourceOffset = 0;
    mAmount = 0;
    mLoadGroup = nsnull;
}

nsFTPChannel::~nsFTPChannel() {
    NS_IF_RELEASE(mUrl);
    NS_IF_RELEASE(mListener);
    NS_IF_RELEASE(mEventQueue);
    NS_IF_RELEASE(mLoadGroup);
    NS_IF_RELEASE(mContext);
}

NS_IMPL_ADDREF(nsFTPChannel);
NS_IMPL_RELEASE(nsFTPChannel);

NS_IMETHODIMP
nsFTPChannel::QueryInterface(const nsIID& aIID, void** aInstancePtr) {
    NS_ASSERTION(aInstancePtr, "no instance pointer");
    if (aIID.Equals(nsCOMTypeInfo<nsIFTPChannel>::GetIID()) ||
        aIID.Equals(nsCOMTypeInfo<nsIChannel>::GetIID()) ||
        aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID()) ) {
        *aInstancePtr = NS_STATIC_CAST(nsIFTPChannel*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(nsCOMTypeInfo<nsIStreamListener>::GetIID()) ||
        aIID.Equals(nsCOMTypeInfo<nsIStreamObserver>::GetIID())) {
        *aInstancePtr = NS_STATIC_CAST(nsIStreamListener*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE; 
}

nsresult
nsFTPChannel::Init(const char* verb, nsIURI* uri, nsILoadGroup *aGroup,
                   nsIEventSinkGetter* getter)
{
    nsresult rv;

    if (mConnected)
        return NS_ERROR_FAILURE;

    mUrl = uri;
    NS_ADDREF(mUrl);

    mLoadGroup = aGroup;
    NS_IF_ADDREF(mLoadGroup);

    if (getter) {
        nsIProgressEventSink* eventSink;
        rv = getter->GetEventSink(verb, nsCOMTypeInfo<nsIProgressEventSink>::GetIID(), 
                                  (nsISupports**)&eventSink);
        if (NS_FAILED(rv)) {
            PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFTPChannel::Init() (couldn't find event sink)\n"));
        }

        // XXX event sinks are optional (for now) in ftp
        if (eventSink) {
            mEventSink = eventSink;
            NS_ADDREF(mEventSink);
        }
    }

    NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueService, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = eventQService->GetThreadEventQueue(PR_CurrentThread(), &mEventQueue);
    }

    return NS_OK;
}

NS_METHOD
nsFTPChannel::Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult)
{
    nsFTPChannel* fc = new nsFTPChannel();
    if (fc == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(fc);
    nsresult rv = fc->QueryInterface(aIID, aResult);
    NS_RELEASE(fc);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:

NS_IMETHODIMP
nsFTPChannel::IsPending(PRBool *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFTPChannel::Cancel(void)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFTPChannel::Suspend(void)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFTPChannel::Resume(void)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
// nsIChannel methods:

NS_IMETHODIMP
nsFTPChannel::GetURI(nsIURI * *aURL)
{
    NS_ADDREF(mUrl);
    *aURL = mUrl;
    return NS_OK;
}

// This class 

NS_IMETHODIMP
nsFTPChannel::OpenInputStream(PRUint32 startPosition, PRInt32 readCount,
                              nsIInputStream **_retval)
{
    // The ftp channel will act as the listener which will receive
    // events from the ftp connection thread. It then uses a syncstreamlistener
    // as it's mListener which receives the listener notifications and writes
    // data down the output stream end of a pipe.
    nsresult rv;

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFTPChannel::OpenInputStream() called\n"));

    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    if (!mEventQueue) {
        NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueService, &rv);
        if (NS_FAILED(rv)) return rv;
        rv = eventQService->GetThreadEventQueue(PR_CurrentThread(), &mEventQueue);
        if (NS_FAILED(rv)) return rv;
    }

    rv = serv->NewSyncStreamListener(_retval /* nsIInputStream **inStream */, 
                                     &mBufferOutputStream /* nsIBufferOutputStream **outStream */,
                                     &mListener/* nsIStreamListener **listener */);
    if (NS_FAILED(rv)) return rv;

    // XXX not sure how we should be using these. I suppose we need to use them 
    // XXX in the nsFTPChannel::OnDataAvailable() method.
    mSourceOffset = startPosition;
    mAmount = readCount;

    ////////////////////////////////
    //// setup the channel thread

    nsIThread* workerThread = nsnull;
    nsFtpConnectionThread* protocolInterpreter = 
        new nsFtpConnectionThread(mEventQueue, this, this, nsnull);
    if (!protocolInterpreter)
        return NS_ERROR_OUT_OF_MEMORY;

    protocolInterpreter->Init(mUrl);
    protocolInterpreter->SetUsePasv(PR_TRUE);

    rv = NS_NewThread(&workerThread, protocolInterpreter);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::OpenOutputStream(PRUint32 startPosition, nsIOutputStream **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFTPChannel::AsyncRead(PRUint32 startPosition, PRInt32 readCount,
                        nsISupports *ctxt,
                        nsIStreamListener *listener)
{
    nsresult rv;

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFTPChannel::AsyncRead() called\n"));

    ///////////////////////////
    //// setup channel state

    if (ctxt) {
        mContext = ctxt;
        NS_ADDREF(mContext);
    }

    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    if (!mEventQueue) {
        NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueService, &rv);
        if (NS_FAILED(rv)) return rv;
        rv = eventQService->GetThreadEventQueue(PR_CurrentThread(), &mEventQueue);
        if (NS_FAILED(rv)) return rv;
    }

    rv = serv->NewAsyncStreamListener(listener, mEventQueue, &mListener);
    if (NS_FAILED(rv)) return rv;
    
    rv = NS_NewPipe(&mBufferInputStream, &mBufferOutputStream,
                    nsnull/*this*/, // XXX need channel to implement nsIPipeObserver
                    NS_FTP_SEGMENT_SIZE, NS_FTP_BUFFER_SIZE);
    if (NS_FAILED(rv)) return rv;

    rv = mBufferOutputStream->SetNonBlocking(PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    mSourceOffset = startPosition;
    mAmount = readCount;

    ////////////////////////////////
    //// setup the channel thread

    nsIThread* workerThread = nsnull;
    nsFtpConnectionThread* protocolInterpreter = 
        new nsFtpConnectionThread(mEventQueue, this, this, ctxt);
    NS_ADDREF(protocolInterpreter);

    if (!protocolInterpreter)
        return NS_ERROR_OUT_OF_MEMORY;

    protocolInterpreter->Init(mUrl);
    protocolInterpreter->SetUsePasv(PR_TRUE);

    rv = NS_NewThread(&workerThread, protocolInterpreter);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::AsyncWrite(nsIInputStream *fromStream,
                         PRUint32 startPosition,
                         PRInt32 writeCount,
                         nsISupports *ctxt,
                         nsIStreamObserver *observer)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFTPChannel::GetLoadAttributes(PRUint32 *aLoadAttributes)
{
    *aLoadAttributes = mLoadAttributes;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::SetLoadAttributes(PRUint32 aLoadAttributes)
{
    mLoadAttributes = aLoadAttributes;
    return NS_OK;
}

// FTP does not provide a file typing mechanism. We fallback to file
// extension mapping.

#define FTP_DUMMY_TYPE  "text/html"
NS_IMETHODIMP
nsFTPChannel::GetContentType(char* *aContentType) {
    nsresult rv = NS_OK;

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFTPChannel::GetContentType()\n"));

    // Parameter validation...
    if (!aContentType) {
        return NS_ERROR_NULL_POINTER;
    }
    *aContentType = nsnull;

    if (mContentType.Length()) {
        *aContentType = mContentType.ToNewCString();
        if (!*aContentType) {
            rv = NS_ERROR_OUT_OF_MEMORY;
        }
        PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFTPChannel::NewChannel() returned %s\n", *aContentType));
        return rv;
    }

    NS_WITH_SERVICE(nsIMIMEService, MIMEService, kMIMEServiceCID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = MIMEService->GetTypeFromURI(mUrl, aContentType);
        if (NS_SUCCEEDED(rv)) {
            mContentType = *aContentType;
            PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFTPChannel::NewChannel() returned %s\n", *aContentType));
            return rv;
        }
    }

    // if all else fails treat it as text/html?
	if (!*aContentType) 
		*aContentType = nsCRT::strdup(FTP_DUMMY_TYPE);
    if (!*aContentType) {
        rv = NS_ERROR_OUT_OF_MEMORY;
    } else {
        rv = NS_OK;
    }

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFTPChannel::NewChannel() returned %s\n", *aContentType));

    return rv;
}

NS_IMETHODIMP
nsFTPChannel::GetLoadGroup(nsILoadGroup * *aLoadGroup)
{
    *aLoadGroup = mLoadGroup;
    NS_IF_ADDREF(*aLoadGroup);
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::GetPrincipal(nsIPrincipal * *aPrincipal)
{
    *aPrincipal = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::SetPrincipal(nsIPrincipal * aPrincipal)
{
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIFTPChannel methods:

NS_IMETHODIMP
nsFTPChannel::Get(void) {
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFTPChannel::Put(void) {
    nsresult rv;
    nsIThread* workerThread = nsnull;
    nsFtpConnectionThread* protocolInterpreter = 
        new nsFtpConnectionThread(mEventQueue, this, this, nsnull);
    NS_ASSERTION(protocolInterpreter, "ftp protocol interpreter alloc failed");
    NS_ADDREF(protocolInterpreter);

    if (!protocolInterpreter)
        return NS_ERROR_OUT_OF_MEMORY;

    protocolInterpreter->Init(mUrl);
    protocolInterpreter->SetAction(PUT);
    protocolInterpreter->SetUsePasv(PR_TRUE);

    rv = NS_NewThread(&workerThread, protocolInterpreter);
    NS_ASSERTION(NS_SUCCEEDED(rv), "new thread failed");
    
    // XXX this release should probably be in the destructor.
    NS_RELEASE(protocolInterpreter);
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::SetStreamListener(nsIStreamListener *aListener) {
    mListener = aListener;
    NS_ADDREF(mListener);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIStreamObserver methods:

NS_IMETHODIMP
nsFTPChannel::OnStartRequest(nsIChannel* channel, nsISupports* context) {
    nsresult rv = NS_OK;
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFTPChannel::OnStartRequest(channel = %x, context = %x)\n", channel, context));
    if (mListener) {
        rv = mListener->OnStartRequest(channel, context);
    }
    return rv;
}

NS_IMETHODIMP
nsFTPChannel::OnStopRequest(nsIChannel* channel, nsISupports* context,
                            nsresult aStatus,
                            const PRUnichar* aMsg) {
    nsresult rv = NS_OK;
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFTPChannel::OnStopRequest(channel = %x, context = %x, status = %d, msg = N/A)\n",channel, context, aStatus));
    if (mListener) {
        rv = mListener->OnStopRequest(channel, context, aStatus, aMsg);
    }
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIStreamListener methods:

NS_IMETHODIMP
nsFTPChannel::OnDataAvailable(nsIChannel* channel, nsISupports* context,
                              nsIInputStream *aIStream, 
                              PRUint32 aSourceOffset,
                              PRUint32 aLength) {
    nsresult rv = NS_OK;

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFTPChannel::OnDataAvailable(channel = %x, context = %x, stream = %x, srcOffset = %d, length = %d)\n", channel, context, aIStream, aSourceOffset, aLength));

    nsIFTPContext *ftpCtxt = nsnull;
    rv = context->QueryInterface(nsCOMTypeInfo<nsIFTPContext>::GetIID(), (void**)&ftpCtxt);
    if (NS_FAILED(rv)) return rv;


    char *type = nsnull;
    rv = ftpCtxt->GetContentType(&type);
    if (NS_FAILED(rv)) return rv;

    mContentType = type;
    nsAllocator::Free(type);

    if (mListener) {
        rv = mListener->OnDataAvailable(channel, context, aIStream, aSourceOffset, aLength);
    }
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
