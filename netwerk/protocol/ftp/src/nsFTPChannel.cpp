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
#include "nsIStreamListener.h"
#include "nsIServiceManager.h"
#include "nsIMIMEService.h"
#include "nsNetUtil.h"
#include "nsMimeTypes.h"

static NS_DEFINE_CID(kMIMEServiceCID,               NS_MIMESERVICE_CID);

#if defined(PR_LOGGING)
extern PRLogModuleInfo* gFTPLog;
#endif /* PR_LOGGING */

// There are two transport connections established for an 
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
    mLock = nsnull;
}

nsFTPChannel::~nsFTPChannel() {
    nsXPIDLCString spec;
    mURL->GetSpec(getter_Copies(spec));
    PR_LOG(gFTPLog, PR_LOG_ALWAYS, ("~nsFTPChannel() for %s", (const char*)spec));
    if (mLock) PR_DestroyLock(mLock);
}

NS_IMPL_THREADSAFE_ADDREF(nsFTPChannel);
NS_IMPL_THREADSAFE_RELEASE(nsFTPChannel);
NS_IMPL_QUERY_INTERFACE7(nsFTPChannel,
                         nsPIFTPChannel, 
                         nsIChannel, 
                         nsIRequest, 
                         nsIInterfaceRequestor, 
                         nsIProgressEventSink,
                         nsIStreamListener,
                         nsIStreamObserver);

nsresult
nsFTPChannel::Init(const char* verb, 
                   nsIURI* uri, 
                   nsILoadGroup* aLoadGroup,
                   nsIInterfaceRequestor* notificationCallbacks, 
                   nsLoadFlags loadAttributes, 
                   nsIURI* originalURI,
                   PRUint32 bufferSegmentSize,
                   PRUint32 bufferMaxSize,
                   nsIProtocolHandler* aHandler, 
                   nsIThreadPool* aPool)
{
    nsresult rv = NS_OK;

    if (mConnected) {
        PR_LOG(gFTPLog, PR_LOG_DEBUG, ("ERROR nsFTPChannel:Init called while connected.\n"));
        return NS_ERROR_ALREADY_CONNECTED;
    }

    // setup channel state
    mHandler = aHandler;
    NS_ASSERTION(aPool, "FTP channel needs a thread pool to play in");
    if (!aPool) return NS_ERROR_NULL_POINTER;
    mPool = aPool;
    mOriginalURI = originalURI ? originalURI : uri;
    mURL = uri;

    rv = mURL->GetHost(getter_Copies(mHost));
    if (NS_FAILED(rv)) return rv;

    rv = SetLoadAttributes(loadAttributes);
    if (NS_FAILED(rv)) return rv;

    rv = SetLoadGroup(aLoadGroup);
    if (NS_FAILED(rv)) return rv;

    rv = SetNotificationCallbacks(notificationCallbacks);
    if (NS_FAILED(rv)) return rv;

    mBufferSegmentSize = bufferSegmentSize;
    mBufferMaxSize = bufferMaxSize;

    NS_ASSERTION(!mLock, "Init should only be called once on a channel");
    mLock = PR_NewLock();
    if (!mLock) return NS_ERROR_OUT_OF_MEMORY;
    return rv;
}

nsresult
nsFTPChannel::SetProxyChannel(nsIChannel *aChannel) {
    if (mConnected) return NS_ERROR_ALREADY_CONNECTED;
    mProxyChannel = aChannel;
    return NS_OK;
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
nsFTPChannel::IsPending(PRBool *result) {
    nsAutoLock lock(mLock);
    if (mProxyChannel)
        return mProxyChannel->IsPending(result);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFTPChannel::Cancel(void) {
    nsresult rv;
    nsAutoLock lock(mLock);
    if (mProxyChannel) {
        return mProxyChannel->Cancel();
    } else if (mConnThread) {
        rv = mConnThread->Cancel();
        mConnThread = nsnull;
        mConnected = PR_FALSE;
        return rv;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::Suspend(void) {
    nsAutoLock lock(mLock);
    if (mProxyChannel) {
        return mProxyChannel->Suspend();
    } else if (mConnThread) {
        return mConnThread->Suspend();
    }
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::Resume(void) {
    nsAutoLock lock(mLock);
    if (mProxyChannel) {
        return mProxyChannel->Resume();
    } else if (mConnThread) {
        return mConnThread->Resume();
    }
    return NS_OK;
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

    if (mProxyChannel) {
        return mProxyChannel->OpenInputStream(startPosition, readCount, _retval);
    }

    if (mConnected) {
        PR_LOG(gFTPLog, PR_LOG_DEBUG, ("ERROR nsFTPChannel:OpenInputStream called while connected.\n"));
        return NS_ERROR_ALREADY_CONNECTED;
    }
    
    // create a pipe. The caller gets the input stream end,
    // and the FTP thread get's the output stream end.
    // The FTP thread will write to the output stream end
    // when data become available to it.
    nsCOMPtr<nsIBufferOutputStream> bufOutStream; // we don't use this piece
    nsCOMPtr<nsIStreamListener>     listener;
    rv = NS_NewSyncStreamListener(_retval, getter_AddRefs(bufOutStream),
                                  getter_AddRefs(listener));
    if (NS_FAILED(rv)) return rv;

    mListener = listener; // ensure that we insert ourselves as the proxy listener

    ///////////////////////////
    //// setup channel state
    mSourceOffset = startPosition;
    mAmount = readCount;

    ////////////////////////////////
    //// setup the channel thread
    nsFtpConnectionThread *thread = nsnull;
    NS_NEWXPCOM(thread, nsFtpConnectionThread);
    if (!thread) return NS_ERROR_OUT_OF_MEMORY;
    mConnThread = thread;

    rv = thread->Init(mHandler, this, mBufferSegmentSize, mBufferMaxSize);
    mHandler = 0;
    if (NS_FAILED(rv)) return rv;

    rv = thread->SetStreamListener(this, nsnull);
    if (NS_FAILED(rv)) return rv;

    if (mLoadGroup) {
        rv = mLoadGroup->AddChannel(this, nsnull);
        if (NS_FAILED(rv)) return rv;
    }

    mConnected = PR_TRUE;
    return mPool->DispatchRequest((nsIRunnable*)thread);
}

NS_IMETHODIMP
nsFTPChannel::OpenOutputStream(PRUint32 startPosition, nsIOutputStream **_retval)
{
    if (mProxyChannel)
        return mProxyChannel->OpenOutputStream(startPosition, _retval);
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

    mListener = listener;
    mUserContext = ctxt;

    if (mEventSink) {
        nsAutoString statusMsg("Beginning FTP transaction.");
        rv = mEventSink->OnStatus(this, ctxt, statusMsg.GetUnicode());
        if (NS_FAILED(rv)) return rv;
    }

    ///////////////////////////
    //// setup channel state
    mSourceOffset = startPosition;
    mAmount = readCount;

    if (mLoadGroup) {
        rv = mLoadGroup->AddChannel(this, nsnull);
        if (NS_FAILED(rv)) return rv;
    }

    if (mProxyChannel) {
        return mProxyChannel->AsyncRead(startPosition, readCount, ctxt, this);
    }

    if (mConnected) {
        PR_LOG(gFTPLog, PR_LOG_DEBUG, ("ERROR nsFTPChannel:AsyncRead called while connected.\n"));
        return NS_ERROR_ALREADY_CONNECTED;
    }

    ////////////////////////////////
    //// setup the channel thread
    nsFtpConnectionThread *thread = nsnull;
    NS_NEWXPCOM(thread, nsFtpConnectionThread);
    if (!thread) return NS_ERROR_OUT_OF_MEMORY;
    mConnThread = thread;

    rv = thread->Init(mHandler, this, mBufferSegmentSize, mBufferMaxSize);
    mHandler = nsnull;
    if (NS_FAILED(rv)) return rv;

    rv = thread->SetStreamListener(this, ctxt);
    if (NS_FAILED(rv)) return rv;

    mConnected = PR_TRUE;

    return mPool->DispatchRequest((nsIRunnable*)thread);
}

NS_IMETHODIMP
nsFTPChannel::AsyncWrite(nsIInputStream *fromStream,
                         PRUint32 startPosition,
                         PRInt32 writeCount,
                         nsISupports *ctxt,
                         nsIStreamObserver *observer)
{
    nsresult rv = NS_OK;

    mObserver = observer;
    mUserContext = ctxt;

    if (mProxyChannel) {
        return mProxyChannel->AsyncWrite(fromStream, startPosition,
                                         writeCount, ctxt, observer);
    }

    NS_ASSERTION(writeCount > 0, "FTP requires stream len info");
    if (writeCount < 1) return NS_ERROR_NOT_INITIALIZED;

    nsFtpConnectionThread *thread = nsnull;
    NS_NEWXPCOM(thread, nsFtpConnectionThread);
    if (!thread) return NS_ERROR_OUT_OF_MEMORY;
    mConnThread = thread;

    rv = thread->Init(mHandler, this, mBufferSegmentSize, mBufferMaxSize);
    mHandler = 0;
    if (NS_FAILED(rv)) return rv;

    rv = thread->SetWriteStream(fromStream, writeCount);
    if (NS_FAILED(rv)) return rv;

    rv = thread->SetStreamObserver(this, ctxt);
    if (NS_FAILED(rv)) return rv;

    if (mLoadGroup) {
        rv = mLoadGroup->AddChannel(this, nsnull);
        if (NS_FAILED(rv)) return rv;
    }

    mConnected = PR_TRUE;

    return mPool->DispatchRequest((nsIRunnable*)thread);
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

    if (!aContentType) return NS_ERROR_NULL_POINTER;

    nsAutoLock lock(mLock);
    *aContentType = nsnull;
    if (mContentType.IsEmpty()) {
        NS_WITH_SERVICE(nsIMIMEService, MIMEService, kMIMEServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;
        rv = MIMEService->GetTypeFromURI(mURL, aContentType);
        if (NS_SUCCEEDED(rv)) {
            mContentType = *aContentType;
        } else {
            mContentType = UNKNOWN_CONTENT_TYPE;
            rv = NS_OK;
        }
    }

    if (!*aContentType) {
        *aContentType = mContentType.ToNewCString();
    }

    if (!*aContentType) return NS_ERROR_OUT_OF_MEMORY;
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFTPChannel::GetContentType() returned %s\n", *aContentType));
    return rv;
}

NS_IMETHODIMP
nsFTPChannel::SetContentType(const char *aContentType)
{
    nsAutoLock lock(mLock);
    mContentType = aContentType;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::GetContentLength(PRInt32 *aContentLength)
{
    nsAutoLock lock(mLock);
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

NS_IMETHODIMP 
nsFTPChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
    *aSecurityInfo = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::SetContentLength(PRInt32 aLength) {
    nsAutoLock lock(mLock);
    mContentLength = aLength;
    return NS_OK;
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
    if (mProxyChannel) {
        // XXX We're appending the *real* host to this string
        // XXX coming from the proxy channel progress notifications.
        // XXX This assumes the proxy channel was setup using a null host name
        nsAutoString msg(aMsg);
        msg += (const char*)mHost;
        return mEventSink ? mEventSink->OnStatus(this, aContext, msg.GetUnicode()) : NS_OK;
    } else {
        return mEventSink ? mEventSink->OnStatus(this, aContext, aMsg) : NS_OK;
    }
}

NS_IMETHODIMP
nsFTPChannel::OnProgress(nsIChannel* aChannel, nsISupports* aContext,
                                  PRUint32 aProgress, PRUint32 aProgressMax) {
    return mEventSink ? mEventSink->OnProgress(this, aContext, aProgress, aProgressMax) : NS_OK;
}


// nsIStreamObserver methods.
NS_IMETHODIMP
nsFTPChannel::OnStopRequest(nsIChannel* aChannel, nsISupports* aContext,
                            nsresult aStatus, const PRUnichar* aMsg) {
    nsresult rv = NS_OK;
    mConnThread = nsnull;

    if (mLoadGroup) {
        rv = mLoadGroup->RemoveChannel(this, nsnull, aStatus, aMsg);
        if (NS_FAILED(rv)) return rv;
    }
    
    if (mObserver) {
        rv = mObserver->OnStopRequest(this, aContext, aStatus, aMsg);
        if (NS_FAILED(rv)) return rv;
    }

    if (mListener) {
        rv = mListener->OnStopRequest(this, aContext, aStatus, aMsg);
        if (NS_FAILED(rv)) return rv;
    }
    return rv;
}

NS_IMETHODIMP
nsFTPChannel::OnStartRequest(nsIChannel *aChannel, nsISupports *aContext) {
    nsresult rv = NS_OK;
    if (mObserver) {
        rv = mObserver->OnStartRequest(this, aContext);
        if (NS_FAILED(rv)) return rv;
    }

    if (mListener) {
        rv = mListener->OnStartRequest(this, aContext);
        if (NS_FAILED(rv)) return rv;
    }
    return rv;
}


// nsIStreamListener method
NS_IMETHODIMP
nsFTPChannel::OnDataAvailable(nsIChannel* aChannel, nsISupports* aContext,
                               nsIInputStream *aInputStream, PRUint32 aSourceOffset,
                               PRUint32 aLength) {
    return mListener->OnDataAvailable(this, aContext, aInputStream, aSourceOffset, aLength);
}
