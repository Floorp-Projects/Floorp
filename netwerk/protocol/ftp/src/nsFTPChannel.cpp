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
#include "nsICapabilities.h"
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

NS_IMETHODIMP_(nsrefcnt) nsFTPChannel::AddRef(void)
{                                                            
  NS_PRECONDITION(PRInt32(mRefCnt) >= 0, "illegal refcnt");  
  ++mRefCnt;                                                 
  NS_LOG_ADDREF(this, mRefCnt, "nsFTPChannel", sizeof(*this));      
  return mRefCnt;                                            
}
                          
NS_IMETHODIMP_(nsrefcnt) nsFTPChannel::Release(void)               
{                                                            
  NS_PRECONDITION(0 != mRefCnt, "dup release");              
  --mRefCnt;                                                 
  NS_LOG_RELEASE(this, mRefCnt, "nsFTPChannel");                    
  if (mRefCnt == 0) {                                        
    mRefCnt = 1; /* stabilize */                             
    NS_DELETEXPCOM(this);                                    
    return 0;                                                
  }                                                          
  return mRefCnt;                                            
}
NS_IMPL_QUERY_INTERFACE4(nsFTPChannel, nsIChannel, nsIFTPChannel, nsIStreamListener, nsIStreamObserver);

//NS_IMPL_ISUPPORTS4(nsFTPChannel, nsIChannel, nsIFTPChannel, nsIStreamListener, nsIStreamObserver);

nsresult
nsFTPChannel::Init(const char* verb, 
                   nsIURI* uri, 
                   nsILoadGroup* aLoadGroup,
                   nsICapabilities* notificationCallbacks, 
                   nsLoadFlags loadAttributes, 
                   nsIURI* originalURI,
                   nsIProtocolHandler* aHandler, 
                   nsIThreadPool* aPool)
{
    nsresult rv;

    if (mConnected)
        return NS_ERROR_FAILURE;

    mHandler = aHandler;

    rv = SetLoadAttributes(loadAttributes);
    if (NS_FAILED(rv)) return rv;

    rv = SetLoadGroup(aLoadGroup);
    if (NS_FAILED(rv)) return rv;

    rv = SetNotificationCallbacks(notificationCallbacks);
    if (NS_FAILED(rv)) return rv;

    NS_ASSERTION(aPool, "FTP channel needs a thread pool to play in");
    if (!aPool) return NS_ERROR_NULL_POINTER;
    mPool = aPool;

    mOriginalURI = originalURI ? originalURI : uri;
    mURL = uri;

    NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueService, &rv);
    if (NS_FAILED(rv)) return rv;

    // the FTP channel is instanciated on the main thread. the actual
    // protocol is interpreted on the FTP thread.
    rv = eventQService->GetThreadEventQueue(PR_CurrentThread(), getter_AddRefs(mEventQueue));
    if (NS_FAILED(rv)) return rv;

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
    nsresult rv = NS_OK;
    if (mProxiedThreadRequest)
        rv = mProxiedThreadRequest->Resume();
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

    mThreadRequest = do_QueryInterface((nsISupports*)(nsIRequest*)protocolInterpreter, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = protocolInterpreter->Init(mURL,                    /* url to load */
                                   mEventQueue,             /* event queue for this thread */
                                   mHandler,
                                   this, ctxt, mCallbacks);
    mHandler = 0;
    if (NS_FAILED(rv)) {
        NS_RELEASE(protocolInterpreter);
        return rv;
    }
    rv = mPool->DispatchRequest((nsIRunnable*)protocolInterpreter);

    NS_RELEASE(protocolInterpreter);
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
nsFTPChannel::GetNotificationCallbacks(nsICapabilities* *aNotificationCallbacks)
{
    *aNotificationCallbacks = mCallbacks.get();
    NS_IF_ADDREF(*aNotificationCallbacks);
    return NS_OK;
}

NS_IMETHODIMP
nsFTPChannel::SetNotificationCallbacks(nsICapabilities* aNotificationCallbacks)
{
    mCallbacks = aNotificationCallbacks;

    if (mCallbacks) {
        nsresult rv = mCallbacks->QueryCapability(NS_GET_IID(nsIProgressEventSink), 
                                                  getter_AddRefs(mEventSink));
        if (NS_FAILED(rv)) {
            PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFTPChannel::Init() (couldn't find event sink)\n"));
        }
    }
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIFTPChannel methods:
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
                                          PROXY_SYNC | PROXY_ALWAYS,
                                          getter_AddRefs(mProxiedThreadRequest));
        mThreadRequest = 0;
    } else {
        // no event queue means the thread has effectively gone away.
        mThreadRequest = 0;
        mProxiedThreadRequest = 0;
    }

    return rv;
}

NS_IMETHODIMP
nsFTPChannel::SetContentLength(PRInt32 aLength) {
    mContentLength = aLength;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIStreamObserver methods:

NS_IMETHODIMP
nsFTPChannel::OnStartRequest(nsIChannel* channel, nsISupports* context) {
    //MOZ_TIMER_START(channelTime, "nsFTPChannel::OnStart(): hit.");
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
    mProxiedThreadRequest = 0; // we don't want anyone trying to use this when the underlying thread has left.
    if (mEventSink) {
        nsAutoString statusMsg("FTP transaction complete.");
#ifndef BUG_16273_FIXED //TODO
        rv = mEventSink->OnStatus(this, context, statusMsg.ToNewUnicode());
#else
        rv = mEventSink->OnStatus(this, context, statusMsg.GetUnicode());
#endif
        if (NS_FAILED(rv)) return rv;
    }
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFTPChannel::OnStopRequest(channel = %x, context = %x, status = %d, msg = N/A)\n",channel, context, aStatus));
    if (mListener) {
        rv = mListener->OnStopRequest(channel, mContext, aStatus, aMsg);
    }

    if (mLoadGroup)
        rv = mLoadGroup->RemoveChannel(this, nsnull, aStatus, aMsg);
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

    if (mListener) {
        rv = mListener->OnDataAvailable(channel, mContext, aIStream, aSourceOffset, aLength);
    }
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
