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
#include "prlog.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIBufferInputStream.h"
#include "nsFtpConnectionThread.h"
#include "nsIEventQueueService.h"
#include "nsIProgressEventSink.h"
#include "nsIEventSinkGetter.h"
#include "nsILoadGroup.h"
#include "nsIFTPContext.h"
#include "nsIMIMEService.h"
#include "nsProxyObjectManager.h"


static NS_DEFINE_IID(kProxyObjectManagerCID,        NS_PROXYEVENT_MANAGER_CID);
static NS_DEFINE_CID(kMIMEServiceCID,               NS_MIMESERVICE_CID);
static NS_DEFINE_CID(kEventQueueService,            NS_EVENTQUEUESERVICE_CID);

#if defined(PR_LOGGING)
extern PRLogModuleInfo* gFTPLog;
#endif /* PR_LOGGING */

// There are actually two transport connections established for an 
// ftp connection. One is used for the command channel , and
// the other for the data channel. The command channel is the first
// connection made and is used to negotiate the second, data, channel.
// The data channel is driven by the command channel and is either
// initiated by the server (PORT command) or by the client (PASV command).
// Client initiation is the most command case and is attempted first.

nsFTPChannel::nsFTPChannel() {
    NS_INIT_REFCNT();
    mConnected = PR_FALSE;
    mLoadAttributes = LOAD_NORMAL;
    mSourceOffset = 0;
    mAmount = 0;
    mContentLength = -1;
}

nsFTPChannel::~nsFTPChannel() {
}

NS_IMPL_ISUPPORTS4(nsFTPChannel, nsIChannel, nsIFTPChannel, nsIStreamListener, nsIStreamObserver);

nsresult
nsFTPChannel::Init(const char* verb, nsIURI* uri, nsILoadGroup *aGroup,
                   nsIEventSinkGetter* getter, nsIURI* originalURI,
                   nsIProtocolHandler* aHandler)
{
    nsresult rv;

    if (mConnected)
        return NS_ERROR_FAILURE;

    mHandler = aHandler;

    mOriginalURI = originalURI ? originalURI : uri;
    mURL = uri;

    mLoadGroup = aGroup;

    if (getter) {
        rv = getter->GetEventSink(verb, NS_GET_IID(nsIProgressEventSink), 
                                  (nsISupports**)&mEventSink);
        if (NS_FAILED(rv)) {
            PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFTPChannel::Init() (couldn't find event sink)\n"));
        }
    }

    NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueService, &rv);
    if (NS_FAILED(rv)) return rv;

    // the FTP channel is instanciated on the main thread. the actual
    // protocol is interpreted on the FTP thread.
    rv = eventQService->GetThreadEventQueue(PR_CurrentThread(), getter_AddRefs(mEventQueue));
    if (NS_FAILED(rv)) return rv;

    // go ahead and create the thread for the connection.
    // we'll init it and kick it off later
    rv = NS_NewThread(getter_AddRefs(mConnectionThread));
    if (NS_FAILED(rv)) return rv;

    // we'll create the FTP connection's event queue here, on this thread.
    // it will be passed to the FTP connection upon FTP connection 
    // initialization. at that point it's up to the FTP conn thread to
    // turn the crank on it.
#if 1
    PRThread *thread; // does not need deleting
    rv = mConnectionThread->GetPRThread(&thread);
    if (NS_FAILED(rv)) return rv;

    PLEventQueue* PLEventQ = PL_CreateEventQueue("FTP thread", thread);
    if (!PLEventQ) return rv;

    rv = eventQService->CreateFromPLEventQueue(PLEventQ, getter_AddRefs(mConnectionEventQueue));
#else
    rv = eventQService->CreateFromIThread(mConnectionThread, getter_AddRefs(mConnectionEventQueue));
#endif

    return rv;
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

// The FTP channel doesn't maintain any connection state. Nor does it
// interpret the protocol. The FTP connection thread is responsible for
// these things and thus, the FTP channel simply calls through to the 
// FTP connection thread using an xpcom proxy object to make the
// cross thread call.

NS_IMETHODIMP
nsFTPChannel::IsPending(PRBool *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFTPChannel::Cancel(void)
{
    nsresult rv = NS_OK;
    if (mThreadRequest)
        rv = mThreadRequest->Cancel();
    return rv;
}

NS_IMETHODIMP
nsFTPChannel::Suspend(void)
{
    nsresult rv = NS_OK;
    if (mThreadRequest)
        rv = mThreadRequest->Suspend();
    return rv;
}

NS_IMETHODIMP
nsFTPChannel::Resume(void)
{
    nsresult rv = NS_OK;
    if (mThreadRequest)
        rv = mThreadRequest->Resume();
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIChannel methods:

NS_IMETHODIMP
nsFTPChannel::GetOriginalURI(nsIURI * *aURL)
{
    *aURL = mOriginalURI;
    NS_ADDREF(*aURL);
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::GetURI(nsIURI * *aURL)
{
    *aURL = mURL;
    NS_ADDREF(*aURL);
    return NS_OK;
}

// This class 

NS_IMETHODIMP
nsFTPChannel::OpenInputStream(PRUint32 startPosition, PRInt32 readCount,
                              nsIInputStream **_retval)
{
#if 0
    // The ftp channel will act as the listener which will receive
    // events from the ftp connection thread. It then uses a syncstreamlistener
    // as it's mListener which receives the listener notifications and writes
    // data down the output stream end of a pipe.
    nsresult rv;

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFTPChannel::OpenInputStream() called\n"));

    rv = serv->NewSyncStreamListener(_retval /* nsIInputStream **inStream */, 
                                     &mBufferOutputStream /* nsIBufferOutputStream **outStream */,
                                     &mListener/* nsIStreamListener **listener */);
    if (NS_FAILED(rv)) return rv;

    mSourceOffset = startPosition;
    mAmount = readCount;

    ////////////////////////////////
    //// setup the channel thread

    nsIThread* workerThread = nsnull;
    nsFtpConnectionThread* protocolInterpreter = 
        new nsFtpConnectionThread(mEventQueue, this, this, nsnull);
    if (!protocolInterpreter)
        return NS_ERROR_OUT_OF_MEMORY;

    protocolInterpreter->Init(mURL);
    protocolInterpreter->SetUsePasv(PR_TRUE);

    rv = NS_NewThread(&workerThread, protocolInterpreter);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
#endif // 0
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFTPChannel::OpenOutputStream(PRUint32 startPosition, nsIOutputStream **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFTPChannel::AsyncOpen(nsIStreamObserver *observer, nsISupports* ctxt)
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

    // XXX we should be using these. esp. for FTP restart.
    mSourceOffset = startPosition;
    mAmount = readCount;

    mContext = ctxt;

    mListener = listener;

    mSourceOffset = startPosition;
    mAmount = readCount;

    ////////////////////////////////
    //// setup the channel thread
    nsFtpConnectionThread* protocolInterpreter;
    NS_NEWXPCOM(protocolInterpreter, nsFtpConnectionThread);
    if (!protocolInterpreter) return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(protocolInterpreter);

    rv = protocolInterpreter->Init(mConnectionEventQueue,   /* FTP thread queue */
                                   mURL,                    /* url to load */
                                   mEventQueue,             /* event queue for this thread */
                                   mHandler,
                                   this, ctxt);
    if (NS_FAILED(rv)) return rv;

    // create the proxy object so we can call into the FTP thread.
    NS_WITH_SERVICE(nsIProxyObjectManager, proxyManager, kProxyObjectManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = proxyManager->GetProxyObject(mConnectionEventQueue,
                                      NS_GET_IID(nsIRequest),
                                      (nsISupports*)(nsIRequest*)protocolInterpreter,
                                      PROXY_SYNC | PROXY_ALWAYS,
                                      getter_AddRefs(mThreadRequest));
    if (NS_FAILED(rv)) return rv;

    rv = mConnectionThread->Init((nsIRunnable*)protocolInterpreter,
                                 0, /* stack size */
                                 PR_PRIORITY_NORMAL,
                                 PR_GLOBAL_THREAD,
                                 PR_UNJOINABLE_THREAD);

    // this extra release is a result of a discussion with 
    // dougt. GetProxyObject is doing an extra addref. dougt
    // can best explain why. If this is suddenly an *extra* 
    // release, yank it.
    NS_RELEASE2(protocolInterpreter, rv);
    NS_RELEASE(protocolInterpreter);
    mConnectionThread = 0; // this is necessary because there is a circular dependency
                           // between the FTPChannel and the connection thread.
                           // we need to ditch our ref to the connection thread asap.
    if (NS_FAILED(rv)) return rv;

    mConnected = PR_TRUE;

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
        rv = MIMEService->GetTypeFromURI(mURL, aContentType);
        if (NS_SUCCEEDED(rv)) {
            mContentType = *aContentType;
            PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFTPChannel::NewChannel() returned %s\n", *aContentType));
            return rv;
        }
    }

    // if all else fails treat it as unknown?
	if (!*aContentType) 
		*aContentType = nsCRT::strdup("application/x-unknown-content-type");
    if (!*aContentType) {
        rv = NS_ERROR_OUT_OF_MEMORY;
    } else {
        rv = NS_OK;
    }

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFTPChannel::GetContentType() returned %s\n", *aContentType));

    return rv;
}


NS_IMETHODIMP
nsFTPChannel::GetContentLength(PRInt32 *aContentLength)
{
    *aContentLength = mContentLength;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::GetLoadGroup(nsILoadGroup * *aLoadGroup)
{
    *aLoadGroup = mLoadGroup;
    NS_IF_ADDREF(*aLoadGroup);
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::GetOwner(nsISupports * *aOwner)
{
    *aOwner = mOwner.get();
    NS_IF_ADDREF(*aOwner);
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::SetOwner(nsISupports * aOwner)
{
    mOwner = aOwner;
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
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFTPChannel::SetStreamListener(nsIStreamListener *aListener) {
    mListener = aListener;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIStreamObserver methods:

NS_IMETHODIMP
nsFTPChannel::OnStartRequest(nsIChannel* channel, nsISupports* context) {
    nsresult rv = NS_OK;
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFTPChannel::OnStartRequest(channel = %x, context = %x)\n", channel, context));
    if (mListener) {
        rv = mListener->OnStartRequest(channel, mContext);
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
        rv = mListener->OnStopRequest(channel, mContext, aStatus, aMsg);
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

    if (context) {
        nsCOMPtr<nsIFTPContext> ftpCtxt = do_QueryInterface(context, &rv);
        if (NS_FAILED(rv)) return rv;

        char *type = nsnull;
        rv = ftpCtxt->GetContentType(&type);
        if (NS_FAILED(rv)) return rv;

        nsCAutoString cType(type);
        cType.ToLowerCase();
        mContentType = cType.GetBuffer();
        nsAllocator::Free(type);

        rv = ftpCtxt->GetContentLength(&mContentLength);
        if (NS_FAILED(rv)) return rv;
    }
    
    if (mListener) {
        rv = mListener->OnDataAvailable(channel, mContext, aIStream, aSourceOffset, aLength);
    }
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
