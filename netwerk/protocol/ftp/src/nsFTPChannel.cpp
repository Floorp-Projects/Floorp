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


#include "nsFTPChannel.h"
#include "nsIStreamListener.h"
#include "nsIServiceManager.h"
#include "nsCExternalHandlerService.h"
#include "nsIMIMEService.h"
#include "nsNetUtil.h"
#include "nsMimeTypes.h"

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
// Client initiation is the most common case and is attempted first.

nsFTPChannel::nsFTPChannel()
    : mLoadAttributes(LOAD_NORMAL),
      mSourceOffset(0),
      mAmount(0),
      mContentLength(-1),
      mFTPState(nsnull),
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
#if defined(PR_LOGGING)
    nsXPIDLCString spec;
    mURL->GetSpec(getter_Copies(spec));
    PR_LOG(gFTPLog, PR_LOG_ALWAYS, ("~nsFTPChannel() for %s", (const char*)spec));
#endif
    NS_IF_RELEASE(mFTPState);
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
nsFTPChannel::Init(nsIURI* uri)
{
    nsresult rv = NS_OK;

    // setup channel state
    mURL = uri;

    rv = mURL->GetHost(getter_Copies(mHost));
    if (NS_FAILED(rv)) return rv;

    if (!mLock) {
        mLock = PR_NewLock();
        if (!mLock) return NS_ERROR_OUT_OF_MEMORY;
    }
    return NS_OK;
}

nsresult
nsFTPChannel::SetProxyChannel(nsIChannel *aChannel) 
{
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
    nsAutoLock lock(mLock);
    mStatus = status;
    if (mProxyChannel) {
        return mProxyChannel->Cancel(status);
    } else if (mFTPState) {
        return mFTPState->Cancel(status);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::Suspend(void) {
    nsAutoLock lock(mLock);
    if (mProxyChannel) {
        return mProxyChannel->Suspend();
    } else if (mFTPState) {
        return mFTPState->Suspend();
    }
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::Resume(void) {
    nsAutoLock lock(mLock);
    if (mProxyChannel) {
        return mProxyChannel->Resume();
    } else if (mFTPState) {
        return mFTPState->Resume();
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
    if (mProxyChannel)
        return mProxyChannel->OpenInputStream(result);
    NS_NOTREACHED("nsFTPChannel::OpenInputStream");
    return NS_ERROR_NOT_IMPLEMENTED;
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
        // rjc says: ignore errors on SetTransferOffset() and
        // SetTransferCount() as they may be unimplemented
        rv = mProxyChannel->SetTransferOffset(mSourceOffset);
        rv = mProxyChannel->SetTransferCount(mAmount);

        return mProxyChannel->AsyncRead(this, ctxt);
    }

    ////////////////////////////////
    //// setup the channel thread
    if (!mFTPState) {
        NS_NEWXPCOM(mFTPState, nsFtpState);
        if (!mFTPState) return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(mFTPState);
    }
    rv = mFTPState->Init(this, mPrompter,
                         mBufferSegmentSize, mBufferMaxSize);
    if (NS_FAILED(rv)) return rv;

    rv = mFTPState->SetStreamListener(this, ctxt);
    if (NS_FAILED(rv)) return rv;

    return mFTPState->Connect();
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

    if (!mFTPState) {
        NS_NEWXPCOM(mFTPState, nsFtpState);
        if (!mFTPState) return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(mFTPState);
    }
    
    rv = mFTPState->Init(this, mPrompter,
                         mBufferSegmentSize, mBufferMaxSize);
    if (NS_FAILED(rv)) return rv;

    rv = mFTPState->SetWriteStream(fromStream, mAmount);
    if (NS_FAILED(rv)) return rv;

    rv = mFTPState->SetStreamObserver(this, ctxt);
    if (NS_FAILED(rv)) return rv;

    if (mLoadGroup) {
        rv = mLoadGroup->AddChannel(this, nsnull);
        if (NS_FAILED(rv)) return rv;
    }
    return mFTPState->Connect();
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

        nsCOMPtr<nsIMIMEService> MIMEService (do_GetService(NS_MIMESERVICE_CONTRACTID, &rv));
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
    return mEventSink ? mEventSink->OnProgress(this, aContext, aProgress, (PRUint32) mContentLength) : NS_OK;
}


// nsIStreamObserver methods.
NS_IMETHODIMP
nsFTPChannel::OnStopRequest(nsIChannel* aChannel, nsISupports* aContext,
                            nsresult aStatus, const PRUnichar* aStatusArg)
{
    nsresult rv = NS_OK;
    
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
