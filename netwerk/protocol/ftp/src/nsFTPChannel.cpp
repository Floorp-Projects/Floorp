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

static NS_DEFINE_CID(kMIMEServiceCID, NS_MIMESERVICE_CID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
 
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

nsFTPChannel::nsFTPChannel()
    : mConnected(PR_FALSE),
      mLoadAttributes(LOAD_NORMAL),
      mSourceOffset(0),
      mAmount(0),
      mContentLength(-1),
      mBufferSegmentSize(0),
      mBufferMaxSize(0),
      mLock(nsnull),
      mStatus(NS_OK),
      mProxyPort(-1),
      mProxyTransparent(PR_FALSE)
{
    NS_INIT_REFCNT();
}

nsFTPChannel::~nsFTPChannel()
{
    nsXPIDLCString spec;
    mURL->GetSpec(getter_Copies(spec));
    PR_LOG(gFTPLog, PR_LOG_ALWAYS, ("~nsFTPChannel() for %s", (const char*)spec));
    if (mLock) PR_DestroyLock(mLock);
}

NS_IMPL_THREADSAFE_ISUPPORTS8(nsFTPChannel,
                              nsIChannel,
                              nsIFTPChannel,
                              nsIProxy,
                              nsIRequest, 
                              nsIInterfaceRequestor, 
                              nsIProgressEventSink,
                              nsIStreamListener,
                              nsIStreamObserver);

nsresult
nsFTPChannel::Init(nsIURI* uri, 
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
    mURL = uri;

    rv = mURL->GetHost(getter_Copies(mHost));
    if (NS_FAILED(rv)) return rv;

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
nsFTPChannel::GetName(PRUnichar* *result)
{
    if (mProxyChannel)
        return mProxyChannel->GetName(result);
    nsresult rv;
    nsXPIDLCString urlStr;
    rv = mURL->GetSpec(getter_Copies(urlStr));
    if (NS_FAILED(rv)) return rv;
    nsString name;
    name.AppendWithConversion(urlStr);
    *result = name.ToNewUnicode();
    return *result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsFTPChannel::IsPending(PRBool *result) {
    nsAutoLock lock(mLock);
    if (mProxyChannel)
        return mProxyChannel->IsPending(result);
    NS_NOTREACHED("nsFTPChannel::IsPending");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFTPChannel::GetStatus(nsresult *status)
{
    *status = mStatus;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::Cancel(nsresult status) {
    NS_ASSERTION(NS_FAILED(status), "shouldn't cancel with a success code");
    nsresult rv;
    nsAutoLock lock(mLock);
    mStatus = status;
    if (mProxyChannel) {
        return mProxyChannel->Cancel(status);
    } else if (mConnThread) {
        rv = mConnThread->Cancel(status);
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
nsFTPChannel::GetOriginalURI(nsIURI* *aURL)
{
    *aURL = mOriginalURI ? mOriginalURI : mURL;
    NS_ADDREF(*aURL);
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::SetOriginalURI(nsIURI* aURL)
{
    mOriginalURI = aURL;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::GetURI(nsIURI* *aURL)
{
    *aURL = mURL;
    NS_ADDREF(*aURL);
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::SetURI(nsIURI* aURL)
{
    mURL = aURL;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::OpenInputStream(nsIInputStream **result)
{
    nsresult rv = NS_OK;

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFTPChannel::OpenInputStream() called\n"));

    if (mProxyChannel) {
        return mProxyChannel->OpenInputStream(result);
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
    rv = NS_NewSyncStreamListener(result, getter_AddRefs(bufOutStream),
                                  getter_AddRefs(listener));
    if (NS_FAILED(rv)) return rv;

    mListener = listener; // ensure that we insert ourselves as the proxy listener

    ///////////////////////////
    //// setup channel state

    ////////////////////////////////
    //// setup the channel thread
    nsFtpConnectionThread *thread = nsnull;
    NS_NEWXPCOM(thread, nsFtpConnectionThread);
    if (!thread) return NS_ERROR_OUT_OF_MEMORY;
    mConnThread = thread;

    rv = thread->Init(mHandler, this, mPrompter,
                      mBufferSegmentSize, mBufferMaxSize);
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
nsFTPChannel::OpenOutputStream(nsIOutputStream **result)
{
    if (mProxyChannel)
        return mProxyChannel->OpenOutputStream(result);
    NS_NOTREACHED("nsFTPChannel::OpenOutputStream");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFTPChannel::AsyncRead(nsIStreamListener *listener, nsISupports *ctxt)
{
    nsresult rv;

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFTPChannel::AsyncRead() called\n"));

    mListener = listener;
    mUserContext = ctxt;

    if (mEventSink) {
        nsCOMPtr<nsIIOService> serv = do_GetService(kIOServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;

        rv = mEventSink->OnStatus(this, ctxt, NS_NET_STATUS_BEGIN_FTP_TRANSACTION, nsnull);
        if (NS_FAILED(rv)) return rv;
    }

    if (mLoadGroup) {
        rv = mLoadGroup->AddChannel(this, nsnull);
        if (NS_FAILED(rv)) return rv;
    }

    if (mProxyChannel) {
        rv = mProxyChannel->SetTransferOffset(mSourceOffset);
        if (NS_FAILED(rv)) return rv;
        rv = mProxyChannel->SetTransferCount(mAmount);
        if (NS_FAILED(rv)) return rv;
        return mProxyChannel->AsyncRead(this, ctxt);
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

    rv = thread->Init(mHandler, this, mPrompter,
                      mBufferSegmentSize, mBufferMaxSize);
    mHandler = nsnull;
    if (NS_FAILED(rv)) return rv;

    rv = thread->SetStreamListener(this, ctxt);
    if (NS_FAILED(rv)) return rv;

    mConnected = PR_TRUE;

    return mPool->DispatchRequest((nsIRunnable*)thread);
}

NS_IMETHODIMP
nsFTPChannel::AsyncWrite(nsIInputStream *fromStream,
                         nsIStreamObserver *observer,
                         nsISupports *ctxt)
{
    nsresult rv = NS_OK;

    mObserver = observer;
    mUserContext = ctxt;

    if (mProxyChannel) {
        rv = mProxyChannel->SetTransferOffset(mSourceOffset);
        if (NS_FAILED(rv)) return rv;
        rv = mProxyChannel->SetTransferCount(mAmount);
        if (NS_FAILED(rv)) return rv;
        return mProxyChannel->AsyncWrite(fromStream, observer, ctxt);
    }

    NS_ASSERTION(mAmount > 0, "FTP requires stream len info");
    if (mAmount < 1) return NS_ERROR_NOT_INITIALIZED;

    nsFtpConnectionThread *thread = nsnull;
    NS_NEWXPCOM(thread, nsFtpConnectionThread);
    if (!thread) return NS_ERROR_OUT_OF_MEMORY;
    mConnThread = thread;

    rv = thread->Init(mHandler, this, mPrompter,
                      mBufferSegmentSize, mBufferMaxSize);
    mHandler = 0;
    if (NS_FAILED(rv)) return rv;

    rv = thread->SetWriteStream(fromStream, mAmount);
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
nsFTPChannel::SetContentLength(PRInt32 aContentLength)
{
    nsAutoLock lock(mLock);
    mContentLength = aContentLength;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::GetTransferOffset(PRUint32 *aTransferOffset)
{
    *aTransferOffset = mSourceOffset;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::SetTransferOffset(PRUint32 aTransferOffset)
{
    mSourceOffset = aTransferOffset;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::GetTransferCount(PRInt32 *aTransferCount)
{
    *aTransferCount = mAmount;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::SetTransferCount(PRInt32 aTransferCount)
{
    mAmount = aTransferCount;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::GetBufferSegmentSize(PRUint32 *aBufferSegmentSize)
{
    *aBufferSegmentSize = mBufferSegmentSize;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::SetBufferSegmentSize(PRUint32 aBufferSegmentSize)
{
    mBufferSegmentSize = aBufferSegmentSize;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::GetBufferMaxSize(PRUint32 *aBufferMaxSize)
{
    *aBufferMaxSize = mBufferMaxSize;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::SetBufferMaxSize(PRUint32 aBufferMaxSize)
{
    mBufferMaxSize = aBufferMaxSize;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::GetLocalFile(nsIFile* *file)
{
    *file = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::GetPipeliningAllowed(PRBool *aPipeliningAllowed)
{
    *aPipeliningAllowed = PR_FALSE;
    return NS_OK;
}
 
NS_IMETHODIMP
nsFTPChannel::SetPipeliningAllowed(PRBool aPipeliningAllowed)
{
    NS_NOTREACHED("SetPipeliningAllowed");
    return NS_ERROR_NOT_IMPLEMENTED;
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
        (void)mCallbacks->GetInterface(NS_GET_IID(nsIProgressEventSink), 
                                       getter_AddRefs(mEventSink));

        (void)mCallbacks->GetInterface(NS_GET_IID(nsIPrompt),
                                       getter_AddRefs(mPrompter));
    }
    return NS_OK;
}

NS_IMETHODIMP 
nsFTPChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
    *aSecurityInfo = nsnull;
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
nsFTPChannel::OnStatus(nsIChannel *aChannel, nsISupports *aContext,
                       nsresult aStatus, const PRUnichar* aStatusArg)
{
    if (!mEventSink)
        return NS_OK;

    nsAutoString str;
    if (mProxyChannel) {
        if (aStatusArg) {
            str.Append(aStatusArg); 
            str.AppendWithConversion("\n");
        }
        str.AppendWithConversion(mHost);
    }
    return mEventSink->OnStatus(this, aContext, aStatus, str.GetUnicode());
}

NS_IMETHODIMP
nsFTPChannel::OnProgress(nsIChannel* aChannel, nsISupports* aContext,
                                  PRUint32 aProgress, PRUint32 aProgressMax) {
    return mEventSink ? mEventSink->OnProgress(this, aContext, aProgress, aProgressMax) : NS_OK;
}


// nsIStreamObserver methods.
NS_IMETHODIMP
nsFTPChannel::OnStopRequest(nsIChannel* aChannel, nsISupports* aContext,
                            nsresult aStatus, const PRUnichar* aStatusArg)
{
    nsresult rv = NS_OK;
    mConnThread = nsnull;

    if (mLoadGroup) {
        rv = mLoadGroup->RemoveChannel(this, nsnull, aStatus, aStatusArg);
        if (NS_FAILED(rv)) return rv;
    }
    
    if (mObserver) {
        rv = mObserver->OnStopRequest(this, aContext, aStatus, aStatusArg);
        if (NS_FAILED(rv)) return rv;
    }

    if (mListener) {
        rv = mListener->OnStopRequest(this, aContext, aStatus, aStatusArg);
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

// nsIFTPChannel methods
NS_IMETHODIMP
nsFTPChannel::GetUsingProxy(PRBool *aUsingProxy)
{
    if (!aUsingProxy)
        return NS_ERROR_NULL_POINTER;
    *aUsingProxy = (!mProxyHost.IsEmpty() && !mProxyTransparent);
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::GetUsingTransparentProxy(PRBool *aUsingProxy)
{
    if (!aUsingProxy)
        return NS_ERROR_NULL_POINTER;
    *aUsingProxy = (!mProxyHost.IsEmpty() && mProxyTransparent);
    return NS_OK;
}

// nsIProxy methods
NS_IMETHODIMP
nsFTPChannel::GetProxyHost(char* *_retval) {
    *_retval = mProxyHost.ToNewCString();
    if (!*_retval) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::SetProxyHost(const char *aProxyHost) {
    mProxyHost = aProxyHost;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::GetProxyPort(PRInt32 *_retval) {
    *_retval = mProxyPort;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::SetProxyPort(PRInt32 aProxyPort) {
   mProxyPort = aProxyPort;
   return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::GetProxyType(char * *_retval)
{
    *_retval = mProxyType.ToNewCString();
    if (!*_retval) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::SetProxyType(const char * aProxyType)
{
    if (nsCRT::strcasecmp(aProxyType, "socks") == 0) {
        mProxyTransparent = PR_TRUE;
    } else {
        mProxyTransparent = PR_FALSE;
    }
    mProxyType = aProxyType;
    return NS_OK;
}
