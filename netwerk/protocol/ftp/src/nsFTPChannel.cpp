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

// ftp implementation

#include "nsFTPChannel.h"
#include "nscore.h"
#include "prlog.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsFtpConnectionThread.h"
#include "nsIEventQueueService.h"
#include "nsIProgressEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsIMIMEService.h"
#include "nsProxyObjectManager.h"
#include "nsIPipe.h"
#include "nsAutoLock.h"

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
    mConnThread = nsnull;
    mAsyncOpen = PR_FALSE;
}

nsFTPChannel::~nsFTPChannel() {
    NS_ASSERTION(!mConnThread, "FTP: connection thread ref still exists");
    PR_LOG(gFTPLog, PR_LOG_ALWAYS, ("~nsFTPChannel() called"));
}

NS_IMPL_ISUPPORTS5(nsFTPChannel, nsPIFTPChannel, nsIChannel, nsIRequest, nsIInterfaceRequestor, nsIProgressEventSink);

nsresult
nsFTPChannel::Init(const char* verb, 
                   nsIURI* uri, 
                   nsILoadGroup* aLoadGroup,
                   nsIInterfaceRequestor* notificationCallbacks, 
                   nsLoadFlags loadAttributes, 
                   nsIURI* originalURI,
                   nsIProtocolHandler* aHandler, 
                   nsIThreadPool* aPool)
{
    nsresult rv = NS_OK;

    if (mConnected)
        return NS_ERROR_ALREADY_CONNECTED;

    // parameter validation
    NS_ASSERTION(aPool, "FTP: channel needs a thread pool to play in");

    // setup channel state
    mHandler = aHandler;
    NS_ASSERTION(aPool, "FTP channel needs a thread pool to play in");
    if (!aPool) return NS_ERROR_NULL_POINTER;
    mPool = aPool;
    mOriginalURI = originalURI ? originalURI : uri;
    mURL = uri;

    rv = SetLoadAttributes(loadAttributes);
    if (NS_FAILED(rv)) return rv;

    rv = SetLoadGroup(aLoadGroup);
    if (NS_FAILED(rv)) return rv;

    rv = SetNotificationCallbacks(notificationCallbacks);
    if (NS_FAILED(rv)) return rv;

    return rv;
}

NS_METHOD
nsFTPChannel::Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult)
{
    nsFTPChannel* fc;
    NS_NEWXPCOM(fc, nsFTPChannel);
    if (!fc) return NS_ERROR_OUT_OF_MEMORY;
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
    if (mProxiedThreadRequest)
        rv = mProxiedThreadRequest->Cancel();
    return rv;
}

NS_IMETHODIMP
nsFTPChannel::Suspend(void)
{
    nsresult rv = NS_OK;
    if (mProxiedThreadRequest)
        rv = mProxiedThreadRequest->Suspend();
    return rv;
}

NS_IMETHODIMP
nsFTPChannel::Resume(void)
{
    // we can't use the mProxiedThreadRequest to resume the thread
    // because it needs the event pump to be running on the thread
    // (which is blocking right now). Instead wake it up directly
    // with a monitor notification.
    NS_ASSERTION(mConnThread, "FTP: connection thread was dropped");
    nsAutoCMonitor mon(mConnThread);
    return mon.Notify();
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

NS_IMETHODIMP
nsFTPChannel::OpenInputStream(PRUint32 startPosition, PRInt32 readCount,
                              nsIInputStream **_retval)
{
    nsresult rv = NS_OK;

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFTPChannel::OpenInputStream() called\n"));

    if (mConnected) return NS_ERROR_ALREADY_CONNECTED;
    
    // create a pipe. The caller gets the input stream end,
    // and the FTP thread get's the output stream end.
    // The FTP thread will write to the output stream end
    // when data become available to it.
    nsCOMPtr<nsIBufferOutputStream> bufOutStream;
    nsCOMPtr<nsIBufferInputStream>  bufInStream;
    rv = NS_NewPipe(getter_AddRefs(bufInStream), getter_AddRefs(bufOutStream));
    if (NS_FAILED(rv)) return rv;

    *_retval = NS_STATIC_CAST(nsIInputStream*, bufInStream.get());
    NS_ADDREF(*_retval);

    ///////////////////////////
    //// setup channel state
    mSourceOffset = startPosition;
    mAmount = readCount;

    ////////////////////////////////
    //// setup the channel thread
    NS_NEWXPCOM(mConnThread, nsFtpConnectionThread);
    if (!mConnThread) return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(mConnThread); // keep our own ref to the thread obj (we'll 
                            // release it later in this same call.

    rv = mConnThread->Init(mHandler, this, nsnull);
    mHandler = 0;
    if (NS_FAILED(rv)) {
        NS_RELEASE(mConnThread);
        return rv;
    }

    rv = mConnThread->SetOutputStream(bufOutStream);
    if (NS_FAILED(rv)) {
        NS_RELEASE(mConnThread);
        return rv;
    }

    mThreadRequest = do_QueryInterface((nsISupports*)(nsIRequest*)mConnThread);

    rv = mPool->DispatchRequest((nsIRunnable*)mConnThread);

    NS_RELEASE(mConnThread); // we're done w/ the raw thread obj
    if (NS_FAILED(rv)) return rv;

    mConnected = PR_TRUE;

    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::OpenOutputStream(PRUint32 startPosition, nsIOutputStream **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFTPChannel::AsyncOpen(nsIStreamObserver *observer, nsISupports* ctxt)
{
    nsresult rv;

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFTPChannel::AsyncOpen() called\n"));

    if (mConnected) return NS_ERROR_ALREADY_CONNECTED;

    ////////////////////////////////
    //// setup the channel thread
    NS_NEWXPCOM(mConnThread, nsFtpConnectionThread);
    if (!mConnThread) return NS_ERROR_OUT_OF_MEMORY;

    // becuase we want to use the actual thread object as a monitor variable
    // we need to hang on to it until the thread itself tells us it's invalid.
    // See nsFTPChannel::Stopped().
    NS_ADDREF(mConnThread); // keep our own ref to the thread obj

    mThreadRequest = do_QueryInterface((nsISupports*)(nsIRequest*)mConnThread);

    rv = mConnThread->Init(mHandler, this, ctxt);
    mHandler = 0;
    if (NS_FAILED(rv)) {
        NS_RELEASE(mConnThread);
        return rv;
    }

    rv = mConnThread->SetStreamObserver(observer, ctxt);
    if (NS_FAILED(rv)) return rv;

    mConnected = PR_TRUE;
    mAsyncOpen = PR_TRUE;

    // start the run
    return mPool->DispatchRequest((nsIRunnable*)mConnThread);
}

NS_IMETHODIMP
nsFTPChannel::AsyncRead(PRUint32 startPosition, PRInt32 readCount,
                        nsISupports *ctxt,
                        nsIStreamListener *listener)
{
    nsresult rv;

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFTPChannel::AsyncRead() called\n"));

    if (mEventSink) {
        nsAutoString statusMsg("Beginning FTP transaction.");
#ifndef BUG_16273_FIXED //TODO
        rv = mEventSink->OnStatus(this, ctxt, statusMsg.ToNewUnicode());
#else
        rv = mEventSink->OnStatus(this, ctxt, statusMsg.GetUnicode());
#endif
        if (NS_FAILED(rv)) return rv;
    }

    ///////////////////////////
    //// setup channel state
    mSourceOffset = startPosition;
    mAmount = readCount;

    if (mAsyncOpen) {
        NS_ASSERTION(mConnThread, "FTP: underlying connection thread went away");
        // we already initialized the connection thread via a prior call
        // to AsyncOpen().
        
        // The connection thread is suspended right now.
        // Set our AsyncRead pertinent state and then wake it up.
        rv = mConnThread->SetStreamListener(listener);
        if (NS_FAILED(rv)) return rv;

        nsAutoCMonitor mon(mConnThread);
        mon.Notify();
    } else {

        if (mConnected) return NS_ERROR_ALREADY_CONNECTED;

        ////////////////////////////////
        //// setup the channel thread
        NS_NEWXPCOM(mConnThread, nsFtpConnectionThread);
        if (!mConnThread) return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(mConnThread);

        mThreadRequest = do_QueryInterface((nsISupports*)(nsIRequest*)mConnThread);

        rv = mConnThread->Init(mHandler, this, ctxt);
        mHandler = 0;
        if (NS_FAILED(rv)) {
            NS_RELEASE(mConnThread);
            return rv;
        }

        rv = mConnThread->SetStreamListener(listener);
        if (NS_FAILED(rv)) return rv;

        rv = mPool->DispatchRequest((nsIRunnable*)mConnThread);

        NS_RELEASE(mConnThread);
        if (NS_FAILED(rv)) return rv;

        mConnected = PR_TRUE;
    }

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
        PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFTPChannel::GetContentType() returned %s\n", *aContentType));
        return rv;
    }

    NS_WITH_SERVICE(nsIMIMEService, MIMEService, kMIMEServiceCID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = MIMEService->GetTypeFromURI(mURL, aContentType);
        if (NS_SUCCEEDED(rv)) {
            mContentType = *aContentType;
            PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFTPChannel::GetContentType() returned %s\n", *aContentType));
            return rv;
        }
    }

    // if all else fails treat it as unknown?
    // XXX this will go away once we have AsyncOpen in place.
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
nsFTPChannel::GetLoadGroup(nsILoadGroup* *aLoadGroup)
{
    *aLoadGroup = mLoadGroup;
    NS_IF_ADDREF(*aLoadGroup);
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
    mLoadGroup = aLoadGroup;
    if (mLoadGroup) {
        nsresult rv = mLoadGroup->AddChannel(this, nsnull);
        if (NS_FAILED(rv)) return rv;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::GetOwner(nsISupports* *aOwner)
{
    *aOwner = mOwner.get();
    NS_IF_ADDREF(*aOwner);
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::SetOwner(nsISupports* aOwner)
{
    mOwner = aOwner;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::GetNotificationCallbacks(nsIInterfaceRequestor* *aNotificationCallbacks)
{
    *aNotificationCallbacks = mCallbacks.get();
    NS_IF_ADDREF(*aNotificationCallbacks);
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::SetNotificationCallbacks(nsIInterfaceRequestor* aNotificationCallbacks)
{
    mCallbacks = aNotificationCallbacks;

    if (mCallbacks) {
        nsresult rv = mCallbacks->GetInterface(NS_GET_IID(nsIProgressEventSink), 
                                               getter_AddRefs(mEventSink));
        if (NS_FAILED(rv)) {
            PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFTPChannel::Init() (couldn't find event sink)\n"));
        }
    }
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsPIFTPChannel methods:
NS_IMETHODIMP
nsFTPChannel::SetConnectionQueue(nsIEventQueue *aEventQ) {
    nsresult rv = NS_OK;

    if (aEventQ) {
        // create the proxy object so we can call into the FTP thread.
        NS_WITH_SERVICE(nsIProxyObjectManager, proxyManager, kProxyObjectManagerCID, &rv);
        if (NS_FAILED(rv)) return rv;

        // change the thread request over to a proxy thread request.
        rv = proxyManager->GetProxyObject(aEventQ,
                                          NS_GET_IID(nsIRequest),
                                          mThreadRequest,
                                          PROXY_ASYNC | PROXY_ALWAYS,
                                          getter_AddRefs(mProxiedThreadRequest));
    }
    mThreadRequest = 0;

    return rv;
}

NS_IMETHODIMP
nsFTPChannel::SetContentLength(PRInt32 aLength) {
    mContentLength = aLength;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::SetContentType(const char *aContentType) {
    mContentType = aContentType;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::Stopped(nsresult aStatus, const PRUnichar *aMsg) {
    nsresult rv = NS_OK;
    // the underlying connection thread has gone away.
    mConnected = PR_FALSE;
    mThreadRequest = 0;
    mProxiedThreadRequest = 0;
    NS_IF_RELEASE(mConnThread);
    if (mLoadGroup)
        rv = mLoadGroup->RemoveChannel(this, nsnull, aStatus, aMsg);

    return rv;
}

// nsIInterfaceRequestor method
NS_IMETHODIMP
nsFTPChannel::GetInterface(const nsIID &anIID, void **aResult ) {
    // capture the progress event sink stuff. pass the rest through.
    if (anIID.Equals(NS_GET_IID(nsIProgressEventSink))) {
        *aResult = NS_STATIC_CAST(nsIProgressEventSink*, this);
        NS_ADDREF(this);
        return NS_OK;
    } else {
        return mCallbacks ? mCallbacks->GetInterface(anIID, aResult) : NS_ERROR_NO_INTERFACE;
    }
}


// nsIProgressEventSink methods
NS_IMETHODIMP
nsFTPChannel::OnStatus(nsIChannel *aChannel,
                                nsISupports *aContext,
                                const PRUnichar *aMsg) {
    return mEventSink ? mEventSink->OnStatus(this, aContext, aMsg) : NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::OnProgress(nsIChannel* aChannel, nsISupports* aContext,
                                  PRUint32 aProgress, PRUint32 aProgressMax) {
    return mEventSink ? mEventSink->OnProgress(this, aContext, aProgress, aProgressMax) : NS_OK;
}
