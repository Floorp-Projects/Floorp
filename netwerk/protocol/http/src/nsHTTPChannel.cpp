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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */


#include "nspr.h"
#include "prlong.h"
#include "nsHTTPChannel.h"
#include "netCore.h"
#include "nsIHTTPEventSink.h"
#include "nsIHTTPProtocolHandler.h"
#include "nsHTTPRequest.h"
#include "nsHTTPResponse.h"
#include "nsIInterfaceRequestor.h"
#include "nsIChannel.h"
#include "nsIInputStream.h"
#include "nsIStreamListener.h"
#include "nsIIOService.h"
#include "nsXPIDLString.h"
#include "nsHTTPAtoms.h"
#include "nsNetUtil.h"
#include "nsIHttpNotify.h"
#include "nsINetModRegEntry.h"
#include "nsProxyObjectManager.h"
#include "nsIServiceManager.h"
#include "nsINetModuleMgr.h"
#include "nsIEventQueueService.h"
#include "nsIMIMEService.h"
#include "nsIEnumerator.h"
#include "nsAuthEngine.h"
#include "nsINetDataCacheManager.h"
#include "nsINetDataCache.h"
#include "nsIScriptSecurityManager.h"
#include "nsIProxy.h"
#include "nsMimeTypes.h"
#include "nsIPrompt.h"
#include "nsISocketTransport.h"
// FIXME - Temporary include.  Delete this when cache is enabled on all platforms
#include "nsIPref.h"

// Once other kinds of auth are up change TODO
#include "nsBasicAuth.h" 
// This will go away once the dialog box starts popping off the window that triggered the load. 
#include "nsAppShellCIDs.h" // TODO remove later
#include "nsIAppShellService.h" // TODO remove later
#include "nsIXULWindow.h" // TODO remove later
static NS_DEFINE_CID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_CID(kProxyObjectManagerCID, NS_PROXYEVENT_MANAGER_CID);
#include "nsINetPrompt.h"
#include "nsProxiedService.h"

static NS_DEFINE_IID(kProxyObjectManagerIID, NS_IPROXYEVENT_MANAGER_IID);
static NS_DEFINE_CID(kEventQueueService, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kNetModuleMgrCID, NS_NETMODULEMGR_CID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

#if defined(PR_LOGGING)
extern PRLogModuleInfo* gHTTPLog;
#endif /* PR_LOGGING */

static NS_DEFINE_CID(kMIMEServiceCID, NS_MIMESERVICE_CID);
static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

nsHTTPChannel::nsHTTPChannel(nsIURI* i_URL, 
                             const char *i_Verb,
                             nsIURI* originalURI,
                             nsHTTPHandler* i_Handler,
                             PRUint32 bufferSegmentSize,
                             PRUint32 bufferMaxSize): 
    mResponse(nsnull),
    mHandler(dont_QueryInterface(i_Handler)),
    mHTTPServerListener(nsnull),
    mResponseContext(nsnull),
    mOriginalURI(dont_QueryInterface(originalURI ? originalURI : i_URL)),
    mURI(dont_QueryInterface(i_URL)),
    mConnected(PR_FALSE),
    mState(HS_IDLE),
    mLoadAttributes(LOAD_NORMAL),
    mLoadGroup(nsnull),
    mCachedResponse(nsnull),
    mCachedContentIsAvailable(PR_FALSE),
    mCachedContentIsValid(PR_FALSE),
    mFiredOnHeadersAvailable(PR_FALSE),
    mFiredOpenOnStartRequest(PR_FALSE),
    mAuthTriedWithPrehost(PR_FALSE),
    mProxy(0),
    mProxyPort(-1),
    mBufferSegmentSize(bufferSegmentSize),
    mBufferMaxSize(bufferMaxSize),
    mTransport(nsnull)
{
    NS_INIT_REFCNT();

#if defined(PR_LOGGING)
  nsXPIDLCString urlCString; 
  mURI->GetSpec(getter_Copies(urlCString));
  
  PR_LOG(gHTTPLog, PR_LOG_DEBUG, 
         ("Creating nsHTTPChannel [this=%x] for URI: %s.\n", 
           this, (const char *)urlCString));
#endif

    mVerb = i_Verb;
}

nsHTTPChannel::~nsHTTPChannel()
{
    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
           ("Deleting nsHTTPChannel [this=%x].\n", this));

#ifdef NS_DEBUG
    NS_RELEASE(mRequest);
#else
    NS_IF_RELEASE(mRequest);
#endif
    NS_IF_RELEASE(mResponse);
    NS_IF_RELEASE(mCachedResponse);

    mHandler         = null_nsCOMPtr();
    mEventSink       = null_nsCOMPtr();
    mPrompter        = null_nsCOMPtr();
    mResponseContext = null_nsCOMPtr();
    mLoadGroup       = null_nsCOMPtr();
    CRTFREEIF(mProxy);
}

NS_IMPL_THREADSAFE_ADDREF(nsHTTPChannel)
NS_IMPL_THREADSAFE_RELEASE(nsHTTPChannel)

NS_IMPL_QUERY_INTERFACE5(nsHTTPChannel,
                         nsIHTTPChannel,
                         nsIChannel,
                         nsIInterfaceRequestor,
                         nsIProgressEventSink,
                         nsIProxy);

////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:

NS_IMETHODIMP
nsHTTPChannel::IsPending(PRBool *result)
{
  return mRequest->IsPending(result);
}

NS_IMETHODIMP
nsHTTPChannel::Cancel(void)
{
  nsresult rv;

  //
  // If this channel is currently waiting for a transport to become available.
  // Notify the HTTPHandler that this request has been cancelled...
  //
  if (!mConnected && (HS_WAITING_FOR_OPEN == mState)) {
    rv = mHandler->CancelPendingChannel(this);
  }

  return mRequest->Cancel();
}

NS_IMETHODIMP
nsHTTPChannel::Suspend(void)
{
  return mRequest->Suspend();
}

NS_IMETHODIMP
nsHTTPChannel::Resume(void)
{
  return mRequest->Resume();
}

////////////////////////////////////////////////////////////////////////////////
// nsIChannel methods:

NS_IMETHODIMP
nsHTTPChannel::GetOriginalURI(nsIURI* *o_URL)
{
    *o_URL = mOriginalURI;
    NS_IF_ADDREF(*o_URL);
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPChannel::GetURI(nsIURI* *o_URL)
{
    *o_URL = mURI;
    NS_IF_ADDREF(*o_URL);
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPChannel::OpenInputStream(PRUint32 startPosition, PRInt32 readCount,
                               nsIInputStream **o_Stream)
{
    nsresult rv;
    if (mConnected) return NS_ERROR_ALREADY_CONNECTED;

    rv = NS_NewSyncStreamListener(o_Stream,
                                  getter_AddRefs(mBufOutputStream),
                                  getter_AddRefs(mResponseDataListener));
    if (NS_FAILED(rv)) return rv;

    mBufOutputStream = 0;
    rv = Open();
    if (NS_FAILED(rv)) return rv;

    // If the cached data hasn't expired, it's unnecessary to contact
    // the server.  Just hand out the data in the cache.
    if (mCachedContentIsValid)
        rv = ReadFromCache(startPosition, readCount);

    return rv;
}

NS_IMETHODIMP
nsHTTPChannel::OpenOutputStream(PRUint32 startPosition, nsIOutputStream **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTTPChannel::AsyncOpen(nsIStreamObserver *observer, nsISupports* ctxt)
{
    nsresult rv = NS_OK;

    // parameter validation
    if (!observer) return NS_ERROR_NULL_POINTER;

    if (mResponseDataListener) {
        rv = NS_ERROR_IN_PROGRESS;
    } 

    mOpenObserver = observer;
    mOpenContext = ctxt;

    return Open();
}

NS_IMETHODIMP
nsHTTPChannel::AsyncRead(PRUint32 startPosition, PRInt32 readCount,
                         nsISupports *aContext,
                         nsIStreamListener *listener)
{
    nsresult rv = NS_OK;

    // Initial parameter checks...
    if (mResponseDataListener) {
        return NS_ERROR_IN_PROGRESS;
    } 
    else if (!listener) {
        return NS_ERROR_NULL_POINTER;
    }

    mResponseDataListener = listener;
    mResponseContext = aContext;

    if (!mOpenObserver)
        Open();
    
    // If the data in the cache hasn't expired, then there's no need to talk
    // with the server.  Create a stream from the cache, synthesizing all the
    // various channel-related events.
    if (mCachedContentIsValid) {
        ReadFromCache(startPosition, readCount);
    } else if (mOpenObserver) {
        // we were AsyncOpen()'d
        NS_ASSERTION(mHTTPServerListener, "ResponseListener was not set!.");
        if (mHTTPServerListener) {
            rv = mHTTPServerListener->FireSingleOnData(listener, aContext);
        } else {
            rv = NS_ERROR_NULL_POINTER;
        }
    }

    return rv;
}

NS_IMETHODIMP
nsHTTPChannel::AsyncWrite(nsIInputStream *fromStream,
                          PRUint32 startPosition,
                          PRInt32 writeCount,
                          nsISupports *ctxt,
                          nsIStreamObserver *observer)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTTPChannel::GetLoadAttributes(PRUint32 *aLoadAttributes)
{
    *aLoadAttributes = mLoadAttributes;
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPChannel::SetLoadAttributes(PRUint32 aLoadAttributes)
{
    mLoadAttributes = aLoadAttributes;
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPChannel::GetContentType(char * *aContentType)
{
    nsresult rv = NS_OK;

    // Parameter validation...
    if (!aContentType) {
        return NS_ERROR_NULL_POINTER;
    }
    *aContentType = nsnull;

    //
    // If the content type has been returned by the server then return that...
    //
    if (mResponse) {
      rv = mResponse->GetContentType(aContentType);
      if (rv != NS_ERROR_NOT_AVAILABLE) return rv;
    }

    //
    // No response yet...  Try to determine the content-type based
    // on the file extension of the URI...
    //
    NS_WITH_SERVICE(nsIMIMEService, MIMEService, kMIMEServiceCID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = MIMEService->GetTypeFromURI(mURI, aContentType);
        if (NS_SUCCEEDED(rv)) return rv;
        // XXX we should probably set the content-type for this http response at this stage too.
    }

    if (!*aContentType) 
        *aContentType = nsCRT::strdup(UNKNOWN_CONTENT_TYPE);
    if (!*aContentType) {
        rv = NS_ERROR_OUT_OF_MEMORY;
    } else {
        rv = NS_OK;
    }

    return rv;
}

NS_IMETHODIMP
nsHTTPChannel::SetContentType(const char *aContentType)
{
  nsresult rv;

  if (mResponse) {
    rv = mResponse->SetContentType(aContentType);
  } else {
    //
    // Do not allow the content-type to be set until a response has been
    // received from the server...
    //
    rv = NS_ERROR_FAILURE;
  }

  return rv;
}


NS_IMETHODIMP
nsHTTPChannel::GetContentLength(PRInt32 *aContentLength)
{
  if (!mResponse)
    return NS_ERROR_NOT_AVAILABLE;
  return mResponse->GetContentLength(aContentLength);
}


NS_IMETHODIMP
nsHTTPChannel::GetLoadGroup(nsILoadGroup * *aLoadGroup)
{
    *aLoadGroup = mLoadGroup;
    NS_IF_ADDREF(*aLoadGroup);
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPChannel::SetLoadGroup(nsILoadGroup *aGroup)
{
    mLoadGroup = aGroup;
  
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPChannel::GetOwner(nsISupports * *aOwner)
{
    *aOwner = mOwner.get();
    NS_IF_ADDREF(*aOwner);
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPChannel::SetOwner(nsISupports * aOwner)
{
    mOwner = aOwner;
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPChannel::GetNotificationCallbacks(nsIInterfaceRequestor* *aNotificationCallbacks)
{
    *aNotificationCallbacks = mCallbacks.get();
    NS_IF_ADDREF(*aNotificationCallbacks);
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPChannel::SetNotificationCallbacks(nsIInterfaceRequestor* 
        aNotificationCallbacks)
{
    nsresult rv = NS_OK;
    mCallbacks = aNotificationCallbacks;

    // Verify that the event sink is http
    if (mCallbacks) {
        // we don't care if this fails -- we don't need an nsIHTTPEventSink
        // to proceed
        (void)mCallbacks->GetInterface(NS_GET_IID(nsIHTTPEventSink),
                                       getter_AddRefs(mEventSink));

        (void)mCallbacks->GetInterface(NS_GET_IID(nsIPrompt),
                                       getter_AddRefs(mPrompter));

        (void)mCallbacks->GetInterface(NS_GET_IID(nsIProgressEventSink),
                                       getter_AddRefs(mProgressEventSink));
    }
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIHTTPChannel methods:

NS_IMETHODIMP
nsHTTPChannel::GetRequestHeader(nsIAtom* i_Header, char* *o_Value)
{
    return mRequest->GetHeader(i_Header, o_Value);
}

NS_IMETHODIMP
nsHTTPChannel::SetRequestHeader(nsIAtom* i_Header, const char* i_Value)
{
    return mRequest->SetHeader(i_Header, i_Value);
}

NS_IMETHODIMP
nsHTTPChannel::GetRequestHeaderEnumerator(nsISimpleEnumerator** aResult)
{
    return mRequest->GetHeaderEnumerator(aResult);
}


NS_IMETHODIMP
nsHTTPChannel::GetResponseHeader(nsIAtom* i_Header, char* *o_Value)
{
    if (!mConnected)
        Open();
    if (mResponse)
        return mResponse->GetHeader(i_Header, o_Value);
    else
        return NS_ERROR_FAILURE ; // NS_ERROR_NO_RESPONSE_YET ? 
}


NS_IMETHODIMP
nsHTTPChannel::GetResponseHeaderEnumerator(nsISimpleEnumerator** aResult)
{
    nsresult rv;

    if (!mConnected) {
        Open();
    }

    if (mResponse) {
        rv = mResponse->GetHeaderEnumerator(aResult);
    } else {
        *aResult = nsnull;
        rv = NS_ERROR_FAILURE ; // NS_ERROR_NO_RESPONSE_YET ? 
    }
    return rv;
}

NS_IMETHODIMP
nsHTTPChannel::SetResponseHeader(nsIAtom* i_Header, const char* i_HeaderValue) {
    nsresult rv = NS_OK;

    if (!mConnected) {
        rv = Open();
        if (NS_FAILED(rv)) return rv;
    }

    if (mResponse) {
        // we need to set the header and ensure that observers are notified again.
        rv = mResponse->SetHeader(i_Header, i_HeaderValue);
        if (NS_FAILED(rv)) return rv;

        rv = OnHeadersAvailable();

    } else {
        rv = NS_ERROR_FAILURE;
    }
    return rv;
}

NS_IMETHODIMP
nsHTTPChannel::GetResponseStatus(PRUint32  *o_Status)
{
    if (!mConnected) 
        Open();
    if (mResponse)
        return mResponse->GetStatus(o_Status);
    return NS_ERROR_FAILURE; // NS_ERROR_NO_RESPONSE_YET ? 
}

NS_IMETHODIMP
nsHTTPChannel::GetResponseString(char* *o_String) 
{
    if (!mConnected) 
        Open();
    if (mResponse)
        return mResponse->GetStatusString(o_String);
    return NS_ERROR_FAILURE; // NS_ERROR_NO_RESPONSE_YET ? 
}

NS_IMETHODIMP
nsHTTPChannel::GetEventSink(nsIHTTPEventSink* *o_EventSink) 
{
    nsresult rv;
    if (o_EventSink) {
        *o_EventSink = mEventSink;
        NS_IF_ADDREF(*o_EventSink);
        rv = NS_OK;
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
}

NS_IMETHODIMP
nsHTTPChannel::GetResponseDataListener(nsIStreamListener* *aListener)
{
    nsresult rv = NS_OK;

    if (aListener && mResponseDataListener) {
        *aListener = mResponseDataListener.get();
        NS_ADDREF(*aListener);
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }

    return rv;
}

NS_IMETHODIMP
nsHTTPChannel::SetRequestMethod(PRUint32/*HTTPMethod*/ i_Method)
{
    return mRequest->SetMethod((HTTPMethod)i_Method);
}

NS_IMETHODIMP
nsHTTPChannel::GetCharset(char* *o_String)
{
  if (!mResponse)
    return NS_ERROR_NOT_AVAILABLE;
  return mResponse->GetCharset(o_String);
}

NS_IMETHODIMP
nsHTTPChannel::SetPostDataStream(nsIInputStream* aPostStream)
{
  return mRequest->SetPostDataStream(aPostStream);
}

NS_IMETHODIMP
nsHTTPChannel::GetPostDataStream(nsIInputStream **o_postStream)
{ 
  return mRequest->GetPostDataStream(o_postStream);
}

NS_IMETHODIMP
nsHTTPChannel::SetAuthTriedWithPrehost(PRBool iTried)
{
    mAuthTriedWithPrehost = iTried;
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPChannel::GetAuthTriedWithPrehost(PRBool* oTried)
{
    if (oTried)
    {
        *oTried = mAuthTriedWithPrehost;
        return NS_OK;
    }
    else
        return NS_ERROR_NULL_POINTER;
}

// nsIInterfaceRequestor method
NS_IMETHODIMP
nsHTTPChannel::GetInterface(const nsIID &anIID, void **aResult ) {
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
nsHTTPChannel::OnStatus(nsIChannel *aChannel,
                        nsISupports *aContext,
                        const PRUnichar *aMsg) {
    nsresult rv = NS_OK;
    if (mCallbacks) {
        nsCOMPtr<nsIProgressEventSink> progressProxy;
        rv = mCallbacks->GetInterface(NS_GET_IID(nsIProgressEventSink), 
                getter_AddRefs(progressProxy));
        if (NS_FAILED(rv)) return rv;
        rv = progressProxy->OnStatus(this, aContext, aMsg);
    }
    return rv;
}

NS_IMETHODIMP
nsHTTPChannel::OnProgress(nsIChannel* aChannel, nsISupports* aContext,
                                  PRUint32 aProgress, PRUint32 aProgressMax) {
    return mProgressEventSink ? mProgressEventSink->OnProgress(this, aContext, 
                                aProgress, aProgressMax) : NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTTPChannel methods:

nsresult nsHTTPChannel::Init(nsILoadGroup *aLoadGroup) 
{
  nsresult rv;

  /* 
    Set up a request object - later set to a clone of a default 
    request from the handler. TODO
  */
  mRequest = new nsHTTPRequest(mURI);
  if (!mRequest) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(mRequest);
  (void) mRequest->SetConnection(this);
  
  rv = SetLoadGroup(aLoadGroup);

  return rv;
}


// Create a cache entry for the channel's URL or retrieve an existing one.  If
// there's an existing cache entry for the current URL, confirm that it doesn't
// contain partially downloaded content.  Finally, check to see if the cache
// data has expired, in which case it may need to be revalidated with the
// server.
//
// We might call this function several times for the same request, i.e. once
// before a transport is requested and once again when it becomes available.
// In the interim, another HTTP request might have updated the cache entry, so
// we need to got through all the machinations each time.
//
nsresult
nsHTTPChannel::CheckCache()
{
    nsresult rv;
    static PRBool warnedCacheIsMissing = PR_FALSE;

    // For now, we handle only GET and HEAD requests
    HTTPMethod httpMethod;
    httpMethod = mRequest->GetMethod();
    if ((httpMethod != HM_GET) && (httpMethod != HM_HEAD))
        return NS_OK;

    // If this is the first time we've been called for this channel,
    // retrieve an existing cache entry or create a new one.
    if (!mCacheEntry) {

#if defined(XP_PC) || defined(XP_MAC) || defined(XP_UNIX)
        // Temporary code to disable cache on platforms where it is not known to work
        static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

        NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &rv); 
        PRBool useCache = PR_FALSE;
        if (NS_SUCCEEDED(rv))
            prefs->GetBoolPref("browser.cache.enable", &useCache);
        if (!useCache)
            return NS_OK;
#endif

        // Get the cache manager service
        // TODO - we should cache this service
        NS_WITH_SERVICE(nsINetDataCacheManager, cacheManager,
                        NS_NETWORK_CACHE_MANAGER_PROGID, &rv);
        if (rv == NS_ERROR_FACTORY_NOT_REGISTERED) {
            if (!warnedCacheIsMissing) {
                NS_WARNING("Unable to find network cache component. "
                           "Network performance will be diminished");
                warnedCacheIsMissing = PR_TRUE;   
            }
            return NS_OK; 
        } else if (NS_FAILED(rv))
            return rv;

        PRUint32 cacheFlags;
        if (mLoadAttributes & nsIChannel::CACHE_AS_FILE)
            cacheFlags = nsINetDataCacheManager::CACHE_AS_FILE;
        else if (mLoadAttributes & nsIChannel::INHIBIT_PERSISTENT_CACHING)
            cacheFlags = nsINetDataCacheManager::BYPASS_PERSISTENT_CACHE;
        else
            cacheFlags = 0;

        // Retrieve an existing cache entry or create a new one if none exists for the
        // given URL.
        //
        // TODO - Pass in post data (or a hash of it) as the secondary key
        // argument to GetCachedNetData()
        nsXPIDLCString urlCString; 
        mURI->GetSpec(getter_Copies(urlCString));
        rv = cacheManager->GetCachedNetData(urlCString, 0, 0, cacheFlags,
                                            getter_AddRefs(mCacheEntry));
        if (NS_FAILED(rv)) return rv;
        NS_ASSERTION(mCacheEntry, "Cache manager must always return cache entry");
        if (!mCacheEntry)
            return NS_ERROR_FAILURE;
    }
    
    // Be pessimistic: Assume cache entry has no useful data
    mCachedContentIsAvailable = mCachedContentIsValid = PR_FALSE;

    // Be pessimistic: Clear If-Modified-Since request header
    SetRequestHeader(nsHTTPAtoms::If_Modified_Since, 0);

    // If we can't use the cache data (because an end-to-end reload has been requested)
    // there's no point in inspecting it since it's about to be overwritten.
    if (mLoadAttributes & nsIChannel::FORCE_RELOAD)
        return NS_OK;

    // Due to architectural limitations in the cache manager, a cache entry can
    // not be accessed if it is currently being updated by another HTTP
    // request.  If that's the case, we ignore the cache entry for purposes of
    // both reading and writing.
    PRBool updateInProgress;
    mCacheEntry->GetUpdateInProgress(&updateInProgress);
    if (updateInProgress)
        return NS_OK;

    PRUint32 contentLength;
    mCacheEntry->GetStoredContentLength(&contentLength);
    PRBool partialFlag;
    mCacheEntry->GetPartialFlag(&partialFlag);

    // Check to see if there's content in the cache.  For now, we can't deal
    // with partial cache entries.
    if (!contentLength || partialFlag)
        return NS_OK;

    // Retrieve the HTTP headers from the cache
    nsXPIDLCString cachedHeaders;
    PRUint32 cachedHeadersLength;
    rv = mCacheEntry->GetAnnotation("HTTP headers", &cachedHeadersLength,
                                    getter_Copies(cachedHeaders));
    if (NS_FAILED(rv)) return rv;
    if (!cachedHeadersLength)
        return NS_ERROR_FAILURE;

    // Parse the cached HTTP headers
    NS_IF_RELEASE(mCachedResponse);
    mCachedResponse = new nsHTTPResponse;
    if (!mCachedResponse)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(mCachedResponse);
    nsSubsumeCStr cachedHeadersCStr(NS_CONST_CAST(char*, 
                NS_STATIC_CAST(const char*, cachedHeaders)),
                                    PR_FALSE);
    rv = mCachedResponse->ParseHeaders(cachedHeadersCStr);
    if (NS_FAILED(rv)) return rv;
    
    // At this point, we know there is an existing, non-truncated entry in the
    // cache and that it has apparently valid HTTP headers.
    mCachedContentIsAvailable = PR_TRUE;

    // If validation is inhibited, we'll just use whatever data is in
    // the cache, regardless of whether or not it has expired.
    if (mLoadAttributes & nsIChannel::VALIDATE_NEVER) {
        mCachedContentIsValid = PR_TRUE;
        return NS_OK;
    }

    // If the must-revalidate directive is present in the cached response, data
    // must always be revalidated with the server, even if the user has
    // configured validation to be turned off.
    PRBool mustRevalidate = PR_FALSE;
    nsXPIDLCString header;
    mCachedResponse->GetHeader(nsHTTPAtoms::Cache_Control, getter_Copies(header));
    if (header) {
        PRInt32 offset;

        nsCAutoString cacheControlHeader = (const char*)header;
        offset = cacheControlHeader.Find("must-revalidate", PR_TRUE);
        if (offset != kNotFound)
            mustRevalidate = PR_TRUE;
    }

    // If the FORCE_VALIDATION flag is set, any cached data won't be used until
    // it's revalidated with the server, so there's no point in checking if it's
    // expired.
    PRBool doIfModifiedSince;
    if ((mLoadAttributes & nsIChannel::FORCE_VALIDATION) || mustRevalidate) {
        doIfModifiedSince = PR_TRUE;
    } else {

        doIfModifiedSince = PR_FALSE;

        // Determine if this is the first time that this cache entry has been
        // accessed in this session.
        PRBool firstAccessThisSession;
        PRTime lastAccessTime, sessionStartTime;
        mCacheEntry->GetLastAccessTime(&lastAccessTime);
        sessionStartTime = mHandler->GetSessionStartTime();
        firstAccessThisSession = LL_UCMP(lastAccessTime, < , sessionStartTime);

        // Check to see if we can use the cache data without revalidating it with
        // the server.
        PRBool useHeuristicExpiration = mLoadAttributes & nsIChannel::VALIDATE_HEURISTICALLY;
        PRBool cacheEntryIsStale;
        cacheEntryIsStale = mCachedResponse->IsStale(useHeuristicExpiration);

        // If the content is stale, issue an if-modified-since request
        if (cacheEntryIsStale) {
            if (mLoadAttributes & nsIChannel::VALIDATE_ONCE_PER_SESSION) {
                doIfModifiedSince = firstAccessThisSession;
            } else {
                // VALIDATE_ALWAYS || VALIDATE_HEURISTICALLY || default
                doIfModifiedSince = PR_TRUE;
            }
        }
    }

    if (doIfModifiedSince) {
        // Add If-Modified-Since header
        nsXPIDLCString lastModified;
        mCachedResponse->GetHeader(nsHTTPAtoms::Last_Modified, getter_Copies(lastModified));
        if (lastModified)
            SetRequestHeader(nsHTTPAtoms::If_Modified_Since, lastModified);

        // Add If-Match header
        nsXPIDLCString etag;
        mCachedResponse->GetHeader(nsHTTPAtoms::ETag, getter_Copies(etag));
        if (etag)
            SetRequestHeader(nsHTTPAtoms::If_None_Match, etag);
    }

    mCachedContentIsValid = !doIfModifiedSince;

    return NS_OK;
}

// If the data in the cache hasn't expired, then there's no need to
// talk with the server, not even to do an if-modified-since.  This
// method creates a stream from the cache, synthesizing all the various
// channel-related events.
nsresult
nsHTTPChannel::ReadFromCache(PRUint32 aStartPosition, PRInt32 aReadCount)
{
    nsresult rv;

    NS_ASSERTION(mCacheEntry && mCachedContentIsValid && mCachedResponse,
                 "Attempting to read from cache when content is not valid");
    if (!mCacheEntry || !mCachedContentIsValid || !mCachedResponse)
        return NS_ERROR_FAILURE;

    NS_ASSERTION(mResponseDataListener, 
                 "Attempt to retrieve cache data before AsyncRead or OpenInputStream");
    if (!mResponseDataListener)
        return NS_ERROR_FAILURE;

#if defined(PR_LOGGING)
  nsresult log_rv;
  char *URLSpec;

  log_rv = mURI->GetSpec(&URLSpec);
  if (NS_FAILED(log_rv)) {
  	URLSpec = nsCRT::strdup("?");
  }

  PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
         ("nsHTTPChannel::ReadFromCache [this=%x].\t"
          "Using cache copy for: %s\n",
          this, URLSpec));
  nsAllocator::Free(URLSpec);
#endif /* PR_LOGGING */

    // Create a cache transport to read the cached response...
    nsCOMPtr<nsIChannel> cacheTransport;
    rv = mCacheEntry->NewChannel(mLoadGroup, getter_AddRefs(cacheTransport));
    if (NS_FAILED(rv)) return rv;

    mRequest->SetTransport(cacheTransport);

    // Fake it so that HTTP headers come from cached versions
    SetResponse(mCachedResponse);
    
    // Create a listener that intercepts cache reads and fires off the appropriate
    // events such as OnHeadersAvailable
    nsHTTPResponseListener* listener;
    listener = new nsHTTPCacheListener(this);
    if (!listener)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(listener);

    listener->SetListener(mResponseDataListener);
    mConnected = PR_TRUE;

    // Fire all the normal events for header arrival
    FinishedResponseHeaders();

    // Pump the cache data downstream
    rv = cacheTransport->AsyncRead(aStartPosition, aReadCount,
                                 mResponseContext, listener);
    NS_RELEASE(listener);
    if (NS_FAILED(rv)) {
        ResponseCompleted(nsnull, rv, 0);
    }
    return rv;
}

// Cache the network response from the server, including both the content and
// the HTTP headers.
nsresult
nsHTTPChannel::CacheReceivedResponse(nsIStreamListener *aListener, nsIStreamListener* *aResult)
{
    nsresult rv;
    NS_ENSURE_ARG_POINTER(aListener);

    // If caching is disabled, there will be no cache entry
    if (!mCacheEntry)
        return NS_OK;

    // If the current response is itself from the cache rather than the network
    // server, don't allow it to overwrite itself.
    if (mCachedContentIsValid)
        return NS_OK;

    // Due to architectural limitations in the cache manager, a cache entry can
    // not be written to if it is currently being read or written.  In that
    // case, we silently cancel the update of the cache entry.
    PRBool inUse;
    rv = mCacheEntry->GetInUse(&inUse);
    if (NS_FAILED(rv)) return rv;
    if (inUse)
        return NS_OK;

    // The no-store directive within the 'Cache-Control:' header indicates that we
    // should not store the response in the cache
    nsXPIDLCString header;
    GetResponseHeader(nsHTTPAtoms::Cache_Control, getter_Copies(header));
    if (header) {
        PRInt32 offset;

        nsCAutoString cacheControlHeader = (const char*)header;
        offset = cacheControlHeader.Find("no-store", PR_TRUE);
        if (offset != kNotFound)
            return NS_OK;
    }

    // Although 'Pragma:no-cache' is not a standard HTTP response header (it's
    // a request header), caching is inhibited when this header is present so
    // as to match existing Navigator behavior.
    GetResponseHeader(nsHTTPAtoms::Pragma, getter_Copies(header));
    if (header) {
        PRInt32 offset;

        nsCAutoString pragmaHeader = (const char*)header;
        offset = pragmaHeader.Find("no-cache", PR_TRUE);
        if (offset != kNotFound)
            return NS_OK;
    }

    // Inhibit any other HTTP requests from writing into this cache entry
    rv = mCacheEntry->SetUpdateInProgress(PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    // For now, tell the cache that we don't support fetching of partial content 
    // TODO - Set to true if server indicates that it supports byte ranges
    rv = mCacheEntry->SetAllowPartial(PR_FALSE);
    if (NS_FAILED(rv)) return rv;

    // Retrieve the value of the 'Last-Modified:' header, if present
    PRTime lastModified;
    PRBool lastModifiedHeaderIsPresent;
    rv = mResponse->ParseDateHeader(nsHTTPAtoms::Last_Modified, &lastModified,
                                    &lastModifiedHeaderIsPresent);

    // Check for corrupted, missing or malformed 'LastModified:' header
    if (NS_SUCCEEDED(rv) && lastModifiedHeaderIsPresent && !LL_IS_ZERO(lastModified)) {

        // Store value of 'Last-Modified:' header into cache entry, used for cache
        // replacement policy decisions
        mCacheEntry->SetLastModifiedTime(lastModified);
    }

    // Retrieve the value of the 'Expires:' header, if present
    PRTime expires;
    PRBool expiresHeaderIsPresent;
    rv = mResponse->ParseDateHeader(nsHTTPAtoms::Expires, &expires, &expiresHeaderIsPresent);

    // Check for corrupted 'Expires:' header
    if (NS_FAILED(rv)) return rv;

    // If there is an 'Expires:' header, tell the cache entry since it uses
    // that information for replacement policy decisions
    if (expiresHeaderIsPresent && !LL_IS_ZERO(expires)) {
        mCacheEntry->SetExpirationTime(expires);
    } else {
        
        // If there is no expires header, set a "stale time" for the cache
        // entry, a heuristic time at which the cache entry is *likely* to be
        // out-of-date with respect to the version on the server.  The cache
        // uses this information for replacement policy decisions.  However, it
        // is not used for the purpose of deciding whether or not to validate
        // cache data with the origin server.

        // Get the value of the 'Date:' response header
        PRTime date;
        PRBool dateHeaderIsPresent;
        rv = mResponse->ParseDateHeader(nsHTTPAtoms::Date, &date, &dateHeaderIsPresent);
        if (NS_FAILED(rv))
            return NS_ERROR_FAILURE;
        
        // Before proceeding, ensure that we don't have a bizarre last-modified time
        if (dateHeaderIsPresent && lastModifiedHeaderIsPresent && LL_UCMP(date, >, lastModified)) {

            // The heuristic stale time is (Date + (Date - Last-Modified)/2)
            PRTime heuristicStaleTime;
            LL_SUB(heuristicStaleTime, date, lastModified);
            LL_USHR(heuristicStaleTime, heuristicStaleTime, 1);
            LL_ADD(heuristicStaleTime, heuristicStaleTime, date);
            mCacheEntry->SetStaleTime(heuristicStaleTime);
        }
    }

    // Store the received HTTP headers in the cache
    nsCString allHeaders;
    rv = mResponse->EmitHeaders(allHeaders);
    if (NS_FAILED(rv)) return rv;
    rv = mCacheEntry->SetAnnotation("HTTP headers", allHeaders.Length()+1, allHeaders.GetBuffer());
    if (NS_FAILED(rv)) return rv;

    mCacheEntry->SetUpdateInProgress(PR_TRUE);
    // Store the HTTP content data in the cache too
    return mCacheEntry->InterceptAsyncRead(aListener, 0, aResult);
}

nsresult
nsHTTPChannel::Open(void)
{
    if (mConnected || (mState > HS_WAITING_FOR_OPEN))
        return NS_ERROR_ALREADY_CONNECTED;

    // Set up a new request observer and a response listener and pass 
    // to the transport
    nsresult rv = NS_OK;

    // If this is the first time, then add the channel to its load group
    if (mState == HS_IDLE) {
        if (mLoadGroup)
            mLoadGroup->AddChannel(this, nsnull);

        // See if there's a cache entry for the given URL
        CheckCache();

        // If the data in the cache is usable, i.e it hasn't expired, then
        // there's no need to request a socket transport.
        if (mCachedContentIsValid)
            return NS_OK;
    }
     
    // If this request was deferred because there was no available socket
    // transport, check the cache again since another HTTP request could have
    // filled in the cache entry while this request was pending.
    if (mState == HS_WAITING_FOR_OPEN) {
        CheckCache();

        // If the data in the cache is usable, i.e it hasn't expired, then
        // there's no need to request a socket transport.
        if (mCachedContentIsValid) {
            // The channel is being restarted by the HTTP protocol handler
            // and the cache data is usable, so start pumping the data from
            // the cache...
            if (!mOpenObserver) {
                rv = ReadFromCache(0, -1);
            }
            return NS_OK;
        }
    }

    rv = mHandler->RequestTransport(mURI, this, 
                                    mBufferSegmentSize, mBufferMaxSize, 
                                    getter_AddRefs(mTransport));

    if (NS_ERROR_BUSY == rv) {
        mState = HS_WAITING_FOR_OPEN;
        return NS_OK;
    }
    if (NS_FAILED(rv)) {
      // Unable to create a transport...  End the request...
      (void) ResponseCompleted(mResponseDataListener, rv, nsnull);
      return rv;
    }
    
    // pass ourself in to act as a proxy for progress callbacks
    rv = mTransport->SetNotificationCallbacks(this);
    if (NS_FAILED(rv)) {
      // Unable to create a transport...  End the request...
      (void) ResponseCompleted(mResponseDataListener, rv, nsnull);
      (void) ReleaseTransport(mTransport);
      return rv;
    }

    // Check for any modules that want to set headers before we
    // send out a request.
    NS_WITH_SERVICE(nsINetModuleMgr, pNetModuleMgr, kNetModuleMgrCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsISimpleEnumerator> pModules;
    rv = pNetModuleMgr->EnumerateModules(NS_NETWORK_MODULE_MANAGER_HTTP_REQUEST_PROGID,
                                         getter_AddRefs(pModules));
    if (NS_FAILED(rv)) return rv;

    // Go through the external modules and notify each one.
    nsCOMPtr<nsISupports> supEntry;
    rv = pModules->GetNext(getter_AddRefs(supEntry));
    while (NS_SUCCEEDED(rv)) 
    {
        nsCOMPtr<nsINetModRegEntry> entry = do_QueryInterface(supEntry, &rv);
        if (NS_FAILED(rv)) 
            return rv;

        nsCOMPtr<nsINetNotify> syncNotifier;
        entry->GetSyncProxy(getter_AddRefs(syncNotifier));
        nsCOMPtr<nsIHTTPNotify> pNotify = do_QueryInterface(syncNotifier, &rv);

        if (NS_SUCCEEDED(rv)) 
        {
            // send off the notification, and block.
            // make the nsIHTTPNotify api call
            pNotify->ModifyRequest((nsISupports*)(nsIRequest*)this);
            // we could do something with the return code from the external
            // module, but what????            
        }
        rv = pModules->GetNext(getter_AddRefs(supEntry)); // go around again
    }

    mRequest->SetTransport(mTransport);

    // if using proxy...
    nsXPIDLCString requestSpec;
    rv = mRequest->GetOverrideRequestSpec(getter_Copies(requestSpec));
    // no one has overwritten this value as yet...
    if (!requestSpec && mProxy && *mProxy)
    {
        nsXPIDLCString strurl;
        if(NS_SUCCEEDED(mURI->GetSpec(getter_Copies(strurl))))
            mRequest->SetOverrideRequestSpec(strurl);
    }

    // Check to see if an authentication header is required
    nsAuthEngine* pAuthEngine = nsnull; 
    if (NS_SUCCEEDED(mHandler->GetAuthEngine(&pAuthEngine)) && pAuthEngine)
    {
        nsXPIDLCString authStr;
        if (NS_SUCCEEDED(pAuthEngine->GetAuthString(mURI, 
                getter_Copies(authStr))))
        {
            if (authStr && *authStr)
                rv = mRequest->SetHeader(nsHTTPAtoms::Authorization, authStr);
        }

        if (mProxy && *mProxy)
        {
            nsXPIDLCString proxyAuthStr;
            if (NS_SUCCEEDED(pAuthEngine->GetProxyAuthString(mProxy, 
                            mProxyPort,
                            getter_Copies(proxyAuthStr))))
            {
                if (proxyAuthStr && *proxyAuthStr)
                    rv = mRequest->SetHeader(nsHTTPAtoms::Proxy_Authorization, 
                            proxyAuthStr);
            }
        }
    }

    rv = mRequest->WriteRequest();
    if (NS_FAILED(rv)) return rv;
    
    mState = HS_WAITING_FOR_RESPONSE;
    mConnected = PR_TRUE;

    return rv;
}


nsresult nsHTTPChannel::Redirect(const char *aNewLocation, 
                                 nsIChannel **aResult)
{
  nsresult rv;
  nsCOMPtr<nsIURI> newURI;
  nsCOMPtr<nsIChannel> channel;

  *aResult = nsnull;

  //
  // Create a new URI using the Location header and the current URL 
  // as a base ...
  //
  NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;
    
  rv = serv->NewURI(aNewLocation, mURI, getter_AddRefs(newURI));
  if (NS_FAILED(rv)) return rv;

  //
  // Move the Reference of the old location to the new one 
  // if the new one has none
  //

  nsXPIDLCString   newref;
  nsCOMPtr<nsIURL> newurl;
  
  newurl = do_QueryInterface(newURI, &rv);
  if (NS_SUCCEEDED(rv)) {
    rv = newurl->GetRef(getter_Copies(newref));
    if (NS_SUCCEEDED(rv) && !newref) {
      nsXPIDLCString   baseref;
      nsCOMPtr<nsIURL> baseurl;


      baseurl = do_QueryInterface(mURI, &rv);
      if (NS_SUCCEEDED(rv)) {

        rv = baseurl->GetRef(getter_Copies(baseref));
        if (NS_SUCCEEDED(rv) && baseref) {
          // If the old URL had a reference and the new URL does not,
          // then move it to the new URL...
          newurl->SetRef(baseref);                
        }
      }
    }
  }

#if defined(PR_LOGGING)
  char *newURLSpec;
  nsresult log_rv;

  newURLSpec = nsnull;
  log_rv = newURI->GetSpec(&newURLSpec);
  if (NS_FAILED(log_rv))
    newURLSpec = nsCRT::strdup("?");
  PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
         ("ProcessRedirect [this=%x].\tRedirecting to: %s.\n",
          this, newURLSpec));
  nsAllocator::Free(newURLSpec);
#endif /* PR_LOGGING */

  NS_WITH_SERVICE(nsIScriptSecurityManager, securityManager, 
                  NS_SCRIPTSECURITYMANAGER_PROGID, &rv);
  if (NS_FAILED(rv)) return rv;
  rv = securityManager->CheckLoadURI(mOriginalURI, newURI, PR_TRUE);
  if (NS_FAILED(rv)) return rv;

  rv = serv->NewChannelFromURI(mVerb.GetBuffer(), newURI, mLoadGroup, 
           mCallbacks, mLoadAttributes, mOriginalURI, 
           mBufferSegmentSize, mBufferMaxSize, getter_AddRefs(channel));
  if (NS_FAILED(rv)) return rv;

  // Convey the referrer if one was used for this channel to the next one-
  nsXPIDLCString referrer;
  GetRequestHeader(nsHTTPAtoms::Referer, getter_Copies(referrer));
  if (referrer && *referrer)
  {
      nsCOMPtr<nsIHTTPChannel> httpChannel = do_QueryInterface(channel);
      if (httpChannel)
          httpChannel->SetRequestHeader(nsHTTPAtoms::Referer, referrer);
  }

  // Start the redirect...
  rv = channel->AsyncRead(0, -1, mResponseContext, mResponseDataListener);

  //
  // Fire the OnRedirect(...) notification.
  //
  if (mEventSink) {
    mEventSink->OnRedirect((nsISupports*)(nsIRequest*)this, newURI);
  }

  // Null out pointers that are no longer needed...
  //
  // This will prevent the OnStopRequest(...) notification from being fired
  // for the original URL...
  //
  mResponseDataListener = 0;
  mOpenObserver = 0;

  *aResult = channel;
  NS_ADDREF(*aResult);

  return rv;
}


nsresult nsHTTPChannel::ResponseCompleted(nsIStreamListener *aListener,
                                          nsresult aStatus,
                                          const PRUnichar* aMsg)
{
  nsresult rv = NS_OK;

  //
  // First:
  //
  // Call the consumer OnStopRequest(...) to end the request...
  if (aListener) {
    rv = aListener->OnStopRequest(this, mResponseContext, aStatus, aMsg);

    if (NS_FAILED(rv)) {
      PR_LOG(gHTTPLog, PR_LOG_ERROR, 
             ("nsHTTPChannel::OnStopRequest(...) [this=%x]."
              "\tOnStopRequest to consumer failed! Status:%x\n",
              this, rv));
    }
  }

  //
  // After the consumer has been notified, remove the channel from its 
  // load group...  This will trigger an OnStopRequest from the load group.
  //
  if (mLoadGroup) {
    (void)mLoadGroup->RemoveChannel(this, nsnull, aStatus, nsnull);
  }

  //
  // Finally, notify the OpenObserver that the request has completed.
  //
  if (mOpenObserver) {
      (void) mOpenObserver->OnStopRequest(this, mOpenContext, aStatus, aMsg);
  }

  // Null out pointers that are no longer needed...

// rjc says: don't null out mResponseContext;
// it needs to be valid for the life of the channel
//  mResponseContext = 0;

  mResponseDataListener = 0;
  NS_IF_RELEASE(mCachedResponse);

  return rv;
}

nsresult nsHTTPChannel::ReleaseTransport(nsIChannel *aTransport)
{
  nsresult rv = NS_OK;
  if (aTransport) {
    (void) mRequest->ReleaseTransport(aTransport);
    rv = mHandler->ReleaseTransport(aTransport);
  }
  
  return rv;
}

nsresult nsHTTPChannel::SetResponse(nsHTTPResponse* i_pResp)
{ 
  NS_IF_RELEASE(mResponse);
  mResponse = i_pResp;
  NS_IF_ADDREF(mResponse);

  return NS_OK;
}

nsresult nsHTTPChannel::GetResponseContext(nsISupports** aContext)
{
  if (aContext) {
    *aContext = mResponseContext;
    NS_IF_ADDREF(*aContext);
    return NS_OK;
  }

  return NS_ERROR_NULL_POINTER;
}


nsresult nsHTTPChannel::Abort()
{
  // Disconnect the consumer from this response listener...  
  // This allows the entity that follows to be discarded 
  // without notifying the consumer...
  if (mHTTPServerListener) {
    mHTTPServerListener->Abort();
  }

  // Null out pointers that are no longer needed...
  //
  // This will prevent the OnStopRequest(...) notification from being fired
  // for the original URL...
  //
  mResponseDataListener = 0;
  mOpenObserver = 0;

  return NS_OK;
}


nsresult nsHTTPChannel::OnHeadersAvailable()
{
    nsresult rv = NS_OK;

    // Notify the event sink that response headers are available...
    if (mEventSink) {
        rv = mEventSink->OnHeadersAvailable((nsISupports*)(nsIRequest*)this);
        if (NS_FAILED(rv)) return rv;
    }

    // Check for any modules that want to receive headers once they've arrived.
    NS_WITH_SERVICE(nsINetModuleMgr, pNetModuleMgr, kNetModuleMgrCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsISimpleEnumerator> pModules;
    rv = pNetModuleMgr->EnumerateModules(NS_NETWORK_MODULE_MANAGER_HTTP_RESPONSE_PROGID, getter_AddRefs(pModules));
    if (NS_FAILED(rv)) return rv;

    // Go through the external modules and notify each one.
    nsCOMPtr<nsISupports> supEntry;
    rv = pModules->GetNext(getter_AddRefs(supEntry));
    while (NS_SUCCEEDED(rv)) 
    {
        nsCOMPtr<nsINetModRegEntry> entry = do_QueryInterface(supEntry, &rv);
        if (NS_FAILED(rv)) 
            return rv;

        nsCOMPtr<nsINetNotify> syncNotifier;
        entry->GetSyncProxy(getter_AddRefs(syncNotifier));
        nsCOMPtr<nsIHTTPNotify> pNotify = do_QueryInterface(syncNotifier, &rv);

        if (NS_SUCCEEDED(rv)) 
        {
            // send off the notification, and block.
            // make the nsIHTTPNotify api call
            pNotify->AsyncExamineResponse((nsISupports*)(nsIRequest*)this);
            // we could do something with the return code from the external
            // module, but what????            
        }
        rv = pModules->GetNext(getter_AddRefs(supEntry)); // go around again
    }
    return NS_OK;
}


nsresult 
nsHTTPChannel::Authenticate(const char *iChallenge, PRBool iProxyAuth)
{
    nsresult rv = NS_ERROR_FAILURE;
    nsIChannel* channel;
    if (!iChallenge)
        return NS_ERROR_NULL_POINTER;
    
    // Determine the new username password combination to use 
    char* newUserPass = nsnull;
    if (!mAuthTriedWithPrehost && !iProxyAuth) // Proxy auth's never in prehost
    {
        nsXPIDLCString prehost;
        
        if (NS_SUCCEEDED(rv = mURI->GetPreHost(getter_Copies(prehost))))
        {
            if ((const char*)prehost) {
                if (!(newUserPass = nsCRT::strdup(prehost)))
                    return NS_ERROR_OUT_OF_MEMORY;
            }
        }
    }

    // Couldnt get one from prehost or has already been tried so...ask
    if (!newUserPass || !*newUserPass)
    {
        /*
            Throw a modal dialog box asking for 
            username, password. Prefill (!?!)
        */

        if (!mPrompter)
            return rv;
        PRUnichar *user=nsnull, *passwd=nsnull;
        PRBool retval = PR_FALSE;

        //TODO localize it!
        nsAutoString message = "Enter username for "; 
         // later on change to only show realm and then host's info. 
        message += iChallenge;
        
        // Get url
         nsXPIDLCString urlCString; 
        mURI->GetHost(getter_Copies(urlCString));
        
        rv = mPrompter->PromptUsernameAndPassword(
                message.GetUnicode(),
                &user,
                &passwd,
                &retval);
       
        if (NS_SUCCEEDED(rv) && (retval))
        {
            nsAutoString temp(user);
            if (passwd) {
                temp += ':';
                temp += passwd;
            }
            CRTFREEIF(newUserPass);
            newUserPass = temp.ToNewCString();
        }
        else 
            return rv;
    }

    // Construct the auth string request header based on info provided. 
    nsXPIDLCString authString;
    // change this later to include other kinds of authentication. TODO 
    if (NS_FAILED(rv = nsBasicAuth::Authenticate(
                    mURI, 
                    NS_STATIC_CAST(const char*, iChallenge), 
                    NS_STATIC_CAST(const char*, newUserPass),
                    getter_Copies(authString))))
        return rv; // Failed to construct an authentication string.

    // Construct a new channel
    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    // For security/privacy purposes, a response to an authenticated request is
    // not cached, except perhaps in the memory cache.
    mLoadAttributes |= nsIChannel::INHIBIT_PERSISTENT_CACHING;

    // This smells like a clone function... maybe there is a 
    // benefit in doing that, think. TODO.
    rv = serv->NewChannelFromURI(mVerb.GetBuffer(), mURI, mLoadGroup, 
                                mCallbacks, 
                                mLoadAttributes, mOriginalURI, 
                                mBufferSegmentSize, mBufferMaxSize, 
                                &channel);// delibrately...
    if (NS_FAILED(rv)) return rv; 
    if (!channel)
        return NS_ERROR_OUT_OF_MEMORY;

    nsCOMPtr<nsIHTTPChannel> httpChannel = do_QueryInterface(channel);
    NS_ASSERTION(httpChannel, "Something terrible happened..!");
    if (!httpChannel)
        return rv;

    // Add the authentication header.
    httpChannel->SetRequestHeader(iProxyAuth ?
            nsHTTPAtoms::Proxy_Authorization : nsHTTPAtoms::Authorization, 
            authString);

    // Let it know that we have already tried prehost stuff...
    httpChannel->SetAuthTriedWithPrehost(PR_TRUE);

    // Fire the new request...
    rv = channel->AsyncRead(0, -1, mResponseContext, mResponseDataListener);

    // Abort the current response...  This will disconnect the consumer from
    // the response listener...  Thus allowing the entity that follows to
    // be discarded without notifying the consumer...
    Abort();

    return rv;
}

nsresult 
nsHTTPChannel::FinishedResponseHeaders(void)
{
    nsresult rv;

    if (mFiredOnHeadersAvailable)
        return NS_OK;

    rv = NS_OK;

    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
           ("nsHTTPChannel::FinishedResponseHeaders [this=%x].\n",
            this));

    if (mOpenObserver && !mFiredOpenOnStartRequest) {
        rv = mOpenObserver->OnStartRequest(this, mOpenContext);
        mFiredOpenOnStartRequest = PR_TRUE;

        // We want to defer header completion notification until the 
        // caller actually does an AsyncRead();
        if (!mResponseDataListener)
            return rv;
    }


    // Notify the consumer that headers are available...
    OnHeadersAvailable();
    mFiredOnHeadersAvailable = PR_TRUE;

    //
    // Check the status code to see if any special processing is necessary.
    //
    // If a redirect (ie. 30x) occurs, the mResponseDataListener is
    // released and a new request is issued...
    //
    return ProcessStatusCode();
}

nsresult
nsHTTPChannel::ProcessStatusCode(void)
{
    nsresult rv = NS_OK;
    PRUint32 statusCode, statusClass;

    statusCode = 0;
    mResponse->GetStatus(&statusCode);

    NS_ASSERTION(mHandler, "HTTP handler went away");
    nsAuthEngine* pEngine;
    // We know this auth challenge response was successful. Cache any
    // authentication so that future accesses can make use of it,
    // particularly other URLs loaded from the body of this response.
    if (NS_SUCCEEDED(mHandler->GetAuthEngine(&pEngine))) 
    {
        nsXPIDLCString authString;
        if (statusCode != 407)
        {
            if (mProxy && *mProxy)
            {
                rv = GetRequestHeader(nsHTTPAtoms::Proxy_Authorization,
                                getter_Copies(authString));
                if (NS_FAILED(rv)) return rv;

                pEngine->SetProxyAuthString(
                        mProxy, mProxyPort, authString);
            }

            if (statusCode != 401 && mAuthTriedWithPrehost) 
            {
                rv = GetRequestHeader(nsHTTPAtoms::Authorization,
                                getter_Copies(authString));
                pEngine->SetAuthString(mURI, authString);
            }
        }
    }

    nsCOMPtr<nsIStreamListener> listener = mResponseDataListener;

    statusClass = statusCode / 100;
    switch (statusClass) {
        //
        // Informational: 1xx
        //
    case 1:
        PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
               ("ProcessStatusCode [this=%x].\tStatus - Informational: %d.\n",
                this, statusCode));
        break;

        //
        // Successful: 2xx
        //
    case 2:
        PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
               ("ProcessStatusCode [this=%x].\tStatus - Successful: %d.\n",
                this, statusCode));
      
        // 200 (OK) and 203 (Non-authoritative success) results can be cached
        if ((statusCode == 200) || (statusCode == 203)) {
            nsCOMPtr<nsIStreamListener> listener2;
            CacheReceivedResponse(listener, getter_AddRefs(listener2));
            if (listener2) {
                listener = listener2;
            }
        }

        break;

        //
        // Redirection: 3xx
        //
    case 3:
        PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
               ("ProcessStatusCode [this=%x].\tStatus - Redirection: %d.\n",
                this, statusCode));

        // A 304 (Not Modified) response to an if-modified-since request indicates
        // that the cached response can be used.
        if (statusCode == 304) {
            rv = ProcessNotModifiedResponse(listener);
            if (NS_FAILED(rv)) return rv;
            break;
        }

        // 300 (Multiple choices) or 301 (Redirect) results can be cached
        if ((statusCode == 300) || (statusCode == 301)) {
            nsCOMPtr<nsIStreamListener> listener2;
            CacheReceivedResponse(listener, getter_AddRefs(listener2));
            if (listener2) {
                listener = listener2;
            }
        }

        rv = ProcessRedirection(statusCode);
        break;

        //
        // Client Error: 4xx
        //
    case 4:
        PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
               ("ProcessStatusCode [this=%x].\tStatus - Client Error: %d.\n",
                this, statusCode));
        if ((401 == statusCode) || (407 == statusCode)) {
            rv = ProcessAuthentication(statusCode);
        }
        break;

        //
        // Server Error: 5xo
        //
    case 5:
        PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
               ("ProcessStatusCode [this=%x].\tStatus - Server Error: %d.\n",
                this, statusCode));
        break;

        //
        // Unknown Status Code category...
        //
    default:
        PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
               ("ProcessStatusCode [this=%x].\tStatus - Unknown Status Code category: %d.\n",
                this, statusCode));
        break;
    }

    // If mResponseDataListener is null this means that the response has been
    // aborted...  So, do not update the response listener because this
    // is being discarded...
    if (mResponseDataListener && mHTTPServerListener) {
        mHTTPServerListener->SetListener(listener);
    }
    return rv;
}

nsresult
nsHTTPChannel::ProcessNotModifiedResponse(nsIStreamListener *aListener)
{
    nsresult rv;
    NS_ASSERTION(!mCachedContentIsValid, "We should never have cached a 304 response");

#if defined(PR_LOGGING)
  nsresult log_rv;
  char *URLSpec;

  log_rv = mURI->GetSpec(&URLSpec);
  if (NS_FAILED(log_rv)) {
  	URLSpec = nsCRT::strdup("?");
  }

  PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
         ("nsHTTPChannel::ProcessNotModifiedResponse [this=%x].\t"
          "Using cache copy for: %s\n",
          this, URLSpec));
  nsAllocator::Free(URLSpec);
#endif /* PR_LOGGING */

    // Orphan the current nsHTTPServerListener instance...  It will be
    // replaced with a nsHTTPCacheListener instance.
    NS_ASSERTION(mHTTPServerListener, "No nsHTTPServerResponse available!");
    if (mHTTPServerListener) {
      mHTTPServerListener->Abort();
    }

    // Update the cached headers with any more recent ones from the
    // server - see RFC2616 [13.5.3]
    nsCOMPtr<nsISimpleEnumerator> enumerator;
    rv = mResponse->GetHeaderEnumerator(getter_AddRefs(enumerator));
    if (NS_FAILED(rv)) return rv;

    mCachedResponse->UpdateHeaders(enumerator);

    // Store the updated HTTP headers in the cache
    // XXX: Should the Expires value be recaluclated too?
    nsCString allHeaders;
    rv = mCachedResponse->EmitHeaders(allHeaders);
    if (NS_FAILED(rv)) return rv;

    rv = mCacheEntry->SetAnnotation("HTTP headers", allHeaders.Length()+1, 
                                     allHeaders.GetBuffer());
    if (NS_FAILED(rv)) return rv;

    // Fake it so that HTTP headers come from cached versions
    SetResponse(mCachedResponse);

    // Create a cache transport to read the cached response...
    nsCOMPtr<nsIChannel> cacheTransport;
    rv = mCacheEntry->NewChannel(mLoadGroup, getter_AddRefs(cacheTransport));
    if (NS_FAILED(rv)) return rv;

    mRequest->SetTransport(cacheTransport);

    // Create a new HTTPCacheListener...
    nsHTTPResponseListener *cacheListener;
    cacheListener = new nsHTTPCacheListener(this);
    if (!cacheListener) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(cacheListener);

    cacheListener->SetListener(aListener);
    mResponseDataListener = aListener;

    rv = cacheTransport->AsyncRead(0, -1, mResponseContext, cacheListener);
    if (NS_FAILED(rv)) {
      ResponseCompleted(cacheListener, rv, nsnull);
    }
    NS_RELEASE(cacheListener);

    return rv;
}

nsresult
nsHTTPChannel::ProcessRedirection(PRInt32 aStatusCode)
{
  nsresult rv = NS_OK;
  nsXPIDLCString location;

  mResponse->GetHeader(nsHTTPAtoms::Location, getter_Copies(location));

  if (((301 == aStatusCode) || (302 == aStatusCode)) && (location))
  {
      nsCOMPtr<nsIChannel> channel;

      rv = Redirect(location, getter_AddRefs(channel));
      if (NS_FAILED(rv)) return rv;
      
      // Abort the current response...  This will disconnect the consumer from
      // the response listener...  Thus allowing the entity that follows to
      // be discarded without notifying the consumer...
      Abort();
  }
  return rv;
}

nsresult
nsHTTPChannel::ProcessAuthentication(PRInt32 aStatusCode)
{
    // 401 and 407 that's all we handle for now... 
    NS_ASSERTION((401 == aStatusCode) || (407 == aStatusCode),
                 "We don't handle other types of errors!"); 

    nsresult rv = NS_OK; // Let life go on...
    nsXPIDLCString challenge; // identifies the auth type and realm.
    
    if (401 == aStatusCode)
    {
        rv = GetResponseHeader(nsHTTPAtoms::WWW_Authenticate, 
                getter_Copies(challenge));
    }
    else if (407 == aStatusCode)
    {
        rv = GetResponseHeader(nsHTTPAtoms::Proxy_Authenticate, 
                getter_Copies(challenge));
    }
    else 
        return rv;
    // We can't send user-password without this challenge.
    if (NS_FAILED(rv) || !challenge || !*challenge)
        return rv;

    return Authenticate(challenge, (407 == aStatusCode));
}

// nsIProxy methods
NS_IMETHODIMP
nsHTTPChannel::GetProxyHost(char* *o_ProxyHost)
{
    return DupString(o_ProxyHost, mProxy);
}

NS_IMETHODIMP
nsHTTPChannel::SetProxyHost(const char* i_ProxyHost) 
{
    CRTFREEIF(mProxy);
    return DupString(&mProxy, i_ProxyHost);
}

NS_IMETHODIMP
nsHTTPChannel::GetProxyPort(PRInt32 *aPort)
{
    *aPort = mProxyPort;
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPChannel::SetProxyPort(PRInt32 i_ProxyPort) 
{
    mProxyPort = i_ProxyPort;
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPChannel::SetProxyRequestURI(const char * i_Spec)
{
    return mRequest ? 
        mRequest->SetOverrideRequestSpec(i_Spec) : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsHTTPChannel::GetProxyRequestURI(char * *o_Spec)
{
    return mRequest ? 
        mRequest->GetOverrideRequestSpec(o_Spec) : NS_ERROR_FAILURE;
}

NS_IMETHODIMP 
nsHTTPChannel::SetReferrer(nsIURI *referrer, PRUint32 referrerLevel)
{
    NS_ASSERTION(referrerLevel > 0, "No need to call SetReferrer with 0 level");
    if (referrerLevel == 0)
        return NS_OK;

    if (!referrer)
        return NS_ERROR_NULL_POINTER;

    // Referer - misspelled, but per the HTTP spec
    nsXPIDLCString spec;
    referrer->GetSpec(getter_Copies(spec));
    if (spec && (referrerLevel <= mHandler->ReferrerLevel()))
    {
        if ((referrerLevel == nsIHTTPChannel::REFERRER_NON_HTTP) || 
            (0 == PL_strncasecmp((const char*)spec, "http",4)))
            return SetRequestHeader(nsHTTPAtoms::Referer, spec);
    }
    return NS_OK; 
}

nsresult DupString(char* *o_Dest, const char* i_Src)
{
    if (!o_Dest)
        return NS_ERROR_NULL_POINTER;
    if (i_Src)
    {
        *o_Dest = nsCRT::strdup(i_Src);
        return (*o_Dest == nsnull) ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
    }
    else
    {
        *o_Dest = nsnull;
        return NS_OK;
    }
}

NS_IMETHODIMP 
nsHTTPChannel::GetUsingProxy(PRBool *aUsingProxy)
{
    if (!aUsingProxy)
        return NS_ERROR_NULL_POINTER;
    *aUsingProxy = (mProxy && *mProxy);
    return NS_OK;
}


NS_IMETHODIMP 
nsHTTPChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
    *aSecurityInfo = nsnull;
    
    if (!aSecurityInfo)
        return NS_ERROR_NULL_POINTER;

    if (!mTransport)
        return NS_OK;
    
    return mTransport->GetSecurityInfo(aSecurityInfo);
}
