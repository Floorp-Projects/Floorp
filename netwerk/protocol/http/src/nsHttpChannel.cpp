/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Darin Fisher <darin@netscape.com> (original author)
 */

#include "nsHttpChannel.h"
#include "nsHttpTransaction.h"
#include "nsHttpConnection.h"
#include "nsHttpHandler.h"
#include "nsHttpAuthCache.h"
#include "nsHttpResponseHead.h"
#include "nsHttp.h"
#include "nsIHttpAuthenticator.h"
#include "nsIAuthPrompt.h"
#include "nsIStringBundle.h"
#include "nsISupportsPrimitives.h"
#include "nsIFileStream.h"
#include "nsCExternalHandlerService.h"
#include "nsIMIMEService.h"
#include "nsMimeTypes.h"
#include "nsNetUtil.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIIDNService.h"
#include "plstr.h"
#include "prprf.h"
#include "nsEscape.h"

static NS_DEFINE_CID(kStreamListenerTeeCID, NS_STREAMLISTENERTEE_CID);

//-----------------------------------------------------------------------------
// nsHttpChannel <public>
//-----------------------------------------------------------------------------

nsHttpChannel::nsHttpChannel()
    : mResponseHead(nsnull)
    , mTransaction(nsnull)
    , mPrevTransaction(nsnull)
    , mConnectionInfo(nsnull)
    , mLoadFlags(LOAD_NORMAL)
    , mStatus(NS_OK)
    , mCapabilities(0)
    , mReferrerType(REFERRER_NONE)
    , mCachedResponseHead(nsnull)
    , mCacheAccess(0)
    , mPostID(0)
    , mRequestTime(0)
    , mIsPending(PR_FALSE)
    , mApplyConversion(PR_TRUE)
    , mTriedCredentialsFromPrehost(PR_FALSE)
    , mFromCacheOnly(PR_FALSE)
    , mCachedContentIsValid(PR_FALSE)
    , mResponseHeadersModified(PR_FALSE)
{
    LOG(("Creating nsHttpChannel @%x\n", this));

    NS_INIT_ISUPPORTS();

    // grab a reference to the handler to ensure that it doesn't go away.
    nsHttpHandler *handler = nsHttpHandler::get();
    NS_ADDREF(handler);
}

nsHttpChannel::~nsHttpChannel()
{
    LOG(("Destroying nsHttpChannel @%x\n", this));

    if (mResponseHead) {
        delete mResponseHead;
        mResponseHead = 0;
    }
    if (mCachedResponseHead) {
        delete mCachedResponseHead;
        mCachedResponseHead = 0;
    }

    NS_IF_RELEASE(mConnectionInfo);
    NS_IF_RELEASE(mTransaction);
    NS_IF_RELEASE(mPrevTransaction);

    // release our reference to the handler
    nsHttpHandler *handler = nsHttpHandler::get();
    NS_RELEASE(handler);
}

nsresult
nsHttpChannel::Init(nsIURI *uri,
                    PRUint8 caps,
                    nsIProxyInfo *proxyInfo)
{
    nsresult rv;

    LOG(("nsHttpChannel::Init [this=%x]\n", this));

    NS_PRECONDITION(uri, "null uri");

    mURI = uri;
    mOriginalURI = uri;
    mCapabilities = caps;

    //
    // Construct connection info object
    //
    nsXPIDLCString host;
    PRInt32 port = -1;
    PRBool usingSSL = PR_FALSE;
    
    rv = mURI->SchemeIs("https", &usingSSL);
    if (NS_FAILED(rv)) return rv;

    rv = mURI->GetHost(getter_Copies(host));
    if (NS_FAILED(rv)) return rv;

    rv = mURI->GetPort(&port);
    if (NS_FAILED(rv)) return rv;

    LOG(("host=%s port=%d\n", host.get(), port));

    nsCOMPtr<nsIIDNService> converter;
    if ((converter = nsHttpHandler::get()->IDNConverter()) &&
        !nsCRT::IsAscii(host))
    {
        nsXPIDLCString hostACE;
        rv = converter->UTF8ToIDNHostName(host.get(), getter_Copies(hostACE));
        if (NS_FAILED(rv)) return rv;

        // replace the host portion in the URL
        nsCOMPtr<nsIURL> url = do_QueryInterface(mURI);
        NS_ASSERTION(url, "mURI is not an nsIURL");
        if (url)
            url->SetHost(hostACE.get());
        // overwrite |host|
        host = hostACE;
    }
    rv = mURI->GetSpec(getter_Copies(mSpec));
    if (NS_FAILED(rv)) return rv;

    LOG(("uri=%s\n", mSpec.get()));

    mConnectionInfo = new nsHttpConnectionInfo(host, port,
                                               proxyInfo, usingSSL);
    if (!mConnectionInfo)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(mConnectionInfo);

    // make sure our load flags include this bit if this is a secure channel.
    if (usingSSL)
        mLoadFlags |= INHIBIT_PERSISTENT_CACHING;

    // Set default request method
    mRequestHead.SetMethod(nsHttp::Get);

    //
    // Set request headers
    //
    nsCString hostLine;
    if (port == -1)
        hostLine.Assign(host.get());
    else if (PL_strchr(host.get(), ':')) {
        hostLine.Assign('[');
        hostLine.Append(host.get());
        hostLine.Append(']');
    } else {
        hostLine.Assign(host.get());
        hostLine.Append(':');
        hostLine.AppendInt(port);
    }
    rv = mRequestHead.SetHeader(nsHttp::Host, hostLine.get());
    if (NS_FAILED(rv)) return rv;

    rv = nsHttpHandler::get()->
        AddStandardRequestHeaders(&mRequestHead.Headers(),
                                  caps,
                                  mConnectionInfo->UsingHttpProxy());
    if (NS_FAILED(rv)) return rv;

    // check to see if authorization headers should be included
    rv = AddAuthorizationHeaders();
    if (NS_FAILED(rv)) return rv;

    // Notify nsIHttpNotify implementations
    rv = nsHttpHandler::get()->OnModifyRequest(this);
    NS_ASSERTION(NS_SUCCEEDED(rv), "OnModifyRequest failed");

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpChannel <private>
//-----------------------------------------------------------------------------

nsresult
nsHttpChannel::Connect(PRBool firstTime)
{
    nsresult rv;

    LOG(("nsHttpChannel::Connect [this=%x]\n", this));

    // true when called from AsyncOpen
    if (firstTime) {
        PRBool delayed = PR_FALSE;

        // open a cache entry for this channel...
        rv = OpenCacheEntry(&delayed);

        if (NS_FAILED(rv)) {
            LOG(("OpenCacheEntry failed [rv=%x]\n", rv));
            // if this channel is only allowed to pull from the cache, then
            // we must fail if we were unable to open a cache entry.
            if (mFromCacheOnly)
                return mPostID ? NS_ERROR_DOCUMENT_NOT_CACHED : rv;
            // otherwise, let's just proceed without using the cache.
        }
 
        if (NS_SUCCEEDED(rv) && delayed)
            return NS_OK;
    }

    // we may or may not have a cache entry at this point
    if (mCacheEntry) {
        // inspect the cache entry to determine whether or not we need to go
        // out to net to validate it.  this call sets mCachedContentIsValid
        // and may set request headers as required for cache validation.
        rv = CheckCache();
        NS_ASSERTION(NS_SUCCEEDED(rv), "cache check failed");

        // read straight from the cache if possible...
        if (mCachedContentIsValid) {
            return ReadFromCache();
        }
        else if (mFromCacheOnly) {
            // The cache no longer contains the requested resource, and we
            // are not allowed to refetch it, so there's nothing more to do.
            // If this was a refetch of a POST transaction's resposne, then
            // this failure indicates that the response is no longer cached.
            return mPostID ? NS_ERROR_DOCUMENT_NOT_CACHED : NS_BINDING_FAILED;
        }
    }

    // hit the net...
    rv = SetupTransaction();
    if (NS_FAILED(rv)) return rv;

    return nsHttpHandler::get()->InitiateTransaction(mTransaction, mConnectionInfo);
}

// called when Connect fails
nsresult
nsHttpChannel::AsyncAbort(nsresult status)
{
    LOG(("nsHttpChannel::AsyncAbort [this=%x status=%x]\n", this, status));

    mStatus = status;
    mIsPending = PR_FALSE;

    // create an async proxy for the listener..
    nsCOMPtr<nsIProxyObjectManager> mgr;
    nsHttpHandler::get()->GetProxyObjectManager(getter_AddRefs(mgr));
    if (mgr) {
        nsCOMPtr<nsIRequestObserver> observer;
        mgr->GetProxyForObject(NS_CURRENT_EVENTQ,
                               NS_GET_IID(nsIRequestObserver),
                               mListener,
                               PROXY_ASYNC | PROXY_ALWAYS,
                               getter_AddRefs(observer));
        if (observer) {
            observer->OnStartRequest(this, mListenerContext);
            observer->OnStopRequest(this, mListenerContext, mStatus);
        }
        mListener = 0;
        mListenerContext = 0;
    }
    // XXX else, no proxy object manager... what do we do?

    // finally remove ourselves from the load group.
    if (mLoadGroup)
        mLoadGroup->RemoveRequest(this, nsnull, status);

    return NS_OK;
}

nsresult
nsHttpChannel::AsyncRedirect()
{
    nsCOMPtr<nsIEventQueueService> eqs;
    nsCOMPtr<nsIEventQueue> eventQ;

    nsHttpHandler::get()->GetEventQueueService(getter_AddRefs(eqs));
    if (eqs)
        eqs->ResolveEventQueue(NS_CURRENT_EVENTQ, getter_AddRefs(eventQ));
    if (!eventQ)
        return NS_ERROR_FAILURE;

    PLEvent *event = new PLEvent;
    if (!event)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF_THIS();

    PL_InitEvent(event, this,
            nsHttpChannel::AsyncRedirect_EventHandlerFunc,
            nsHttpChannel::AsyncRedirect_EventCleanupFunc);

    PRStatus status = eventQ->PostEvent(event);
    return status == PR_SUCCESS ? NS_OK : NS_ERROR_FAILURE;
}

void *PR_CALLBACK
nsHttpChannel::AsyncRedirect_EventHandlerFunc(PLEvent *ev)
{
    nsHttpChannel *chan =
        NS_STATIC_CAST(nsHttpChannel *, PL_GetEventOwner(ev));

    if (chan) {
        chan->HandleAsyncRedirect();
        NS_RELEASE(chan);
    }
    return nsnull;
}

void PR_CALLBACK
nsHttpChannel::AsyncRedirect_EventCleanupFunc(PLEvent *ev)
{
    delete ev;
}

void
nsHttpChannel::HandleAsyncRedirect()
{
    nsresult rv = NS_OK;

    LOG(("nsHttpChannel::HandleAsyncRedirect [this=%p]\n", this));

    // since this event is handled asynchronously, it is possible that this
    // channel could have been canceled, in which case there would be no point
    // in processing the redirect.
    if (NS_SUCCEEDED(mStatus)) {
        rv = ProcessRedirection(mResponseHead->Status());
        if (NS_FAILED(rv))
            mStatus = rv;
    }

    // close the cache entry... blow it away if we couldn't process
    // the redirect for some reason.
    CloseCacheEntry(rv);

    mIsPending = PR_FALSE;

    if (mLoadGroup)
        mLoadGroup->RemoveRequest(this, nsnull, mStatus);
}

nsresult
nsHttpChannel::SetupTransaction()
{
    NS_ENSURE_TRUE(!mTransaction, NS_ERROR_ALREADY_INITIALIZED);

    nsCOMPtr<nsIStreamListener> listenerProxy;
    nsresult rv = NS_NewStreamListenerProxy(getter_AddRefs(listenerProxy),
                                            this, nsnull,
                                            NS_HTTP_SEGMENT_SIZE,
                                            NS_HTTP_BUFFER_SIZE);
    if (NS_FAILED(rv)) return rv;

    // create the transaction object
    mTransaction = new nsHttpTransaction(listenerProxy, this, mCapabilities);
    if (!mTransaction)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(mTransaction);

    // use the URI path if not proxying (transparent proxying such as SSL proxy
    // does not count here).
    nsXPIDLCString requestURIStr;
    const char* requestURI;
    if (mConnectionInfo->UsingSSL() ||
        !mConnectionInfo->UsingHttpProxy()) {
        rv = mURI->GetPath(getter_Copies(requestURIStr));
        if (NS_FAILED(rv)) return rv;
        requestURI = requestURIStr.get();
    }
    else
        requestURI = mSpec.get();

    // trim off the #ref portion if any...
    char *p = PL_strchr(requestURI, '#');
    if (p) *p = 0;

    mRequestHead.SetVersion(nsHttpHandler::get()->DefaultVersion());
    mRequestHead.SetRequestURI(requestURI);

    // set the request time for cache expiration calculations
    mRequestTime = NowInSeconds();

    // if doing a reload, force end-to-end
    if (mLoadFlags & LOAD_BYPASS_CACHE) {
        // We need to send 'Pragma:no-cache' to inhibit proxy caching even if
        // no proxy is configured since we might be talking with a transparent
        // proxy, i.e. one that operates at the network level.  See bug #14772.
        mRequestHead.SetHeader(nsHttp::Pragma, "no-cache");
        mRequestHead.SetHeader(nsHttp::Cache_Control, "no-cache");
    }
    else if ((mLoadFlags & VALIDATE_ALWAYS) && (mCacheAccess & nsICache::ACCESS_READ)) {
        // We need to send 'Cache-Control: max-age=0' to force each cache along
        // the path to the origin server to revalidate its own entry, if any,
        // with the next cache or server.  See bug #84847.
        mRequestHead.SetHeader(nsHttp::Cache_Control, "max-age=0");
    }

    return mTransaction->SetupRequest(&mRequestHead, mUploadStream);
}

void
nsHttpChannel::ApplyContentConversions()
{
    if (!mResponseHead)
        return;

    LOG(("nsHttpChannel::ApplyContentConversions [this=%x]\n", this));

    if (!mApplyConversion) {
        LOG(("not applying conversion per mApplyConversion\n"));
        return;
    }

    const char *val = mResponseHead->PeekHeader(nsHttp::Content_Encoding);
    if (nsHttpHandler::get()->IsAcceptableEncoding(val)) {
        nsCOMPtr<nsIStreamConverterService> serv;
        nsresult rv = nsHttpHandler::get()->
                GetStreamConverterService(getter_AddRefs(serv));
        // we won't fail to load the page just because we couldn't load the
        // stream converter service.. carry on..
        if (NS_SUCCEEDED(rv)) {
            nsCOMPtr<nsIStreamListener> converter;
            nsAutoString from = NS_ConvertASCIItoUCS2(val);
            rv = serv->AsyncConvertData(from.get(),
                                        NS_LITERAL_STRING("uncompressed").get(),
                                        mListener,
                                        mListenerContext,
                                        getter_AddRefs(converter));
            if (NS_SUCCEEDED(rv)) {
                LOG(("converter installed from \'%s\' to \'uncompressed\'\n", val));
                mListener = converter;
            }
        }
    }
}

nsresult
nsHttpChannel::ProcessResponse()
{
    nsresult rv = NS_OK;
    PRUint32 httpStatus = mResponseHead->Status();

    LOG(("nsHttpChannel::ProcessResponse [this=%x httpStatus=%u]\n",
        this, httpStatus));

    // notify nsIHttpNotify implementations
    rv = nsHttpHandler::get()->OnExamineResponse(this);
    NS_ASSERTION(NS_SUCCEEDED(rv), "OnExamineResponse failed");

    // handle different server response categories
    switch (httpStatus) {
    case 200:
    case 203:
    case 206:
        rv = ProcessNormal();
        break;
    case 300:
    case 301:
    case 302:
    case 307:
        // these redirects can be cached (don't store the response body)
        if (mCacheEntry) {
            rv = InitCacheEntry();
            if (NS_SUCCEEDED(rv) && mCacheEntry) {
                // XXX we must open an output stream to force the cache service to
                // select a cache device for our entry -- bad cache service!!
                rv = mCacheEntry->GetTransport(getter_AddRefs(mCacheTransport));
                if (NS_SUCCEEDED(rv)) {
                    nsCOMPtr<nsIOutputStream> out;
                    rv = mCacheTransport->OpenOutputStream(0, PRUint32(-1), 0, getter_AddRefs(out));
                }
            }
            CloseCacheEntry(rv);
        }
        rv = ProcessRedirection(httpStatus);
        if (NS_FAILED(rv)) {
            LOG(("ProcessRedirection failed [rv=%x]\n", rv));
            rv = ProcessNormal();
        }
        break;
    case 303:
    case 305:
        // these redirects cannot be cached
        CloseCacheEntry(NS_ERROR_ABORT);
        rv = ProcessRedirection(httpStatus);
        if (NS_FAILED(rv)) {
            LOG(("ProcessRedirection failed [rv=%x]\n", rv));
            rv = ProcessNormal();
        }
        break;
    case 304:
        rv = ProcessNotModified();
        if (NS_FAILED(rv)) {
            LOG(("ProcessNotModified failed [rv=%x]\n", rv));
            rv = ProcessNormal();
        }
        break;
    case 401:
    case 407:
        rv = ProcessAuthentication(httpStatus);
        if (NS_FAILED(rv)) {
            LOG(("ProcessAuthentication failed [rv=%x]\n", rv));
            CloseCacheEntry(NS_ERROR_ABORT);
            rv = ProcessNormal();
        }
        break;
    default:
        CloseCacheEntry(NS_ERROR_ABORT);
        rv = ProcessNormal();
        break;
    }

    return rv;
}

nsresult
nsHttpChannel::ProcessNormal()
{
    nsresult rv;

    LOG(("nsHttpChannel::ProcessNormal [this=%x]\n", this));

    // For .gz files, apache sends both a Content-Type: application/x-gzip
    // as well as Content-Encoding: gzip, which is completely wrong.  In
    // this case, we choose to ignore the rogue Content-Encoding header. We
    // must do this early on so as to prevent it from being seen up stream.
    const char *encoding = mResponseHead->PeekHeader(nsHttp::Content_Encoding);
    if (encoding && PL_strcasestr(encoding, "gzip") && (
        !PL_strcmp(mResponseHead->ContentType(), APPLICATION_GZIP) ||
        !PL_strcmp(mResponseHead->ContentType(), APPLICATION_GZIP2))) {
        // clear the Content-Encoding header
        mResponseHead->SetHeader(nsHttp::Content_Encoding, nsnull); 
    }

    // this must be called before firing OnStartRequest, since http clients,
    // such as imagelib, expect our cache entry to already have the correct
    // expiration time (bug 87710).
    if (mCacheEntry) {
        rv = InitCacheEntry();
        if (NS_FAILED(rv)) return rv;
    }

    rv = mListener->OnStartRequest(this, mListenerContext);
    if (NS_FAILED(rv)) return rv;

    // install stream converter if required
    ApplyContentConversions();

    // install cache listener if we still have a cache entry open
    if (mCacheEntry)
        rv = InstallCacheListener();

    return rv;
}

//-----------------------------------------------------------------------------
// nsHttpChannel <cache>
//-----------------------------------------------------------------------------

nsresult
nsHttpChannel::ProcessNotModified()
{
    nsresult rv;

    LOG(("nsHttpChannel::ProcessNotModified [this=%x]\n", this)); 

    NS_ENSURE_TRUE(mCachedResponseHead, NS_ERROR_NOT_INITIALIZED);
    NS_ENSURE_TRUE(mCacheEntry, NS_ERROR_NOT_INITIALIZED);

    // merge any new headers with the cached response headers
    rv = mCachedResponseHead->UpdateHeaders(mResponseHead->Headers());
    if (NS_FAILED(rv)) return rv;

    // update the cached response head
    nsCAutoString head;
    mCachedResponseHead->Flatten(head, PR_TRUE);
    rv = mCacheEntry->SetMetaDataElement("response-head", head.get());
    if (NS_FAILED(rv)) return rv;

    // make the cached response be the current response
    delete mResponseHead;
    mResponseHead = mCachedResponseHead;
    mCachedResponseHead = 0;

    rv = UpdateExpirationTime();
    if (NS_FAILED(rv)) return rv;

    // drop our reference to the current transaction... ie. let it finish
    // in the background, since we can most likely reuse the connection.
    mPrevTransaction = mTransaction;
    mTransaction = nsnull;

    mCachedContentIsValid = PR_TRUE;
    return ReadFromCache();
}

nsresult
nsHttpChannel::OpenCacheEntry(PRBool *delayed)
{
    nsresult rv;

    *delayed = PR_FALSE;

    LOG(("nsHttpChannel::OpenCacheEntry [this=%x]", this));

    // make sure we're not abusing this function
    NS_PRECONDITION(!mCacheEntry, "cache entry already open");

    nsCAutoString cacheKey;

    if (mRequestHead.Method() == nsHttp::Post) {
        // If the post id is already set then this is an attempt to replay
        // a post transaction via the cache.  Otherwise, we need a unique
        // post id for this transaction.
        if (mPostID == 0)
            mPostID = nsHttpHandler::get()->GenerateUniqueID();
    }
    else if ((mRequestHead.Method() != nsHttp::Get) &&
             (mRequestHead.Method() != nsHttp::Head)) {
        // don't use the cache for other types of requests
        return NS_OK;
    }
    else if (mRequestHead.PeekHeader(nsHttp::Range)) {
        // we don't support caching for byte range requests
        return NS_OK;
    }

    GenerateCacheKey(cacheKey);

    // Get a cache session with appropriate storage policy
    nsCacheStoragePolicy storagePolicy;
    if (mLoadFlags & INHIBIT_PERSISTENT_CACHING)
        storagePolicy = nsICache::STORE_IN_MEMORY;
    else
        storagePolicy = nsICache::STORE_ANYWHERE; // allow on disk
    nsCOMPtr<nsICacheSession> session;
    rv = nsHttpHandler::get()->GetCacheSession(storagePolicy,
                                               getter_AddRefs(session));
    if (NS_FAILED(rv)) return rv;

    // Are we offline?
    PRBool offline = PR_FALSE;
    
    nsCOMPtr<nsIIOService> ioService;
    rv = nsHttpHandler::get()->GetIOService(getter_AddRefs(ioService));
    ioService->GetOffline(&offline);

    // Set the desired cache access mode accordingly...
    nsCacheAccessMode accessRequested;
    if (offline) {
        // Since we are offline, we can only read from the cache.
        accessRequested = nsICache::ACCESS_READ;
        mFromCacheOnly = PR_TRUE;
    }
    else if (mLoadFlags & LOAD_BYPASS_CACHE)
        accessRequested = nsICache::ACCESS_WRITE; // replace cache entry
    else
        accessRequested = nsICache::ACCESS_READ_WRITE; // normal browsing

    // we'll try to synchronously open the cache entry... however, it may be
    // in use and not yet validated, in which case we'll try asynchronously
    // opening the cache entry.
    rv = session->OpenCacheEntry(cacheKey.get(), accessRequested, PR_FALSE,
                                 getter_AddRefs(mCacheEntry));
    if (rv == NS_ERROR_CACHE_WAIT_FOR_VALIDATION) {
        // access to the cache entry has been denied
        rv = session->AsyncOpenCacheEntry(cacheKey.get(), accessRequested, this);
        if (NS_FAILED(rv)) return rv;
        // we'll have to wait for the cache entry
        *delayed = PR_TRUE;
    }
    else if (rv == NS_OK) {
        mCacheEntry->GetAccessGranted(&mCacheAccess);
        LOG(("got cache entry [access=%x]\n", mCacheAccess));
    }
    return rv;
}

nsresult
nsHttpChannel::GenerateCacheKey(nsACString &cacheKey)
{
    cacheKey.SetLength(0);
    if (mPostID) {
        char buf[32];
        PR_snprintf(buf, sizeof(buf), "%x", mPostID);
        cacheKey.Append("id=");
        cacheKey.Append(buf);
        cacheKey.Append("&uri=");
    }
    // Strip any trailing #ref from the URL before using it as the key
    const char *p = PL_strchr(mSpec, '#');
    if (p)
        cacheKey.Append(mSpec, p - mSpec);
    else
        cacheKey.Append(mSpec);
    return NS_OK;
}

// UpdateExpirationTime is called when a new response comes in from the server.
// It updates the stored response-time and sets the expiration time on the
// cache entry.  
//
// From section 13.2.4 of RFC2616, we compute expiration time as follows:
//
//    timeRemaining = freshnessLifetime - currentAge
//    expirationTime = now + timeRemaining
// 
nsresult
nsHttpChannel::UpdateExpirationTime()
{
    PRUint32 now = NowInSeconds(), timeRemaining = 0;

    NS_ENSURE_TRUE(mResponseHead, NS_ERROR_FAILURE);

    if (!mResponseHead->MustRevalidate()) {
        nsresult rv;
        PRUint32 freshnessLifetime, currentAge;

        rv = mResponseHead->ComputeCurrentAge(now, mRequestTime, &currentAge); 
        if (NS_FAILED(rv)) return rv;

        rv = mResponseHead->ComputeFreshnessLifetime(&freshnessLifetime);
        if (NS_FAILED(rv)) return rv;

        LOG(("freshnessLifetime = %u, currentAge = %u\n",
            freshnessLifetime, currentAge));

        if (freshnessLifetime > currentAge)
            timeRemaining = freshnessLifetime - currentAge;
    }
    return mCacheEntry->SetExpirationTime(now + timeRemaining);
}

// CheckCache is called from Connect after a cache entry has been opened for
// this URL but before going out to net.  It's purpose is to set or clear the 
// mCachedContentIsValid flag, and to configure an If-Modified-Since request
// if validation is required.
nsresult
nsHttpChannel::CheckCache()
{
    nsresult rv = NS_OK;

    LOG(("nsHTTPChannel::CheckCache [this=%x entry=%x]",
        this, mCacheEntry.get()));
		
    // Be pessimistic: assume the cache entry has no useful data.
    mCachedContentIsValid = PR_FALSE;

    // Don't proceed unless we have opened a cache entry for reading.
    if (!mCacheEntry || !(mCacheAccess & nsICache::ACCESS_READ))
        return NS_OK;

    nsXPIDLCString buf;

    // Get the method that was used to generate the cached response
    rv = mCacheEntry->GetMetaDataElement("request-method", getter_Copies(buf));
    if (NS_FAILED(rv)) return rv;

    nsHttpAtom method = nsHttp::ResolveAtom(buf);
    if (method == nsHttp::Head) {
        // The cached response does not contain an entity.  We can only reuse
        // the response if the current request is also HEAD.
        if (mRequestHead.Method() != nsHttp::Head)
            return NS_OK;
    }
    buf.Adopt(0);

    // Get the cached HTTP response headers
    rv = mCacheEntry->GetMetaDataElement("response-head", getter_Copies(buf));
    if (NS_FAILED(rv)) return rv;

    // Parse the cached HTTP response headers
    NS_ASSERTION(!mCachedResponseHead, "memory leak detected");
    mCachedResponseHead = new nsHttpResponseHead();
    if (!mCachedResponseHead)
        return NS_ERROR_OUT_OF_MEMORY;
    rv = mCachedResponseHead->Parse((char *) buf.get());
    if (NS_FAILED(rv)) return rv;
    buf.Adopt(0);

    // If we were only granted read access, then assume the entry is valid.
    if (mCacheAccess == nsICache::ACCESS_READ) {
        mCachedContentIsValid = PR_TRUE;
        return NS_OK;
    }

    // If the cached content-length is set and it does not match the data size
    // of the cached content, then refetch.
    PRInt32 contentLength = mCachedResponseHead->ContentLength();
    if (contentLength != -1) {
        PRUint32 size;
        rv = mCacheEntry->GetDataSize(&size);
        if (NS_FAILED(rv)) return rv;

        if (size != (PRUint32) contentLength) {
            LOG(("Cached data size does not match the Content-Length header "
                 "[content-length=%u size=%u]\n", contentLength, size));
            // looks like a partial entry.
            // XXX must re-fetch until we learn how to do byte range requests.
            return NS_OK;
        }
    }

    PRBool doValidation = PR_FALSE;

    // Be optimistic: assume that we won't need to do validation
    mRequestHead.SetHeader(nsHttp::If_Modified_Since, nsnull);
    mRequestHead.SetHeader(nsHttp::If_None_Match, nsnull);

    // If the LOAD_FROM_CACHE flag is set, any cached data can simply be used.
    if (mLoadFlags & LOAD_FROM_CACHE) {
        LOG(("NOT validating based on LOAD_FROM_CACHE load flag\n"));
        doValidation = PR_FALSE;
        goto end;
    }

    // If the VALIDATE_ALWAYS flag is set, any cached data won't be used until
    // it's revalidated with the server.
    if (mLoadFlags & VALIDATE_ALWAYS) {
        LOG(("Validating based on VALIDATE_ALWAYS load flag\n"));
        doValidation = PR_TRUE;
        goto end;
    }

    // check revalidation is strictly required.
    if (mCachedResponseHead->MustRevalidate()) {
        doValidation = PR_TRUE;
        goto end;
    }

    // delay checking this flag until we've verified that the response headers
    // do not require mandatory revalidation.
    if (mLoadFlags & VALIDATE_NEVER) {
        LOG(("Not validating based on VALIDATE_NEVER flag\n"));
        doValidation = PR_FALSE;
        goto end;
    }

    // Check if the cache entry has expired...
    {
        PRUint32 time = 0; // a temporary variable for storing time values...

        rv = mCacheEntry->GetExpirationTime(&time);
        if (NS_FAILED(rv)) return rv;

        if (NowInSeconds() <= time)
            doValidation = PR_FALSE;
        else if (mLoadFlags & VALIDATE_ONCE_PER_SESSION) {
            // If the cached response does not include expiration infor-
            // mation, then we must validate the response, despite whether
            // or not this is the first access this session.  This behavior
            // is consistent with existing browsers and is generally expected
            // by web authors.
            rv = mCachedResponseHead->ComputeFreshnessLifetime(&time);
            if (NS_FAILED(rv)) return rv;

            if (time == 0) {
                doValidation = PR_TRUE;
            } 
            else {
                rv = mCacheEntry->GetLastModified(&time);
                if (NS_FAILED(rv)) return rv;
                // Determine if this is the first time that this cache entry
                // has been accessed in this session, and validate if so.
                doValidation = (nsHttpHandler::get()->SessionStartTime() > time);
            }

        }
        else
            doValidation = PR_TRUE;

        LOG(("%salidating based on expiration time\n", doValidation ? "V" : "Not v"));
    }

end:
    mCachedContentIsValid = !doValidation;

    if (doValidation) {
        const char *val;
        // Add If-Modified-Since header if a Last-Modified was given
        val = mCachedResponseHead->PeekHeader(nsHttp::Last_Modified);
        if (val)
            mRequestHead.SetHeader(nsHttp::If_Modified_Since, val);

        // Add If-None-Match header if an ETag was given in the response
        val = mCachedResponseHead->PeekHeader(nsHttp::ETag);
        if (val)
            mRequestHead.SetHeader(nsHttp::If_None_Match, val);
    }

    LOG(("CheckCache [this=%x doValidation=%d]\n", this, doValidation));
    return NS_OK;
}

// If the data in the cache hasn't expired, then there's no need to
// talk with the server, not even to do an if-modified-since.  This
// method creates a stream from the cache, synthesizing all the various
// channel-related events.
nsresult
nsHttpChannel::ReadFromCache()
{
    nsresult rv;

    NS_ENSURE_TRUE(mCacheEntry, NS_ERROR_FAILURE);
    NS_ENSURE_TRUE(mCachedContentIsValid, NS_ERROR_FAILURE);

    LOG(("nsHttpChannel::ReadFromCache [this=%x] "
         "Using cached copy of: %s\n", this, mSpec.get()));

    if (mCachedResponseHead) {
        NS_ASSERTION(!mResponseHead, "memory leak");
        mResponseHead = mCachedResponseHead;
        mCachedResponseHead = 0;
    }

    // if we don't already have security info, try to get it from the cache 
    // entry. there are two cases to consider here: 1) we are just reading
    // from the cache, or 2) this may be due to a 304 not modified response,
    // in which case we could have security info from a socket transport.
    if (!mSecurityInfo)
        mCacheEntry->GetSecurityInfo(getter_AddRefs(mSecurityInfo));

    if (mCacheAccess & nsICache::ACCESS_WRITE) {
        // We have write access to the cache, but we don't need to go to the
        // server to validate at this time, so just mark the cache entry as
        // valid in order to allow others access to this cache entry.
        mCacheEntry->MarkValid();
    }

    // if this is a cached redirect, we must process the redirect asynchronously
    // since AsyncOpen may not have returned yet.
    if (mResponseHead && (mResponseHead->Status() / 100 == 3))
        return AsyncRedirect();

    // Get a transport to the cached data...
    rv = mCacheEntry->GetTransport(getter_AddRefs(mCacheTransport));
    if (NS_FAILED(rv)) return rv;

    // Hookup the notification callbacks interface to the new transport...
    mCacheTransport->SetNotificationCallbacks(this, 
                                  ((mLoadFlags & nsIRequest::LOAD_BACKGROUND) 
                                    ? nsITransport::DONT_REPORT_PROGRESS 
                                    : 0));

    // Pump the cache data downstream
    return mCacheTransport->AsyncRead(this, mListenerContext,
                                      0, PRUint32(-1), 0,
                                      getter_AddRefs(mCacheReadRequest));
}

nsresult
nsHttpChannel::CloseCacheEntry(nsresult status)
{
    nsresult rv = NS_OK;
    if (mCacheEntry) {
        LOG(("nsHttpChannel::CloseCacheEntry [this=%x status=%x]", this, status));

        if (NS_FAILED(status) && (mCacheAccess & nsICache::ACCESS_WRITE)) {
            LOG(("dooming cache entry!!"));
            rv = mCacheEntry->Doom();
        }

        if (mCachedResponseHead) {
            delete mCachedResponseHead;
            mCachedResponseHead = 0;
        }

        mCacheReadRequest = 0;
        mCacheTransport = 0;
        mCacheEntry = 0;
        mCacheAccess = 0;
    }
    return rv;
}

// Initialize the cache entry for writing.
//  - finalize storage policy
//  - store security info
//  - update expiration time
//  - store headers and other meta data
nsresult
nsHttpChannel::InitCacheEntry()
{
    const char *val;
    nsresult rv;

    NS_ENSURE_TRUE(mCacheEntry, NS_ERROR_UNEXPECTED);
    NS_ENSURE_TRUE(mCacheAccess & nsICache::ACCESS_WRITE, NS_ERROR_UNEXPECTED);

    // Don't cache the response again if already cached...
    if (mCachedContentIsValid)
        return NS_OK;

    LOG(("nsHttpChannel::InitCacheEntry [this=%x entry=%x]\n",
        this, mCacheEntry.get()));

    // XXX blow away any existing cache meta data

    // The no-store directive within the 'Cache-Control:' header indicates
    // that we should not store the response in the cache.
    val = mResponseHead->PeekHeader(nsHttp::Cache_Control);
    if (val && PL_strcasestr(val, "no-store")) {
        LOG(("Inhibiting caching because of \"%s\"\n", val));
        CloseCacheEntry(NS_ERROR_ABORT);
        return NS_OK;
    }

    // For HTTPS transactions, the storage policy will already be IN_MEMORY.
    // We are concerned instead about load attributes which may have changed.
    if (mLoadFlags & INHIBIT_PERSISTENT_CACHING) {
        rv = mCacheEntry->SetStoragePolicy(nsICache::STORE_IN_MEMORY);
        if (NS_FAILED(rv)) return rv;
    }

    // Store secure data in memory only
    if (mSecurityInfo)
        mCacheEntry->SetSecurityInfo(mSecurityInfo);

    // Set the expiration time for this cache entry
    rv = UpdateExpirationTime();
    if (NS_FAILED(rv)) return rv;

    // Store the HTTP request method with the cache entry so we can distinguish
    // for example GET and HEAD responses.
    rv = mCacheEntry->SetMetaDataElement("request-method", mRequestHead.Method().get());
    if (NS_FAILED(rv)) return rv;

    // Store the received HTTP head with the cache entry as an element of
    // the meta data.
    nsCAutoString head;
    mResponseHead->Flatten(head, PR_TRUE);
    return mCacheEntry->SetMetaDataElement("response-head", head.get());
}

// Finalize the cache entry
//  - may need to rewrite response headers if any headers changed
//  - may need to recalculate the expiration time if any headers changed
//  - called only for freshly written cache entries
nsresult
nsHttpChannel::FinalizeCacheEntry()
{
    LOG(("nsHttpChannel::FinalizeCacheEntry [this=%x]\n", this));

    if (mResponseHead && mResponseHeadersModified) {
        // The no-store directive within the 'Cache-Control:' header indicates
        // that we should not store the response in the cache.
        // XXX this should probably be done from within SetResponseHeader.
        const char *val = mResponseHead->PeekHeader(nsHttp::Cache_Control);
        if (val && PL_strcasestr(val, "no-store")) {
            LOG(("Dooming cache entry because of \"%s\"\n", val));
            mCacheEntry->Doom();
            return NS_OK;
        }

        // Set the expiration time for this cache entry
        nsresult rv = UpdateExpirationTime();
        if (NS_FAILED(rv)) return rv;
    }
    return NS_OK;
}

// Open an output stream to the cache entry and insert a listener tee into
// the chain of response listeners.
nsresult
nsHttpChannel::InstallCacheListener()
{
    nsresult rv;

    LOG(("Preparing to write data into the cache [uri=%s]\n", mSpec.get()));

    rv = mCacheEntry->GetTransport(getter_AddRefs(mCacheTransport));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIOutputStream> out;
    rv = mCacheTransport->OpenOutputStream(0, PRUint32(-1), 0, getter_AddRefs(out));
    if (NS_FAILED(rv)) return rv;

    // XXX disk cache does not support overlapped i/o yet
#if 0
    // Mark entry valid inorder to allow simultaneous reading...
    rv = mCacheEntry->MarkValid();
    if (NS_FAILED(rv)) return rv;
#endif

    nsCOMPtr<nsIStreamListenerTee> tee =
        do_CreateInstance(kStreamListenerTeeCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = tee->Init(mListener, out);
    if (NS_FAILED(rv)) return rv;

    mListener = do_QueryInterface(tee, &rv);
    return rv;
}

//-----------------------------------------------------------------------------
// nsHttpChannel <redirect>
//-----------------------------------------------------------------------------

nsresult
nsHttpChannel::ProcessRedirection(PRUint32 redirectType)
{
    LOG(("nsHttpChannel::ProcessRedirection [this=%x type=%u]\n",
        this, redirectType));

    const char *location = mResponseHead->PeekHeader(nsHttp::Location);

    // if a location header was not given, then we can't perform the redirect,
    // so just carry on as though this were a normal response.
    if (!location)
        return NS_ERROR_FAILURE;

    LOG(("redirecting to: %s\n", location));

    nsresult rv;
    nsCOMPtr<nsIChannel> newChannel;

    if (redirectType == 305) {
        // we must repeat the request via the proxy specified by location
 
        PRInt32 proxyPort;
        
        // location is of the form "host:port"
        char *p = PL_strchr(location, ':');
        if (p) {
            *p = 0;
            proxyPort = atoi(p+1);
        }
        else
            proxyPort = 80;

        nsCOMPtr<nsIProxyInfo> pi;
        rv = NS_NewProxyInfo("http", location, proxyPort, getter_AddRefs(pi));

        // talk to the http handler directly for this case
        rv = nsHttpHandler::get()->
                NewProxiedChannel(mURI, pi, getter_AddRefs(newChannel));
        if (NS_FAILED(rv)) return rv;
    }
    else {
        // create a new URI using the location header and the current URL
        // as a base...
        nsCOMPtr<nsIIOService> ioService;
        rv = nsHttpHandler::get()->GetIOService(getter_AddRefs(ioService));

        nsCOMPtr<nsIURI> newURI;
        rv = ioService->NewURI(location, mURI, getter_AddRefs(newURI));
        if (NS_FAILED(rv)) return rv;

        // move the reference of the old location to the new one if the new
        // one has none.
        nsCOMPtr<nsIURL> newURL = do_QueryInterface(newURI, &rv);
        if (NS_SUCCEEDED(rv)) {
            nsXPIDLCString ref;
            rv = newURL->GetRef(getter_Copies(ref));
            if (NS_SUCCEEDED(rv) && !ref) {
                nsCOMPtr<nsIURL> baseURL = do_QueryInterface(mURI, &rv);
                if (NS_SUCCEEDED(rv)) {
                    baseURL->GetRef(getter_Copies(ref));
                    if (ref)
                        newURL->SetRef(ref);
                }
            }
        }

        // build the new channel
        rv = NS_OpenURI(getter_AddRefs(newChannel), newURI, ioService, mLoadGroup,
                        mCallbacks, mLoadFlags | LOAD_REPLACE);
        if (NS_FAILED(rv)) return rv;
    }

    // convey the original uri
    rv = newChannel->SetOriginalURI(mOriginalURI);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(newChannel);
    if (httpChannel) {
        // convey the referrer if one was used for this channel to the next one
        if (mReferrer)
            httpChannel->SetReferrer(mReferrer, mReferrerType);
        // convey the mApplyConversion flag (bug 91862)
        httpChannel->SetApplyConversion(mApplyConversion);
    }

    // call out to the event sink to notify it of this redirection.
    if (mHttpEventSink) {
        rv = mHttpEventSink->OnRedirect(this, newChannel);
        if (NS_FAILED(rv)) return rv;
    }
    // XXX we used to talk directly with the script security manager, but that
    // should really be handled by the event sink implementation.

    // begin loading the new channel
    rv = newChannel->AsyncOpen(mListener, mListenerContext);
    if (NS_FAILED(rv)) return rv;

    // set redirect status
    mStatus = NS_BINDING_REDIRECTED;

    // close down this transaction (null if processing a cached redirect)
    if (mTransaction)
        mTransaction->Cancel(NS_BINDING_REDIRECTED);
    
    // disconnect from our listener
    mListener = 0;
    mListenerContext = 0;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpChannel <auth>
//-----------------------------------------------------------------------------

nsresult
nsHttpChannel::ProcessAuthentication(PRUint32 httpStatus)
{
    LOG(("nsHttpChannel::ProcessAuthentication [this=%x code=%u]\n",
        this, httpStatus));

    const char *challenge;
    PRBool proxyAuth = (httpStatus == 407);

    if (proxyAuth)
        challenge = mResponseHead->PeekHeader(nsHttp::Proxy_Authenticate);
    else
        challenge = mResponseHead->PeekHeader(nsHttp::WWW_Authenticate);

    if (!challenge) {
        LOG(("null challenge!\n"));
        return NS_ERROR_UNEXPECTED;
    }

    LOG(("challenge=%s\n", challenge));

    nsCAutoString creds;
    nsresult rv = GetCredentials(challenge, proxyAuth, creds);
    if (NS_FAILED(rv)) return rv;

    // set the authentication credentials
    if (proxyAuth)
        mRequestHead.SetHeader(nsHttp::Proxy_Authorization, creds.get());
    else
        mRequestHead.SetHeader(nsHttp::Authorization, creds.get());

    // kill off the current transaction
    mTransaction->Cancel(NS_BINDING_REDIRECTED);
    mPrevTransaction = mTransaction;
    mTransaction = nsnull;

    // toggle mIsPending to allow nsIHttpNotify implementations to modify
    // the request headers (bug 95044).
    mIsPending = PR_FALSE;

    // notify nsIHttpNotify implementations.. the server response could
    // have included cookies that must be sent with this authentication
    // attempt (bug 84794).
    rv = nsHttpHandler::get()->OnModifyRequest(this);
    NS_ASSERTION(NS_SUCCEEDED(rv), "OnModifyRequest failed");
   
    mIsPending = PR_TRUE;

    // and create a new one...
    rv = SetupTransaction();
    if (NS_FAILED(rv)) return rv;

    // rewind the upload stream
    if (mUploadStream) {
        nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(mUploadStream);
        if (seekable)
            seekable->Seek(nsISeekableStream::NS_SEEK_SET, 0);
        else {
            // try nsIRandomAccessStore
            nsCOMPtr<nsIRandomAccessStore> ras = do_QueryInterface(mUploadStream);
            if (ras)
                ras->Seek(PR_SEEK_SET, 0);
        }
    }

    rv = nsHttpHandler::get()->InitiateTransaction(mTransaction, mConnectionInfo);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

nsresult
nsHttpChannel::GetCredentials(const char *challenges,
                              PRBool proxyAuth,
                              nsAFlatCString &creds)
{
    nsAutoString user, pass;
    nsresult rv;
    
    LOG(("nsHttpChannel::GetCredentials [this=%x proxyAuth=%d challenges=%s]\n",
        this, proxyAuth, challenges));

    nsHttpAuthCache *authCache = nsHttpHandler::get()->AuthCache();
    if (!authCache)
        return NS_ERROR_NOT_INITIALIZED;

    // proxy auth's never in prehost
    if (!mTriedCredentialsFromPrehost && !proxyAuth) {
        rv = GetUserPassFromURI(user, pass);
        if (NS_FAILED(rv)) return rv;
        mTriedCredentialsFromPrehost = PR_TRUE;
    }

    // figure out which challenge we can handle and which authenticator to use.
    nsCAutoString challenge;
    nsCOMPtr<nsIHttpAuthenticator> auth;

    rv = SelectChallenge(challenges, challenge, getter_AddRefs(auth));

    if (!auth) {
        LOG(("authentication type not supported\n"));
        return NS_ERROR_FAILURE;
    }

    nsCAutoString realm;
    rv = ParseRealm(challenge.get(), realm);
    if (NS_FAILED(rv)) return rv;

    const char *triedCreds = nsnull;
    const char *host;
    nsXPIDLCString path;
    PRInt32 port;

    if (proxyAuth) {
        host = mConnectionInfo->ProxyHost();
        port = mConnectionInfo->ProxyPort();
        triedCreds = mRequestHead.PeekHeader(nsHttp::Proxy_Authorization);
    }
    else {
        host = mConnectionInfo->Host();
        port = mConnectionInfo->Port();
        triedCreds = mRequestHead.PeekHeader(nsHttp::Authorization);

        rv = GetCurrentPath(getter_Copies(path));
        if (NS_FAILED(rv)) return rv;
    }

    //
    // if we already tried some credentials for this transaction, then
    // we need to possibly clear them from the cache, unless the credentials
    // in the cache have changed, in which case we'd want to give them a
    // try instead.
    //
    authCache->GetCredentialsForDomain(host, port, realm.get(), creds);

    if (triedCreds && !PL_strcmp(triedCreds, creds.get())) {
        // ok.. clear the credentials from the cache
        authCache->SetCredentials(host, port, nsnull, realm.get(), nsnull);
        creds.Truncate(0);
    }
    // otherwise, let's try the credentials we got from the cache

    if (!creds.IsEmpty()) {
        LOG(("using cached credentials!\n"));
        return NS_OK;
    }

    if (user.IsEmpty()) {
        // at this point we are forced to interact with the user to get their
        // username and password for this domain.
        rv = PromptForUserPass(host, port, proxyAuth, realm.get(), user, pass);
        if (NS_FAILED(rv)) return rv;
    }

    // talk to the authenticator to get credentials for this user/pass combo.
    nsXPIDLCString result;
    rv = auth->GenerateCredentials(this,
                                   challenge.get(),
                                   user.get(),
                                   pass.get(),
                                   getter_Copies(result));
    if (NS_FAILED(rv)) return rv;

    creds.Assign(result);

    // store these credentials in the cache.  we do this even though we don't
    // yet know that these credentials are valid b/c we need to avoid prompting
    // the user more than once in case the credentials are valid.
    return authCache->SetCredentials(host, port, path, realm.get(), creds.get());
}

nsresult
nsHttpChannel::SelectChallenge(const char *challenges,
                               nsAFlatCString &challenge,
                               nsIHttpAuthenticator **auth)
{
    nsCAutoString scheme;

    LOG(("nsHttpChannel::SelectChallenge [this=%x]\n", this));

    // loop over the various challenges (LF separated)...
    for (const char *eol = challenges - 1; eol; ) {
        const char *p = eol + 1;

        // get the challenge string
        if ((eol = PL_strchr(p, '\n')) != nsnull)
            challenge.Assign(p, eol - p);
        else
            challenge.Assign(p);

        // get the challenge type
        if ((p = PL_strchr(challenge.get(), ' ')) != nsnull)
            scheme.Assign(challenge.get(), p - challenge.get());
        else
            scheme.Assign(challenge);

        // normalize to lowercase
        ToLowerCase(scheme);

        if (NS_SUCCEEDED(GetAuthenticator(scheme.get(), auth)))
            return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

nsresult
nsHttpChannel::GetAuthenticator(const char *scheme, nsIHttpAuthenticator **auth)
{
    LOG(("nsHttpChannel::GetAuthenticator [this=%x scheme=%s]\n", this, scheme));

    nsCAutoString contractid;
    contractid.Assign(NS_HTTP_AUTHENTICATOR_CONTRACTID_PREFIX);
    contractid.Append(scheme);

    nsresult rv;
    nsCOMPtr<nsIHttpAuthenticator> serv = do_GetService(contractid.get(), &rv);
    if (NS_FAILED(rv)) return rv;

    *auth = serv;
    NS_ADDREF(*auth);
    return NS_OK;
}

nsresult
nsHttpChannel::GetUserPassFromURI(nsAString &user,
                                  nsAString &pass)
{
    LOG(("nsHttpChannel::GetUserPassFromURI [this=%x]\n", this));

    // XXX should be a necko utility function 
    nsXPIDLCString prehost;
    mURI->GetPreHost(getter_Copies(prehost));
    if (prehost) {
        nsresult rv;

        nsXPIDLCString buf;
        rv = nsStdUnescape((char*)prehost.get(), getter_Copies(buf));
        if (NS_FAILED(rv)) return rv;

        char *p = PL_strchr(buf, ':');
        if (p) {
            // user:pass
            *p = 0;
            user = NS_ConvertASCIItoUCS2(buf);
            pass = NS_ConvertASCIItoUCS2(p+1);
        }
        else {
            // user
            user = NS_ConvertASCIItoUCS2(buf);
        }
    }
    return NS_OK;
}

nsresult
nsHttpChannel::ParseRealm(const char *challenge, nsACString &realm)
{
    //
    // From RFC2617 section 1.2, the realm value is defined as such:
    //
    //    realm       = "realm" "=" realm-value
    //    realm-value = quoted-string
    //
    // but, we'll accept anything after the the "=" up to the first space, or
    // end-of-line, if the string is not quoted.
    //
    const char *p = PL_strcasestr(challenge, "realm=");
    if (p) {
        p += 6;
        if (*p == '"')
            p++;
        const char *end = PL_strchr(p, '"');
        if (!end)
            end = PL_strchr(p, ' ');
        if (end)
            realm.Assign(p, end - p);
        else
            realm.Assign(p);
    }
    return NS_OK;
}

nsresult
nsHttpChannel::PromptForUserPass(const char *host,
                                 PRInt32 port,
                                 PRBool proxyAuth,
                                 const char *realm,
                                 nsAString &user,
                                 nsAString &pass)
{
    LOG(("nsHttpChannel::PromptForUserPass [this=%x realm=%s]\n", this, realm));

    nsresult rv;
    nsCOMPtr<nsIAuthPrompt> authPrompt = do_GetInterface(mCallbacks, &rv); 
    if (NS_FAILED(rv)) {
        NS_WARNING("notification callbacks should provide nsIAuthPrompt");
        return rv;
    }

    // construct the domain string
    nsCAutoString domain;
    domain.Assign(host);
    domain.Append(':');
    domain.AppendInt(port);

    nsAutoString hostU = NS_ConvertASCIItoUCS2(domain);

    domain.Append(" (");
    domain.Append(realm);
    domain.Append(')');

    // construct the message string
    nsCOMPtr<nsIStringBundleService> bundleSvc =
            do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIStringBundle> bundle;
    rv = bundleSvc->CreateBundle(NECKO_MSGS_URL, getter_AddRefs(bundle));
    if (NS_FAILED(rv)) return rv;

    // figure out what message to display...
    nsXPIDLString message;
    if (proxyAuth) {
        const PRUnichar *strings[] = { hostU.get() };
        rv = bundle->FormatStringFromName(
                        NS_LITERAL_STRING("EnterUserPasswordForProxy").get(),
                        strings, 1,
                        getter_Copies(message));
    }
    else {
        nsAutoString realmU;
        realmU.Assign(NS_LITERAL_STRING("\""));
        realmU.AppendWithConversion(realm);
        realmU.Append(NS_LITERAL_STRING("\""));

        const PRUnichar *strings[] = { realmU.get(), hostU.get() };
        rv = bundle->FormatStringFromName(
                        NS_LITERAL_STRING("EnterUserPasswordForRealm").get(),
                        strings, 2,
                        getter_Copies(message));
    }
    if (NS_FAILED(rv)) return rv;

    // prompt the user...
    nsXPIDLString userBuf, passBuf;
    PRBool retval = PR_FALSE;
    rv = authPrompt->PromptUsernameAndPassword(nsnull,
                                               message.get(),
                                               NS_ConvertASCIItoUCS2(domain).get(),
                                               nsIAuthPrompt::SAVE_PASSWORD_PERMANENTLY,
                                               getter_Copies(userBuf),
                                               getter_Copies(passBuf),
                                               &retval);
    if (NS_FAILED(rv))
        return rv;
    if (!retval)
        return NS_ERROR_ABORT;

    user.Assign(userBuf);
    pass.Assign(passBuf);
    return NS_OK;
}

nsresult
nsHttpChannel::AddAuthorizationHeaders()
{
    LOG(("nsHttpChannel::AddAuthorizationHeaders [this=%x]\n", this));
    nsHttpAuthCache *authCache = nsHttpHandler::get()->AuthCache();
    if (authCache) {
        nsCAutoString creds;
        nsCAutoString realm;
        nsresult rv;

        // check if proxy credentials should be sent
        const char *proxyHost = mConnectionInfo->ProxyHost();
        if (proxyHost) {
            rv = authCache->GetCredentialsForPath(proxyHost,
                                                  mConnectionInfo->ProxyPort(),
                                                  nsnull, realm, creds);
            if (NS_SUCCEEDED(rv)) {
                LOG(("adding Proxy-Authorization request header\n"));
                mRequestHead.SetHeader(nsHttp::Proxy_Authorization, creds.get());
            }
        }

        // check if server credentials should be sent
        nsXPIDLCString path;
        rv = GetCurrentPath(getter_Copies(path));
        if (NS_FAILED(rv)) return rv;

        rv = authCache->GetCredentialsForPath(mConnectionInfo->Host(),
                                              mConnectionInfo->Port(),
                                              path.get(),
                                              realm,
                                              creds);
        if (NS_SUCCEEDED(rv)) {
            LOG(("adding Authorization request header\n"));
            mRequestHead.SetHeader(nsHttp::Authorization, creds.get());
        }
    }
    return NS_OK;
}

nsresult
nsHttpChannel::GetCurrentPath(char **path)
{
    nsresult rv;
    nsCOMPtr<nsIURL> url = do_QueryInterface(mURI);
    if (url)
        rv = url->GetDirectory(path);
    else
        rv = mURI->GetPath(path);
    return rv;
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_THREADSAFE_ISUPPORTS10(nsHttpChannel,
                              nsIRequest,
                              nsIChannel,
                              nsIRequestObserver,
                              nsIStreamListener,
                              nsIHttpChannel,
                              nsIInterfaceRequestor,
                              nsIProgressEventSink,
                              nsICachingChannel,
                              nsIUploadChannel,
                              nsICacheListener)

//-----------------------------------------------------------------------------
// nsHttpChannel::nsIRequest
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::GetName(PRUnichar **aName)
{
    NS_ENSURE_ARG_POINTER(aName);
    *aName = ToNewUnicode(mSpec);
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::IsPending(PRBool *value)
{
    NS_ENSURE_ARG_POINTER(value);
    *value = mIsPending;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetStatus(nsresult *aStatus)
{
    NS_ENSURE_ARG_POINTER(aStatus);
    *aStatus = mStatus;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::Cancel(nsresult status)
{
    LOG(("nsHttpChannel::Cancel [this=%x status=%x]\n", this, status));
    mStatus = status;
    if (mTransaction)
        mTransaction->Cancel(status);
    else if (mCacheReadRequest)
        mCacheReadRequest->Cancel(status);
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::Suspend()
{
    LOG(("nsHttpChannel::Suspend [this=%x]\n", this));
    if (mTransaction)
        mTransaction->Suspend();
    else if (mCacheReadRequest)
        mCacheReadRequest->Suspend();
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::Resume()
{
    LOG(("nsHttpChannel::Resume [this=%x]\n", this));
    if (mTransaction)
        mTransaction->Resume();
    else if (mCacheReadRequest)
        mCacheReadRequest->Resume();
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetLoadGroup(nsILoadGroup **aLoadGroup)
{
    NS_ENSURE_ARG_POINTER(aLoadGroup);
    *aLoadGroup = mLoadGroup;
    NS_IF_ADDREF(*aLoadGroup);
    return NS_OK;
}
NS_IMETHODIMP
nsHttpChannel::SetLoadGroup(nsILoadGroup *aLoadGroup)
{
    mLoadGroup = aLoadGroup;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetLoadFlags(nsLoadFlags *aLoadFlags)
{
    NS_ENSURE_ARG_POINTER(aLoadFlags);
    *aLoadFlags = mLoadFlags;
    return NS_OK;
}
NS_IMETHODIMP
nsHttpChannel::SetLoadFlags(nsLoadFlags aLoadFlags)
{
    mLoadFlags = aLoadFlags;

    // don't let anyone overwrite this bit if we're using a secure channel.
    if (mConnectionInfo && mConnectionInfo->UsingSSL())
        mLoadFlags |= INHIBIT_PERSISTENT_CACHING;

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsIChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::GetOriginalURI(nsIURI **originalURI)
{
    NS_ENSURE_ARG_POINTER(originalURI);
    *originalURI = mOriginalURI;
    NS_IF_ADDREF(*originalURI);
    return NS_OK;
}
NS_IMETHODIMP
nsHttpChannel::SetOriginalURI(nsIURI *originalURI)
{
    mOriginalURI = originalURI;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetURI(nsIURI **URI)
{
    NS_ENSURE_ARG_POINTER(URI);
    *URI = mURI;
    NS_IF_ADDREF(*URI);
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetOwner(nsISupports **owner)
{
    NS_ENSURE_ARG_POINTER(owner);
    *owner = mOwner;
    NS_IF_ADDREF(*owner);
    return NS_OK;
}
NS_IMETHODIMP
nsHttpChannel::SetOwner(nsISupports *owner)
{
    mOwner = owner;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetNotificationCallbacks(nsIInterfaceRequestor **callbacks)
{
    NS_ENSURE_ARG_POINTER(callbacks);
    *callbacks = mCallbacks;
    NS_IF_ADDREF(*callbacks);
    return NS_OK;
}
NS_IMETHODIMP
nsHttpChannel::SetNotificationCallbacks(nsIInterfaceRequestor *callbacks)
{
    mCallbacks = callbacks;

    mHttpEventSink = do_GetInterface(mCallbacks);
    mProgressSink = do_GetInterface(mCallbacks);
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetSecurityInfo(nsISupports **securityInfo)
{
    NS_ENSURE_ARG_POINTER(securityInfo);
    *securityInfo = mSecurityInfo;
    NS_IF_ADDREF(*securityInfo);
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetContentType(char **value)
{
    nsresult rv;

    NS_ENSURE_ARG_POINTER(value);
    *value = nsnull;

    if (mResponseHead && mResponseHead->ContentType())
        return DupString(mResponseHead->ContentType(), value);

    // else if the there isn't a response yet or if the response does not
    // contain a content-type header, try to determine the content type
    // from the file extension of the URI...

    // We had to do this same hack in 4.x. Sometimes, we run an http url that
    // ends in special extensions like .dll, .exe, etc and the server doesn't
    // provide a specific content type for the document. In actuality the 
    // document is really text/html (sometimes). For these cases, we don't want
    // to ask the mime service for the content type because it will make 
    // incorrect conclusions based on the file extension. Instead, set the 
    // content type to unknown and allow our unknown content type decoder a
    // chance to sniff the data stream and conclude a content type. 

    PRBool doMimeLookup = PR_TRUE;
    nsCOMPtr<nsIURL> url = do_QueryInterface(mURI);
    if (url) {
        nsXPIDLCString ext;
        url->GetFileExtension(getter_Copies(ext));
        if (ext && (!PL_strcasecmp(ext, "dll") || !PL_strcasecmp(ext, "exe")))
            doMimeLookup = PR_FALSE;
    }
    if (doMimeLookup) {
        nsCOMPtr<nsIMIMEService> mime;
        nsHttpHandler::get()->GetMimeService(getter_AddRefs(mime));
        if (mime) {
            rv = mime->GetTypeFromURI(mURI, value);
            if (NS_SUCCEEDED(rv)) {
                // cache this result if possible
                if (mResponseHead)
                    mResponseHead->SetContentType(*value);
                return rv;
            }
        }
    }

    // the content-type can only be set to application/x-unknown-content-type 
    // if there is data from which to infer a content-type.
    if (!mResponseHead)
        return NS_ERROR_NOT_AVAILABLE;

    *value = PL_strdup(UNKNOWN_CONTENT_TYPE);
    return *value ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}
NS_IMETHODIMP
nsHttpChannel::SetContentType(const char *value)
{
    if (!mResponseHead)
        return NS_ERROR_NOT_AVAILABLE;

    mResponseHead->SetContentType(value);
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetContentLength(PRInt32 *value)
{
    NS_ENSURE_ARG_POINTER(value);

    if (!mResponseHead)
        return NS_ERROR_NOT_AVAILABLE;

    *value = mResponseHead->ContentLength();
    return NS_OK;
}
NS_IMETHODIMP
nsHttpChannel::SetContentLength(PRInt32 value)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHttpChannel::Open(nsIInputStream **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHttpChannel::AsyncOpen(nsIStreamListener *listener, nsISupports *context)
{
    LOG(("nsHttpChannel::AsyncOpen [this=%x]\n", this));

    NS_ENSURE_ARG_POINTER(listener);
    NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);

    PRInt32 port;
    nsresult rv = mURI->GetPort(&port);
    if (NS_FAILED(rv))
        return rv;
 
    nsCOMPtr<nsIIOService> ioService;
    rv = nsHttpHandler::get()->GetIOService(getter_AddRefs(ioService));
    if (NS_FAILED(rv)) return rv;

    rv = NS_CheckPortSafety(port, "http", ioService); // FIX - other schemes?
    if (NS_FAILED(rv))
        return rv;
    
    mIsPending = PR_TRUE;

    mListener = listener;
    mListenerContext = context;

    // add ourselves to the load group.  from this point forward, we'll report
    // all failures asynchronously.
    if (mLoadGroup)
        mLoadGroup->AddRequest(this, nsnull);

    rv = Connect();
    if (NS_FAILED(rv)) {
        LOG(("Connect failed [rv=%x]\n", rv));
        CloseCacheEntry(rv);
        AsyncAbort(rv);
    }
    return NS_OK;
}
//-----------------------------------------------------------------------------
// nsHttpChannel::nsIHttpChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::GetRequestMethod(char **method)
{
    return DupString(mRequestHead.Method().get(), method);
}
NS_IMETHODIMP
nsHttpChannel::SetRequestMethod(const char *method)
{
    NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);

    nsHttpAtom atom = nsHttp::ResolveAtom(method);
    if (!atom)
        return NS_ERROR_FAILURE;

    mRequestHead.SetMethod(atom);
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetReferrer(nsIURI **referrer)
{
    NS_ENSURE_ARG_POINTER(referrer);
    *referrer = mReferrer;
    NS_IF_ADDREF(*referrer);
    return NS_OK;
}

#define numInvalidReferrerSchemes 8

static char * invalidReferrerSchemes [numInvalidReferrerSchemes] = 
{
  "chrome",
  "resource",
  "file",
  "mailbox",
  "imap",
  "news",
  "snews",
  "imaps"
};

NS_IMETHODIMP
nsHttpChannel::SetReferrer(nsIURI *referrer, PRUint32 referrerType)
{
    NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);

    if (nsHttpHandler::get()->ReferrerLevel() < referrerType)
        return NS_OK;

    // don't remember this referrer if it's on our black list....
    if (referrer)
    {
      PRBool invalidScheme = PR_FALSE;
      for (PRUint32 i = 0; i < numInvalidReferrerSchemes && !invalidScheme; i++)
         referrer->SchemeIs(invalidReferrerSchemes[i], &invalidScheme);

      if (invalidScheme) return NS_OK; // kick out....
    }


    // Handle secure referrals.
    // Support referrals from a secure server if this is a secure site
    // and the host names are the same.
    if (referrer) {
        PRBool isHTTPS = PR_FALSE;
        referrer->SchemeIs("https", &isHTTPS);
        if (isHTTPS) {
            nsXPIDLCString referrerHost;
            nsXPIDLCString host;
            referrer->GetHost(getter_Copies(referrerHost));
            mURI->GetHost(getter_Copies(host));
            mURI->SchemeIs("https",&isHTTPS);

            if (nsCRT::strcasecmp(referrerHost, host) != 0) {
                return NS_OK;
            }

            if (!isHTTPS) {
                return NS_OK;
            }
        }
    }

    // save a copy of the referrer so we can return it if requested
    mReferrer = referrer;

    // save a copy of the referrer type for redirects
    mReferrerType = (PRUint8) referrerType;

    // clear the old referer first
    mRequestHead.SetHeader(nsHttp::Referer, nsnull);

    if (referrer) {
        nsXPIDLCString spec;
        referrer->GetSpec(getter_Copies(spec));
        if (spec) {
            nsCAutoString ref(spec.get());
            // strip away any prehost; we don't want to be giving out passwords ;-)
            nsXPIDLCString prehost;
            referrer->GetPreHost(getter_Copies(prehost));
            if (prehost && *prehost) {
                PRUint32 prehostLoc = PRUint32(ref.Find(prehost, PR_TRUE));
                ref.Cut(prehostLoc, nsCharTraits<char>::length(prehost) + 1); // + 1 for @
            }
            mRequestHead.SetHeader(nsHttp::Referer, ref.get());
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetRequestHeader(const char *header, char **value)
{
    nsHttpAtom atom = nsHttp::ResolveAtom(header);
    if (!atom)
        return NS_ERROR_NOT_AVAILABLE;

    return mRequestHead.GetHeader(atom, value);
}

NS_IMETHODIMP
nsHttpChannel::SetRequestHeader(const char *header, const char *value)
{
    NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);

    LOG(("nsHttpChannel::SetRequestHeader [this=%x header=%s value=%s]\n",
        this, header, value));

    nsHttpAtom atom = nsHttp::ResolveAtom(header);
    if (!atom) {
        NS_WARNING("failed to resolve atom");
        return NS_ERROR_NOT_AVAILABLE;
    }

    return mRequestHead.SetHeader(atom, value);
}

NS_IMETHODIMP
nsHttpChannel::VisitRequestHeaders(nsIHttpHeaderVisitor *visitor)
{
    return mRequestHead.Headers().VisitHeaders(visitor);
}

NS_IMETHODIMP
nsHttpChannel::GetUploadStream(nsIInputStream **stream)
{
    NS_ENSURE_ARG_POINTER(stream);
    *stream = mUploadStream;
    NS_IF_ADDREF(*stream);
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::SetUploadStream(nsIInputStream *stream, const char* contentType, PRInt32 contentLength)
{
    mUploadStream = stream;
    if (mUploadStream)
        mRequestHead.SetMethod(nsHttp::Post);
    else
        mRequestHead.SetMethod(nsHttp::Get);
    return NS_OK;
}


NS_IMETHODIMP
nsHttpChannel::SetUploadFile(nsIFile *file, const char* contentType, PRInt32 contentLength)
{
    if (!file) return NS_ERROR_NULL_POINTER;

    nsresult rv;
    // Grab a file input stream
    nsCOMPtr<nsIInputStream> stream;
    rv = NS_NewLocalFileInputStream(getter_AddRefs(stream), file);    
    if (NS_FAILED(rv))
        return rv;
    
    // get the content type
    if (contentType)
        return SetUploadStream(stream, contentType, contentLength); 

    nsCOMPtr<nsIMIMEService> MIMEService (do_GetService(NS_MIMESERVICE_CONTRACTID, &rv));
    if (NS_FAILED(rv)) return rv;
    nsXPIDLCString mimeType;
    rv = MIMEService->GetTypeFromFile(file, getter_Copies(mimeType));
    if (NS_FAILED(rv)) return rv;

    // set the stream on ourselves
    return SetUploadStream(stream, mimeType, contentLength); 
}

NS_IMETHODIMP
nsHttpChannel::GetResponseStatus(PRUint32 *value)
{
    NS_ENSURE_ARG_POINTER(value);
    if (!mResponseHead)
        return NS_ERROR_NOT_AVAILABLE;
    *value = mResponseHead->Status();
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetResponseStatusText(char **value)
{
    if (!mResponseHead)
        return NS_ERROR_NOT_AVAILABLE;
    return DupString(mResponseHead->StatusText(), value);
}

NS_IMETHODIMP
nsHttpChannel::GetResponseHeader(const char *header, char **value)
{
    if (!mResponseHead)
        return NS_ERROR_NOT_AVAILABLE;
    nsHttpAtom atom = nsHttp::ResolveAtom(header);
    if (!atom)
        return NS_ERROR_NOT_AVAILABLE;
    return mResponseHead->GetHeader(atom, value);
}

NS_IMETHODIMP
nsHttpChannel::SetResponseHeader(const char *header, const char *value)
{
    if (!mResponseHead)
        return NS_ERROR_NOT_AVAILABLE;
    nsHttpAtom atom = nsHttp::ResolveAtom(header);
    if (!atom)
        return NS_ERROR_NOT_AVAILABLE;

    // these response headers must not be changed 
    if (atom == nsHttp::Content_Type ||
        atom == nsHttp::Content_Length ||
        atom == nsHttp::Content_Encoding ||
        atom == nsHttp::Trailer ||
        atom == nsHttp::Transfer_Encoding)
        return NS_ERROR_ILLEGAL_VALUE;

    nsresult rv = mResponseHead->SetHeader(atom, value);

    // XXX temporary hack until http supports some form of a header change observer
    if ((atom == nsHttp::Set_Cookie) && NS_SUCCEEDED(rv))
        rv = nsHttpHandler::get()->OnExamineResponse(this);

    mResponseHeadersModified = PR_TRUE;
    return rv;
}

NS_IMETHODIMP
nsHttpChannel::VisitResponseHeaders(nsIHttpHeaderVisitor *visitor)
{
    if (!mResponseHead)
        return NS_ERROR_NOT_AVAILABLE;
    return mResponseHead->Headers().VisitHeaders(visitor);
}

NS_IMETHODIMP
nsHttpChannel::GetCharset(char **value)
{
    if (!mResponseHead)
        return NS_ERROR_NOT_AVAILABLE;
    return DupString(mResponseHead->ContentCharset(), value);
}

NS_IMETHODIMP
nsHttpChannel::GetApplyConversion(PRBool *value)
{
    NS_ENSURE_ARG_POINTER(value);
    *value = mApplyConversion;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::SetApplyConversion(PRBool value)
{
    LOG(("nsHttpChannel::SetApplyConversion [this=%x value=%d]\n", this, value));
    mApplyConversion = value;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsIRequestObserver
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::OnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
    // capture the request's status, so our consumers will know ASAP of any
    // connection failures, etc - bug 93581
    request->GetStatus(&mStatus);

    LOG(("nsHttpChannel::OnStartRequest [this=%x request=%x status=%x]\n",
        this, request, mStatus));

    if (mTransaction) {
        // grab the security info from the connection object; the transaction
        // is guaranteed to own a reference to the connection.
        mSecurityInfo = mTransaction->SecurityInfo();

        // all of the response headers have been acquired, so we can take ownership
        // of them from the transaction.
        mResponseHead = mTransaction->TakeResponseHead();
        // the response head may be null if the transaction was cancelled.  in
        // which case we just need to call OnStartRequest/OnStopRequest.
        if (mResponseHead)
            return ProcessResponse();
    }

    // there won't be a response head if we've been cancelled
    nsresult rv = mListener->OnStartRequest(this, mListenerContext);
    if (NS_FAILED(rv)) return rv;

    // install stream converter if required
    ApplyContentConversions();
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::OnStopRequest(nsIRequest *request, nsISupports *ctxt, nsresult status)
{
    LOG(("nsHttpChannel::OnStopRequest [this=%x request=%x status=%x]\n",
        this, request, status));

    // if the request is a previous transaction, then simply release it.
    if (request == mPrevTransaction) {
        NS_RELEASE(mPrevTransaction);
        mPrevTransaction = nsnull;
    }

    // if the request is for something we no longer reference, then simply 
    // drop this event.
    if ((request != mTransaction) && (request != mCacheReadRequest))
        return NS_OK;

    mIsPending = PR_FALSE;
    mStatus = status;

    // at this point, we're done with the transaction
    if (mTransaction) {
        NS_RELEASE(mTransaction);
        mTransaction = nsnull;
    }

    // perform any final cache operations before we close the cache entry.
    if (mCacheEntry && (mCacheAccess & nsICache::ACCESS_WRITE))
        FinalizeCacheEntry();
    
    // we don't support overlapped i/o (bug 82418)
#if 0
    if (mCacheEntry && NS_SUCCEEDED(status))
        mCacheEntry->MarkValid();
#endif

    if (mListener) {
        mListener->OnStopRequest(this, mListenerContext, status);
        mListener = 0;
        mListenerContext = 0;
    }

    if (mCacheEntry)
        CloseCacheEntry(status);

    if (mLoadGroup)
        mLoadGroup->RemoveRequest(this, nsnull, status);

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsIStreamListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::OnDataAvailable(nsIRequest *request, nsISupports *ctxt,
                               nsIInputStream *input,
                               PRUint32 offset, PRUint32 count)
{
    LOG(("nsHttpChannel::OnDataAvailable [this=%x request=%x offset=%u count=%u]\n",
        this, request, offset, count));

    // if the request is for something we no longer reference, then simply 
    // drop this event.
    if ((request != mTransaction) && (request != mCacheReadRequest)) {
        NS_WARNING("got stale request... why wasn't it cancelled?");
        return NS_BASE_STREAM_CLOSED;
    }

    if (mListener)
        return mListener->OnDataAvailable(this, mListenerContext, input, offset, count);

    return NS_BASE_STREAM_CLOSED;
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsIInterfaceRequestor
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::GetInterface(const nsIID &iid, void **result)
{
    if (iid.Equals(NS_GET_IID(nsIProgressEventSink))) {
        //
        // we return ourselves as the progress event sink so we can intercept
        // notifications and set the correct request and context parameters.
        // but, if we don't have a progress sink to forward those messages
        // to, then there's no point in handing out a reference to ourselves.
        //
        if (!mProgressSink)
            return NS_ERROR_NO_INTERFACE;

        return QueryInterface(iid, result);
    }

    if (mCallbacks)
        return mCallbacks->GetInterface(iid, result);

    return NS_ERROR_NO_INTERFACE;
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsIProgressEventSink
//-----------------------------------------------------------------------------

// called on the socket thread
NS_IMETHODIMP
nsHttpChannel::OnStatus(nsIRequest *req, nsISupports *ctx, nsresult status,
                        const PRUnichar *statusText)
{
    if (mProgressSink)
        mProgressSink->OnStatus(this, mListenerContext, status, statusText);

    return NS_OK;
}

// called on the socket thread
NS_IMETHODIMP
nsHttpChannel::OnProgress(nsIRequest *req, nsISupports *ctx,
                          PRUint32 progress, PRUint32 progressMax)
{
    if (mProgressSink)
        mProgressSink->OnProgress(this, mListenerContext, progress, progressMax);

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsICachingChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::GetCacheToken(nsISupports **token)
{
    NS_ENSURE_ARG_POINTER(token);
    if (!mCacheEntry)
        return NS_ERROR_NOT_AVAILABLE;
    return CallQueryInterface(mCacheEntry, token);
}

NS_IMETHODIMP
nsHttpChannel::SetCacheToken(nsISupports *token)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHttpChannel::GetCacheKey(nsISupports **key)
{
    nsresult rv;
    NS_ENSURE_ARG_POINTER(key);

    LOG(("nsHttpChannel::GetCacheKey [this=%x]\n", this));

    *key = nsnull;

    nsCOMPtr<nsISupportsPRUint32> container =
        do_CreateInstance(NS_SUPPORTS_PRUINT32_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = container->SetData(mPostID);
    if (NS_FAILED(rv)) return rv;

    return CallQueryInterface(container, key);
}

NS_IMETHODIMP
nsHttpChannel::SetCacheKey(nsISupports *key, PRBool fromCacheOnly)
{
    nsresult rv;

    LOG(("nsHttpChannel::SetCacheKey [this=%x key=%x fromCacheOnly=%d]\n",
        this, key, fromCacheOnly));

    // can only set the cache key if a load is not in progress
    NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);

    if (!key)
        mPostID = 0;
    else {
        // extract the post id
        nsCOMPtr<nsISupportsPRUint32> container = do_QueryInterface(key, &rv);
        if (NS_FAILED(rv)) return rv;

        rv = container->GetData(&mPostID);
        if (NS_FAILED(rv)) return rv;
    }

    mFromCacheOnly = fromCacheOnly;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetCacheAsFile(PRBool *value)
{
    NS_ENSURE_ARG_POINTER(value);
    if (!mCacheEntry)
        return NS_ERROR_NOT_AVAILABLE;
    nsCacheStoragePolicy storagePolicy;
    mCacheEntry->GetStoragePolicy(&storagePolicy);
    *value = (storagePolicy == nsICache::STORE_ON_DISK_AS_FILE);
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::SetCacheAsFile(PRBool value)
{
    if (!mCacheEntry || mLoadFlags & INHIBIT_PERSISTENT_CACHING)
        return NS_ERROR_NOT_AVAILABLE;
    nsCacheStoragePolicy policy;
    if (value)
        policy = nsICache::STORE_ON_DISK_AS_FILE;
    else
        policy = nsICache::STORE_ANYWHERE;
    return mCacheEntry->SetStoragePolicy(policy);
}

NS_IMETHODIMP
nsHttpChannel::GetCacheFile(nsIFile **cacheFile)
{
    if (!mCacheEntry)
        return NS_ERROR_NOT_AVAILABLE;
    return mCacheEntry->GetFile(cacheFile);
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsICacheListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::OnCacheEntryAvailable(nsICacheEntryDescriptor *entry,
                                     nsCacheAccessMode access,
                                     nsresult status)
{
    LOG(("nsHttpChannel::OnCacheEntryAvailable [this=%x entry=%x "
         "access=%x status=%x]\n", this, entry, access, status));

    // if the channel's already fired onStopRequest, then we should ignore
    // this event.
    if (!mIsPending)
        return NS_OK;

    // otherwise, we have to handle this event.
    if (NS_SUCCEEDED(status)) {
        mCacheEntry = entry;
        mCacheAccess = access;
    }

    nsresult rv;

    if (NS_FAILED(mStatus)) {
        LOG(("channel was canceled [this=%x status=%x]\n", this, mStatus));
        rv = mStatus;
    }
    else // advance to the next state...
        rv = Connect(PR_FALSE);

    // a failure from Connect means that we have to abort the channel.
    if (NS_FAILED(rv)) {
        CloseCacheEntry(rv);
        AsyncAbort(rv);
    }

    return NS_OK;
}
