/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set expandtab ts=4 sw=4 sts=4: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net> (original author)
 *   Christian Biesinger <cbiesinger@web.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIURL.h"
#include "nsIScriptSecurityManager.h"
#include "nsIIDNService.h"
#include "nsIStreamListenerTee.h"
#include "nsISeekableStream.h"
#include "nsCPasswordManager.h"
#include "nsMimeTypes.h"
#include "nsNetUtil.h"
#include "nsString.h"
#include "nsPrintfCString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsAutoPtr.h"
#include "plstr.h"
#include "prprf.h"
#include "nsEscape.h"
#include "nsICookieService.h"
#include "nsIResumableChannel.h"
#include "nsInt64.h"

static NS_DEFINE_CID(kStreamListenerTeeCID, NS_STREAMLISTENERTEE_CID);

static NS_METHOD DiscardSegments(nsIInputStream *input,
                                 void *closure,
                                 const char *buf,
                                 PRUint32 offset,
                                 PRUint32 count,
                                 PRUint32 *countRead)
{
    *countRead = count;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpChannel <public>
//-----------------------------------------------------------------------------

nsHttpChannel::nsHttpChannel()
    : mResponseHead(nsnull)
    , mTransaction(nsnull)
    , mConnectionInfo(nsnull)
    , mLoadFlags(LOAD_NORMAL)
    , mStatus(NS_OK)
    , mLogicalOffset(0)
    , mCaps(0)
    , mCachedResponseHead(nsnull)
    , mCacheAccess(0)
    , mPostID(0)
    , mRequestTime(0)
    , mAuthContinuationState(nsnull)
    , mStartPos(LL_MaxUint())
    , mRedirectionLimit(gHttpHandler->RedirectionLimit())
    , mIsPending(PR_FALSE)
    , mApplyConversion(PR_TRUE)
    , mAllowPipelining(PR_TRUE)
    , mCachedContentIsValid(PR_FALSE)
    , mCachedContentIsPartial(PR_FALSE)
    , mResponseHeadersModified(PR_FALSE)
    , mCanceled(PR_FALSE)
    , mTransactionReplaced(PR_FALSE)
    , mUploadStreamHasHeaders(PR_FALSE)
    , mAuthRetryPending(PR_FALSE)
    , mSuppressDefensiveAuth(PR_FALSE)
    , mResuming(PR_FALSE)
{
    LOG(("Creating nsHttpChannel @%x\n", this));

    // grab a reference to the handler to ensure that it doesn't go away.
    nsHttpHandler *handler = gHttpHandler;
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

    NS_IF_RELEASE(mAuthContinuationState);

    // release our reference to the handler
    nsHttpHandler *handler = gHttpHandler;
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
    mDocumentURI = nsnull;
    mCaps = caps;

    //
    // Construct connection info object
    //
    nsCAutoString host;
    PRInt32 port = -1;
    PRBool usingSSL = PR_FALSE;
    
    rv = mURI->SchemeIs("https", &usingSSL);
    if (NS_FAILED(rv)) return rv;

    rv = mURI->GetAsciiHost(host);
    if (NS_FAILED(rv)) return rv;

    // reject the URL if it doesn't specify a host
    if (host.IsEmpty())
        return NS_ERROR_MALFORMED_URI;

    rv = mURI->GetPort(&port);
    if (NS_FAILED(rv)) return rv;

    LOG(("host=%s port=%d\n", host.get(), port));

    rv = mURI->GetAsciiSpec(mSpec);
    if (NS_FAILED(rv)) return rv;

    LOG(("uri=%s\n", mSpec.get()));

    mConnectionInfo = new nsHttpConnectionInfo(host, port,
                                               proxyInfo, usingSSL);
    if (!mConnectionInfo)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(mConnectionInfo);

    // make sure our load flags include this bit if this is a secure channel.
    if (usingSSL && !gHttpHandler->IsPersistentHttpsCachingEnabled()) 
        mLoadFlags |= INHIBIT_PERSISTENT_CACHING;

    // Set default request method
    mRequestHead.SetMethod(nsHttp::Get);

    //
    // Set request headers
    //
    nsCAutoString hostLine;
    if (strchr(host.get(), ':')) {
        // host is an IPv6 address literal and must be encapsulated in []'s
        hostLine.Assign('[');
        hostLine.Append(host);
        hostLine.Append(']');
    }
    else
        hostLine.Assign(host);
    if (port != -1) {
        hostLine.Append(':');
        hostLine.AppendInt(port);
    }

    rv = mRequestHead.SetHeader(nsHttp::Host, hostLine);
    if (NS_FAILED(rv)) return rv;

    rv = gHttpHandler->
        AddStandardRequestHeaders(&mRequestHead.Headers(), caps,
                                  !mConnectionInfo->UsingSSL() &&
                                  mConnectionInfo->UsingHttpProxy());

    return rv;
}

//-----------------------------------------------------------------------------
// nsHttpChannel <private>
//-----------------------------------------------------------------------------

nsresult
nsHttpChannel::AsyncCall(nsAsyncCallback funcPtr)
{
    nsresult rv;

    nsAsyncCallEvent *event = new nsAsyncCallEvent;
    if (!event)
        return NS_ERROR_OUT_OF_MEMORY;

    event->mFuncPtr = funcPtr;

    NS_ADDREF_THIS();

    PL_InitEvent(event, this,
                 nsHttpChannel::AsyncCall_EventHandlerFunc,
                 nsHttpChannel::AsyncCall_EventCleanupFunc);

    rv = mEventQ->PostEvent(event);
    if (NS_FAILED(rv)) {
        PL_DestroyEvent(event);
        NS_RELEASE_THIS();
    }
    return rv;
}

void *PR_CALLBACK
nsHttpChannel::AsyncCall_EventHandlerFunc(PLEvent *ev)
{
    nsHttpChannel *chan =
        NS_STATIC_CAST(nsHttpChannel *, PL_GetEventOwner(ev));

    nsAsyncCallEvent *ace = (nsAsyncCallEvent *) ev;
    nsAsyncCallback funcPtr = ace->mFuncPtr;

    if (chan) {
        (chan->*funcPtr)();
        NS_RELEASE(chan);
    }
    return nsnull;
}

void PR_CALLBACK
nsHttpChannel::AsyncCall_EventCleanupFunc(PLEvent *ev)
{
    delete (nsAsyncCallEvent *) ev;
}

nsresult
nsHttpChannel::Connect(PRBool firstTime)
{
    nsresult rv;

    LOG(("nsHttpChannel::Connect [this=%x]\n", this));

    // true when called from AsyncOpen
    if (firstTime) {
        PRBool delayed = PR_FALSE;
        PRBool offline = PR_FALSE;
    
        // are we offline?
        nsCOMPtr<nsIIOService> ioService;
        rv = gHttpHandler->GetIOService(getter_AddRefs(ioService));
        if (NS_FAILED(rv)) return rv;

        ioService->GetOffline(&offline);
        if (offline)
            mLoadFlags |= LOAD_ONLY_FROM_CACHE;

        // Don't allow resuming when cache must be used
        if (mResuming && (mLoadFlags & LOAD_ONLY_FROM_CACHE)) {
            LOG(("Resuming from cache is not supported yet"));
            return NS_ERROR_DOCUMENT_NOT_CACHED;
        }

        // open a cache entry for this channel...
        rv = OpenCacheEntry(offline, &delayed);

        if (NS_FAILED(rv)) {
            LOG(("OpenCacheEntry failed [rv=%x]\n", rv));
            // if this channel is only allowed to pull from the cache, then
            // we must fail if we were unable to open a cache entry.
            if (mLoadFlags & LOAD_ONLY_FROM_CACHE)
                return NS_ERROR_DOCUMENT_NOT_CACHED;
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
        else if (mLoadFlags & LOAD_ONLY_FROM_CACHE) {
            // the cache contains the requested resource, but it must be 
            // validated before we can reuse it.  since we are not allowed
            // to hit the net, there's nothing more to do.  the document
            // is effectively not in the cache.
            return NS_ERROR_DOCUMENT_NOT_CACHED;
        }
    }

    // check to see if authorization headers should be included
    AddAuthorizationHeaders();

    // hit the net...
    rv = SetupTransaction();
    if (NS_FAILED(rv)) return rv;

    rv = gHttpHandler->InitiateTransaction(mTransaction);
    if (NS_FAILED(rv)) return rv;

    return mTransactionPump->AsyncRead(this, nsnull);
}

// called when Connect fails
nsresult
nsHttpChannel::AsyncAbort(nsresult status)
{
    LOG(("nsHttpChannel::AsyncAbort [this=%x status=%x]\n", this, status));

    mStatus = status;
    mIsPending = PR_FALSE;

    // create a proxy for the listener..
    nsCOMPtr<nsIRequestObserver> observer;
    NS_NewRequestObserverProxy(getter_AddRefs(observer), mListener, mEventQ);
    if (observer) {
        observer->OnStartRequest(this, mListenerContext);
        observer->OnStopRequest(this, mListenerContext, mStatus);
    }
    else {
        NS_ERROR("unable to create request observer proxy");
        // XXX else, no proxy object manager... what do we do?
    }
    mListener = 0;
    mListenerContext = 0;

    // finally remove ourselves from the load group.
    if (mLoadGroup)
        mLoadGroup->RemoveRequest(this, nsnull, status);

    return NS_OK;
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
        if (NS_FAILED(rv)) {
            // If ProcessRedirection fails, then we have to send out the
            // OnStart/OnStop notifications.
            LOG(("ProcessRedirection failed [rv=%x]\n", rv));
            mStatus = rv;
            if (mListener) {
                mListener->OnStartRequest(this, mListenerContext);
                mListener->OnStopRequest(this, mListenerContext, mStatus);
                mListener = 0;
                mListenerContext = 0;
            }
        }
    }

    // close the cache entry... blow it away if we couldn't process
    // the redirect for some reason.
    CloseCacheEntry(rv);

    mIsPending = PR_FALSE;

    if (mLoadGroup)
        mLoadGroup->RemoveRequest(this, nsnull, mStatus);
}

void
nsHttpChannel::HandleAsyncNotModified()
{
    LOG(("nsHttpChannel::HandleAsyncNotModified [this=%p]\n", this));

    if (mListener) {
        mListener->OnStartRequest(this, mListenerContext);
        mListener->OnStopRequest(this, mListenerContext, mStatus);
        mListener = 0;
        mListenerContext = 0;
    }

    CloseCacheEntry(NS_OK);

    mIsPending = PR_FALSE;

    if (mLoadGroup)
        mLoadGroup->RemoveRequest(this, nsnull, mStatus);
}

nsresult
nsHttpChannel::SetupTransaction()
{
    LOG(("nsHttpChannel::SetupTransaction [this=%x]\n", this));

    NS_ENSURE_TRUE(!mTransaction, NS_ERROR_ALREADY_INITIALIZED);

    nsresult rv;

    if (mCaps & NS_HTTP_ALLOW_PIPELINING) {
        //
        // disable pipelining if:
        //   (1) pipelining has been explicitly disabled
        //   (2) request corresponds to a top-level document load (link click)
        //   (3) request method is non-idempotent
        //
        // XXX does the toplevel document check really belong here?  or, should
        //     we push it out entirely to necko consumers?
        //
        if (!mAllowPipelining || (mLoadFlags & LOAD_INITIAL_DOCUMENT_URI) ||
            !(mRequestHead.Method() == nsHttp::Get ||
              mRequestHead.Method() == nsHttp::Head)) {
            LOG(("  pipelining disallowed\n"));
            mCaps &= ~NS_HTTP_ALLOW_PIPELINING;
        }
    }

    // use the URI path if not proxying (transparent proxying such as SSL proxy
    // does not count here). also, figure out what version we should be speaking.
    nsCAutoString buf, path;
    nsCString* requestURI;
    if (mConnectionInfo->UsingSSL() || !mConnectionInfo->UsingHttpProxy()) {
        rv = mURI->GetPath(path);
        if (NS_FAILED(rv)) return rv;
        // path may contain UTF-8 characters, so ensure that they're escaped.
        if (NS_EscapeURL(path.get(), path.Length(), esc_OnlyNonASCII, buf))
            requestURI = &buf;
        else
            requestURI = &path;
        mRequestHead.SetVersion(gHttpHandler->HttpVersion());
    }
    else {
        rv = mURI->GetUserPass(buf);
        if (NS_FAILED(rv)) return rv;
        if (!buf.IsEmpty() && ((strncmp(mSpec.get(), "http:", 5) == 0) ||
                                strncmp(mSpec.get(), "https:", 6) == 0)) {
            nsCOMPtr<nsIURI> tempURI;
            rv = mURI->Clone(getter_AddRefs(tempURI));
            if (NS_FAILED(rv)) return rv;
            rv = tempURI->SetUserPass(EmptyCString());
            if (NS_FAILED(rv)) return rv;
            rv = tempURI->GetAsciiSpec(path);
            if (NS_FAILED(rv)) return rv;
            requestURI = &path;
        }
        else
            requestURI = &mSpec;
        mRequestHead.SetVersion(gHttpHandler->ProxyHttpVersion());
    }

    // trim off the #ref portion if any...
    PRInt32 ref = requestURI->FindChar('#');
    if (ref != kNotFound)
        requestURI->SetLength(ref);

    mRequestHead.SetRequestURI(*requestURI);

    // set the request time for cache expiration calculations
    mRequestTime = NowInSeconds();

    // if doing a reload, force end-to-end
    if (mLoadFlags & LOAD_BYPASS_CACHE) {
        // We need to send 'Pragma:no-cache' to inhibit proxy caching even if
        // no proxy is configured since we might be talking with a transparent
        // proxy, i.e. one that operates at the network level.  See bug #14772.
        mRequestHead.SetHeader(nsHttp::Pragma, NS_LITERAL_CSTRING("no-cache"), PR_TRUE);
        // If we're configured to speak HTTP/1.1 then also send 'Cache-control:
        // no-cache'
        if (mRequestHead.Version() >= NS_HTTP_VERSION_1_1)
            mRequestHead.SetHeader(nsHttp::Cache_Control, NS_LITERAL_CSTRING("no-cache"), PR_TRUE);
    }
    else if ((mLoadFlags & VALIDATE_ALWAYS) && (mCacheAccess & nsICache::ACCESS_READ)) {
        // We need to send 'Cache-Control: max-age=0' to force each cache along
        // the path to the origin server to revalidate its own entry, if any,
        // with the next cache or server.  See bug #84847.
        //
        // If we're configured to speak HTTP/1.0 then just send 'Pragma: no-cache'
        if (mRequestHead.Version() >= NS_HTTP_VERSION_1_1)
            mRequestHead.SetHeader(nsHttp::Cache_Control, NS_LITERAL_CSTRING("max-age=0"), PR_TRUE);
        else
            mRequestHead.SetHeader(nsHttp::Pragma, NS_LITERAL_CSTRING("no-cache"), PR_TRUE);
    }

    if (mResuming) {
        char buf[32];
        PR_snprintf(buf, sizeof(buf), "bytes=%llu-", mStartPos);
        mRequestHead.SetHeader(nsHttp::Range, nsDependentCString(buf));

        if (mEntityID) {
            // Also, we want an error if this resource changed in the meantime
            nsCAutoString entityTag;
            rv = mEntityID->GetEntityTag(entityTag);
            if (NS_SUCCEEDED(rv) && !entityTag.IsEmpty()) {
                mRequestHead.SetHeader(nsHttp::If_Match, entityTag);
            }

            nsCAutoString lastMod;
            rv = mEntityID->GetLastModified(lastMod);
            if (NS_SUCCEEDED(rv) && !lastMod.IsEmpty()) {
                mRequestHead.SetHeader(nsHttp::If_Unmodified_Since, lastMod);

            }
        }
    }

    // create the transaction object
    mTransaction = new nsHttpTransaction();
    if (!mTransaction)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(mTransaction);

    nsCOMPtr<nsIAsyncInputStream> responseStream;
    rv = mTransaction->Init(mCaps, mConnectionInfo, &mRequestHead,
                            mUploadStream, mUploadStreamHasHeaders,
                            mEventQ, mCallbacks, this,
                            getter_AddRefs(responseStream));
    if (NS_FAILED(rv)) return rv;

    rv = NS_NewInputStreamPump(getter_AddRefs(mTransactionPump),
                               responseStream);
    return rv;
}

void
nsHttpChannel::AddCookiesToRequest()
{
    nsXPIDLCString cookie;

    nsICookieService *cs = gHttpHandler->GetCookieService();
    if (cs)
        cs->GetCookieStringFromHttp(mURI,
                                    mDocumentURI ? mDocumentURI : mOriginalURI,
                                    this,
                                    getter_Copies(cookie));

    // overwrite any existing cookie headers.  be sure to clear any
    // existing cookies if we have no cookies to set or if the cookie
    // service is unavailable.
    mRequestHead.SetHeader(nsHttp::Cookie, cookie, PR_FALSE);
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
    if (gHttpHandler->IsAcceptableEncoding(val)) {
        nsCOMPtr<nsIStreamConverterService> serv;
        nsresult rv = gHttpHandler->
                GetStreamConverterService(getter_AddRefs(serv));
        // we won't fail to load the page just because we couldn't load the
        // stream converter service.. carry on..
        if (NS_SUCCEEDED(rv)) {
            nsCOMPtr<nsIStreamListener> converter;
            nsCAutoString from(val);
            ToLowerCase(from);
            rv = serv->AsyncConvertData(from.get(),
                                        "uncompressed",
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
nsHttpChannel::CallOnStartRequest()
{
    if (mResponseHead && mResponseHead->ContentType().IsEmpty()) {
        if (!mContentTypeHint.IsEmpty())
            mResponseHead->SetContentType(mContentTypeHint);
        else {
            // Uh-oh.  We had better find out what type we are!

            // XXX This does not work with content-encodings...  but
            // neither does applying the conversion from the URILoader

            nsCOMPtr<nsIStreamConverterService> serv;
            nsresult rv = gHttpHandler->
                GetStreamConverterService(getter_AddRefs(serv));
            // If we failed, we just fall through to the "normal" case
            if (NS_SUCCEEDED(rv)) {
                nsCOMPtr<nsIStreamListener> converter;
                rv = serv->AsyncConvertData(UNKNOWN_CONTENT_TYPE,
                                            "*/*",
                                            mListener,
                                            mListenerContext,
                                            getter_AddRefs(converter));
                if (NS_SUCCEEDED(rv)) {
                    mListener = converter;
                }
            }
        }
    }

    if (mResponseHead && mResponseHead->ContentCharset().IsEmpty())
        mResponseHead->SetContentCharset(mContentCharsetHint);
    
    LOG(("  calling mListener->OnStartRequest\n"));
    nsresult rv = mListener->OnStartRequest(this, mListenerContext);
    if (NS_FAILED(rv)) return rv;

    // install stream converter if required
    ApplyContentConversions();

    return rv;
}

nsresult
nsHttpChannel::ProcessResponse()
{
    nsresult rv;
    PRUint32 httpStatus = mResponseHead->Status();

    LOG(("nsHttpChannel::ProcessResponse [this=%x httpStatus=%u]\n",
        this, httpStatus));

    // set cookies, if any exist
    SetCookie(mResponseHead->PeekHeader(nsHttp::Set_Cookie));

    // notify "http-on-examine-response" observers
    gHttpHandler->OnExamineResponse(this);

    // handle unused username and password in url (see bug 232567)
    if (httpStatus != 401 && httpStatus != 407)
        CheckForSuperfluousAuth();

    // handle different server response categories
    switch (httpStatus) {
    case 200:
    case 203:
        // Per RFC 2616, 14.35.2, "A server MAY ignore the Range header".
        // So if a server does that and sends 200 instead of 206 that we
        // expect, notify our caller.
        if (mResuming) {
            Cancel(NS_ERROR_NOT_RESUMABLE);
            rv = CallOnStartRequest();
            break;
        }
        // these can normally be cached
        rv = ProcessNormal();
        break;
    case 206:
        if (mCachedContentIsPartial) // an internal byte range request...
            rv = ProcessPartialContent();
        else
            rv = ProcessNormal();
        break;
    case 300:
    case 301:
    case 302:
    case 307:
        // these redirects can be cached (don't store the response body)
        rv = ProcessRedirection(httpStatus);
        if (NS_SUCCEEDED(rv))
            CloseCacheEntry(InitCacheEntry());
        else {
            LOG(("ProcessRedirection failed [rv=%x]\n", rv));
            rv = ProcessNormal();
        }
        break;
    case 303:
#if 0
    case 305: // disabled as a security measure (see bug 187996).
#endif
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
            CheckForSuperfluousAuth();
            rv = ProcessNormal();
        }
        break;
    case 412: // Precondition failed
    case 416: // Invalid range
        if (mResuming) {
            Cancel(NS_ERROR_ENTITY_CHANGED);
            rv = CallOnStartRequest();
            break;
        }
        // fall through
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

    // if we're here, then any byte-range requests failed to result in a partial
    // response.  we must clear this flag to prevent BufferPartialContent from
    // being called inside our OnDataAvailable (see bug 136678).
    mCachedContentIsPartial = PR_FALSE;

    // For .gz files, apache sends both a Content-Type: application/x-gzip
    // as well as Content-Encoding: gzip, which is completely wrong.  In
    // this case, we choose to ignore the rogue Content-Encoding header. We
    // must do this early on so as to prevent it from being seen up stream.
    // The same problem exists for Content-Encoding: compress in default
    // Apache installs.
    const char *encoding = mResponseHead->PeekHeader(nsHttp::Content_Encoding);
    if (encoding && PL_strcasestr(encoding, "gzip") && (
        mResponseHead->ContentType().EqualsLiteral(APPLICATION_GZIP) ||
        mResponseHead->ContentType().EqualsLiteral(APPLICATION_GZIP2) ||
        mResponseHead->ContentType().EqualsLiteral(APPLICATION_GZIP3))) {
        // clear the Content-Encoding header
        mResponseHead->ClearHeader(nsHttp::Content_Encoding);
    }
    else if (encoding && PL_strcasestr(encoding, "compress") && (
             mResponseHead->ContentType().EqualsLiteral(APPLICATION_COMPRESS) ||
             mResponseHead->ContentType().EqualsLiteral(APPLICATION_COMPRESS2))) {
        // clear the Content-Encoding header
        mResponseHead->ClearHeader(nsHttp::Content_Encoding);
    }

    // this must be called before firing OnStartRequest, since http clients,
    // such as imagelib, expect our cache entry to already have the correct
    // expiration time (bug 87710).
    if (mCacheEntry) {
        rv = InitCacheEntry();
        if (NS_FAILED(rv)) return rv; // XXX this early return prevents OnStartRequest from firing!
    }

    // Check that the server sent us what we were asking for
    if (mResuming) {
        // Create an entity id from the response
        nsCOMPtr<nsIResumableEntityID> id;
        rv = GetEntityID(getter_AddRefs(id));
        if (NS_FAILED(rv)) {
            // If creating an entity id is not possible -> error
            Cancel(NS_ERROR_NOT_RESUMABLE);
        }
        // If we were passed an entity id, verify it's equal to the server's
        else if (mEntityID) {
            PRBool equal;
            rv = mEntityID->Equals(id, &equal);
            if (NS_FAILED(rv) || !equal)
                Cancel(NS_ERROR_ENTITY_CHANGED);
        }
    }

    rv = CallOnStartRequest();
    if (NS_FAILED(rv)) return rv;

    // install cache listener if we still have a cache entry open
    if (mCacheEntry)
        rv = InstallCacheListener();

    return rv;
}

void
nsHttpChannel::GetCallback(const nsIID &aIID, void **aResult)
{
    NS_ASSERTION(aResult && !*aResult, "invalid argument in GetCallback");

    if (mCallbacks)
        mCallbacks->GetInterface(aIID, aResult);

    if (!*aResult && mLoadGroup) {
        nsCOMPtr<nsIInterfaceRequestor> cbs;
        mLoadGroup->GetNotificationCallbacks(getter_AddRefs(cbs));
        if (cbs)
            cbs->GetInterface(aIID, aResult);
    }
}

nsresult
nsHttpChannel::PromptTempRedirect()
{
    nsresult rv;
    nsCOMPtr<nsIStringBundleService> bundleService =
            do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIStringBundle> stringBundle;
    rv = bundleService->CreateBundle(NECKO_MSGS_URL, getter_AddRefs(stringBundle));
    if (NS_FAILED(rv)) return rv;

    nsXPIDLString messageString;
    rv = stringBundle->GetStringFromName(NS_LITERAL_STRING("RepostFormData").get(), getter_Copies(messageString));
    // GetStringFromName can return NS_OK and NULL messageString.
    if (NS_SUCCEEDED(rv) && messageString) {
        PRBool repost = PR_FALSE;

        nsCOMPtr<nsIPrompt> prompt;
        GetCallback(NS_GET_IID(nsIPrompt), getter_AddRefs(prompt));
        if (!prompt)
            return NS_ERROR_NO_INTERFACE;

        prompt->Confirm(nsnull, messageString, &repost);
        if (!repost)
            return NS_ERROR_FAILURE;
    }

    return rv;
}

nsresult
nsHttpChannel::ProxyFailover()
{
    LOG(("nsHttpChannel::ProxyFailover [this=%x]\n", this));

    nsresult rv;

    nsCOMPtr<nsIProtocolProxyService> pps =
            do_GetService(NS_PROTOCOLPROXYSERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIProxyInfo> pi;
    rv = pps->GetFailoverForProxy(mConnectionInfo->ProxyInfo(), mURI, mStatus,
                                  getter_AddRefs(pi));
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIChannel> newChannel;
    rv = gHttpHandler->NewProxiedChannel(mURI, pi, getter_AddRefs(newChannel));
    if (NS_FAILED(rv))
        return rv;

    rv = SetupReplacementChannel(mURI, newChannel, PR_TRUE);
    if (NS_FAILED(rv))
        return rv;

    // open new channel
    rv = newChannel->AsyncOpen(mListener, mListenerContext);
    if (NS_FAILED(rv))
        return rv;

    mStatus = NS_BINDING_REDIRECTED;
    mListener = nsnull;
    mListenerContext = nsnull;
    return rv;
}

PRBool
nsHttpChannel::ResponseWouldVary()
{
    PRBool result = PR_FALSE;
    nsCAutoString buf, metaKey;
    mCachedResponseHead->GetHeader(nsHttp::Vary, buf);
    if (!buf.IsEmpty()) {
        NS_NAMED_LITERAL_CSTRING(prefix, "request-");

        // enumerate the elements of the Vary header...
        char *val = buf.BeginWriting(); // going to munge buf
        char *token = nsCRT::strtok(val, NS_HTTP_HEADER_SEPS, &val);
        while (token) {
            //
            // if "*", then assume response would vary.  technically speaking,
            // "Vary: header, *" is not permitted, but we allow it anyways.
            //
            // if the response depends on the value of the "Cookie" header, then
            // bail since we do not store cookies in the cache.  this is done
            // for the following reasons:
            //
            //   1- cookies can be very large in size
            //
            //   2- cookies may contain sensitive information.  (for parity with
            //      out policy of not storing Set-cookie headers in the cache
            //      meta data, we likewise do not want to store cookie headers
            //      here.)
            //
            // this implementation is obviously not fully standards compliant, but
            // it is perhaps most prudent given the above issues.
            //
            if ((*token == '*') || (PL_strcasecmp(token, "cookie") == 0)) {
                result = PR_TRUE;
                break;
            }
            else {
                // build cache meta data key...
                metaKey = prefix + nsDependentCString(token);

                // check the last value of the given request header to see if it has
                // since changed.  if so, then indeed the cached response is invalid.
                nsXPIDLCString lastVal;
                mCacheEntry->GetMetaDataElement(metaKey.get(), getter_Copies(lastVal));
                if (lastVal) {
                    nsHttpAtom atom = nsHttp::ResolveAtom(token);
                    const char *newVal = mRequestHead.PeekHeader(atom);
                    if (newVal && (strcmp(newVal, lastVal) != 0)) {
                        result = PR_TRUE; // yes, response would vary
                        break;
                    }
                }
                
                // next token...
                token = nsCRT::strtok(val, NS_HTTP_HEADER_SEPS, &val);
            }
        }
    }
    return result;
}

//-----------------------------------------------------------------------------
// nsHttpChannel <byte-range>
//-----------------------------------------------------------------------------

nsresult
nsHttpChannel::SetupByteRangeRequest(PRUint32 partialLen)
{
    // cached content has been found to be partial, add necessary request
    // headers to complete cache entry.

    // use strongest validator available...
    const char *val = mCachedResponseHead->PeekHeader(nsHttp::ETag);
    if (!val)
        val = mCachedResponseHead->PeekHeader(nsHttp::Last_Modified);
    if (!val) {
        // if we hit this code it means mCachedResponseHead->IsResumable() is
        // either broken or not being called.
        NS_NOTREACHED("no cache validator");
        return NS_ERROR_FAILURE;
    }

    char buf[32];
    PR_snprintf(buf, sizeof(buf), "bytes=%u-", partialLen);

    mRequestHead.SetHeader(nsHttp::Range, nsDependentCString(buf));
    mRequestHead.SetHeader(nsHttp::If_Range, nsDependentCString(val));

    return NS_OK;
}

nsresult
nsHttpChannel::ProcessPartialContent()
{
    nsresult rv;

    // ok, we've just received a 206
    //
    // we need to stream whatever data is in the cache out first, and then
    // pick up whatever data is on the wire, writing it into the cache.

    LOG(("nsHttpChannel::ProcessPartialContent [this=%x]\n", this)); 

    NS_ENSURE_TRUE(mCachedResponseHead, NS_ERROR_NOT_INITIALIZED);
    NS_ENSURE_TRUE(mCacheEntry, NS_ERROR_NOT_INITIALIZED);

    // suspend the current transaction
    rv = mTransactionPump->Suspend();
    if (NS_FAILED(rv)) return rv;

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

    // notify observers interested in looking at a response that has been
    // merged with any cached headers (http-on-examine-merged-response).
    gHttpHandler->OnExamineMergedResponse(this);

    // the cached content is valid, although incomplete.
    mCachedContentIsValid = PR_TRUE;
    return ReadFromCache();
}

nsresult
nsHttpChannel::OnDoneReadingPartialCacheEntry(PRBool *streamDone)
{
    nsresult rv;

    LOG(("nsHttpChannel::OnDoneReadingPartialCacheEntry [this=%x]", this));

    // by default, assume we would have streamed all data or failed...
    *streamDone = PR_TRUE;

    // setup cache listener to append to cache entry
    PRUint32 size;
    rv = mCacheEntry->GetDataSize(&size);
    if (NS_FAILED(rv)) return rv;

    rv = InstallCacheListener(size);
    if (NS_FAILED(rv)) return rv;

    // need to track the logical offset of the data being sent to our listener
    mLogicalOffset = size;

    // we're now completing the cached content, so we can clear this flag.
    // this puts us in the state of a regular download.
    mCachedContentIsPartial = PR_FALSE;

    // resume the transaction if it exists, otherwise the pipe contained the
    // remaining part of the document and we've now streamed all of the data.
    if (mTransactionPump) {
        rv = mTransactionPump->Resume();
        if (NS_SUCCEEDED(rv))
            *streamDone = PR_FALSE;
    }
    else
        NS_NOTREACHED("no transaction");
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

    // notify observers interested in looking at a reponse that has been
    // merged with any cached headers
    gHttpHandler->OnExamineMergedResponse(this);

    mCachedContentIsValid = PR_TRUE;
    rv = ReadFromCache();
    if (NS_FAILED(rv)) return rv;

    mTransactionReplaced = PR_TRUE;
    return NS_OK;
}

nsresult
nsHttpChannel::OpenCacheEntry(PRBool offline, PRBool *delayed)
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
            mPostID = gHttpHandler->GenerateUniqueID();
    }
    else if ((mRequestHead.Method() != nsHttp::Get) &&
             (mRequestHead.Method() != nsHttp::Head)) {
        // don't use the cache for other types of requests
        return NS_OK;
    }
    else if (mRequestHead.PeekHeader(nsHttp::Range)) {
        // we don't support caching for byte range requests initiated
        // by our clients or via nsIResumableChannel.
        // XXX perhaps we could munge their byte range into the cache
        // key to make caching sort'a work.
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
    rv = gHttpHandler->GetCacheSession(storagePolicy,
                                       getter_AddRefs(session));
    if (NS_FAILED(rv)) return rv;

    // Set the desired cache access mode accordingly...
    nsCacheAccessMode accessRequested;
    if (offline)
        accessRequested = nsICache::ACCESS_READ; // have no way of writing to cache
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
    else if (NS_SUCCEEDED(rv)) {
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
    const char *spec = mSpec.get();
    const char *p = strchr(spec, '#');
    if (p)
        cacheKey.Append(spec, p - spec);
    else
        cacheKey.Append(spec);
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
    NS_ENSURE_TRUE(mResponseHead, NS_ERROR_FAILURE);

    PRUint32 expirationTime = 0;
    if (!mResponseHead->MustValidate()) {
        PRUint32 freshnessLifetime = 0;
        nsresult rv;

        rv = mResponseHead->ComputeFreshnessLifetime(&freshnessLifetime);
        if (NS_FAILED(rv)) return rv;

        if (freshnessLifetime > 0) {
            PRUint32 now = NowInSeconds(), currentAge = 0;

            rv = mResponseHead->ComputeCurrentAge(now, mRequestTime, &currentAge); 
            if (NS_FAILED(rv)) return rv;

            LOG(("freshnessLifetime = %u, currentAge = %u\n",
                freshnessLifetime, currentAge));

            if (freshnessLifetime > currentAge) {
                PRUint32 timeRemaining = freshnessLifetime - currentAge;
                // be careful... now + timeRemaining may overflow
                if (now + timeRemaining < now)
                    expirationTime = PRUint32(-1);
                else
                    expirationTime = now + timeRemaining;
            }
            else
                expirationTime = now;
        }
    }
    return mCacheEntry->SetExpirationTime(expirationTime);
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

    // We'll need this value in later computations...
    PRUint32 lastModifiedTime;
    rv = mCacheEntry->GetLastModified(&lastModifiedTime);
    if (NS_FAILED(rv)) return rv;

    // Determine if this is the first time that this cache entry
    // has been accessed during this session.
    PRBool fromPreviousSession =
            (gHttpHandler->SessionStartTime() > lastModifiedTime);

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
    // of the cached content, then the cached response is partial...
    // either we need to issue a byte range request or we need to refetch the
    // entire document.
    nsInt64 contentLength = mCachedResponseHead->ContentLength();
    if (contentLength != nsInt64(-1)) {
        PRUint32 size;
        rv = mCacheEntry->GetDataSize(&size);
        if (NS_FAILED(rv)) return rv;

        if (nsInt64(size) != contentLength) {
            LOG(("Cached data size does not match the Content-Length header "
                 "[content-length=%lld size=%u]\n", PRInt64(contentLength), size));
            if ((nsInt64(size) < contentLength) && mCachedResponseHead->IsResumable()) {
                // looks like a partial entry.
                rv = SetupByteRangeRequest(size);
                if (NS_FAILED(rv)) return rv;
                mCachedContentIsPartial = PR_TRUE;
            }
            return NS_OK;
        }
    }

    PRBool doValidation = PR_FALSE;

    // Be optimistic: assume that we won't need to do validation
    mRequestHead.ClearHeader(nsHttp::If_Modified_Since);
    mRequestHead.ClearHeader(nsHttp::If_None_Match);

    // If the LOAD_FROM_CACHE flag is set, any cached data can simply be used.
    if (mLoadFlags & LOAD_FROM_CACHE) {
        LOG(("NOT validating based on LOAD_FROM_CACHE load flag\n"));
        doValidation = PR_FALSE;
    }
    // If the VALIDATE_ALWAYS flag is set, any cached data won't be used until
    // it's revalidated with the server.
    else if (mLoadFlags & VALIDATE_ALWAYS) {
        LOG(("Validating based on VALIDATE_ALWAYS load flag\n"));
        doValidation = PR_TRUE;
    }
    // Even if the VALIDATE_NEVER flag is set, there are still some cases in
    // which we must validate the cached response with the server.
    else if (mLoadFlags & VALIDATE_NEVER) {
        LOG(("VALIDATE_NEVER set\n"));
        // if no-store or if no-cache and ssl, validate cached response (see
        // bug 112564 for an explanation of this logic)
        if (mCachedResponseHead->NoStore() ||
           (mCachedResponseHead->NoCache() && mConnectionInfo->UsingSSL())) {
            LOG(("Validating based on (no-store || (no-cache && ssl)) logic\n"));
            doValidation = PR_TRUE;
        }
        else {
            LOG(("NOT validating based on VALIDATE_NEVER load flag\n"));
            doValidation = PR_FALSE;
        }
    }
    // check if validation is strictly required...
    else if (mCachedResponseHead->MustValidate()) {
        LOG(("Validating based on MustValidate() returning TRUE\n"));
        doValidation = PR_TRUE;
    }

    else if (ResponseWouldVary()) {
        LOG(("Validating based on Vary headers returning TRUE\n"));
        doValidation = PR_TRUE;
    }
    // Check if the cache entry has expired...
    else {
        PRUint32 time = 0; // a temporary variable for storing time values...

        rv = mCacheEntry->GetExpirationTime(&time);
        if (NS_FAILED(rv)) return rv;

        if (NowInSeconds() <= time)
            doValidation = PR_FALSE;
        else if (mCachedResponseHead->MustValidateIfExpired())
            doValidation = PR_TRUE;
        else if (mLoadFlags & VALIDATE_ONCE_PER_SESSION) {
            // If the cached response does not include expiration infor-
            // mation, then we must validate the response, despite whether
            // or not this is the first access this session.  This behavior
            // is consistent with existing browsers and is generally expected
            // by web authors.
            rv = mCachedResponseHead->ComputeFreshnessLifetime(&time);
            if (NS_FAILED(rv)) return rv;

            if (time == 0)
                doValidation = PR_TRUE;
            else
                doValidation = fromPreviousSession;
        }
        else
            doValidation = PR_TRUE;

        LOG(("%salidating based on expiration time\n", doValidation ? "V" : "Not v"));
    }

    if (!doValidation) {
        //
        // Check the authorization headers used to generate the cache entry.
        // We must validate the cache entry if:
        //
        // 1) the cache entry was generated prior to this session w/
        //    credentials (see bug 103402).
        // 2) the cache entry was generated w/o credentials, but would now
        //    require credentials (see bug 96705).
        //
        // NOTE: this does not apply to proxy authentication.
        //
        mCacheEntry->GetMetaDataElement("auth", getter_Copies(buf));
        doValidation =
            (fromPreviousSession && !buf.IsEmpty()) ||
            (buf.IsEmpty() && mRequestHead.PeekHeader(nsHttp::Authorization));
    }

    if (!doValidation) {
        // Sites redirect back to the original URI after setting a session/tracking
        // cookie. In such cases, force revalidation so that we hit the net and do not
        // cycle thru cached responses.
        PRInt16 status = mCachedResponseHead->Status();
        const char* cookie = mRequestHead.PeekHeader(nsHttp::Cookie);
        if (cookie && 
            (status == 300 || status == 301 || status == 302 ||
             status == 303 || status == 307))
             doValidation = PR_TRUE;
    }

    mCachedContentIsValid = !doValidation;

    if (doValidation) {
        //
        // now, we are definitely going to issue a HTTP request to the server.
        // make it conditional if possible.
        //
        // do not attempt to validate no-store content, since servers will not
        // expect it to be cached.  (we only keep it in our cache for the
        // purposes of back/forward, etc.)
        //
        // the request method MUST be either GET or HEAD (see bug 175641).
        //
        if (!mCachedResponseHead->NoStore() &&
            (mRequestHead.Method() == nsHttp::Get ||
             mRequestHead.Method() == nsHttp::Head)) {
            const char *val;
            // Add If-Modified-Since header if a Last-Modified was given
            val = mCachedResponseHead->PeekHeader(nsHttp::Last_Modified);
            if (val)
                mRequestHead.SetHeader(nsHttp::If_Modified_Since,
                                       nsDependentCString(val));
            // Add If-None-Match header if an ETag was given in the response
            val = mCachedResponseHead->PeekHeader(nsHttp::ETag);
            if (val)
                mRequestHead.SetHeader(nsHttp::If_None_Match,
                                       nsDependentCString(val));
        }
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

    if ((mCacheAccess & nsICache::ACCESS_WRITE) && !mCachedContentIsPartial) {
        // We have write access to the cache, but we don't need to go to the
        // server to validate at this time, so just mark the cache entry as
        // valid in order to allow others access to this cache entry.
        mCacheEntry->MarkValid();
    }

    // if this is a cached redirect, we must process the redirect asynchronously
    // since AsyncOpen may not have returned yet.  Make sure there is a Location
    // header, otherwise we'll have to treat this like a normal 200 response.
    if (mResponseHead && (mResponseHead->Status() / 100 == 3) 
                      && (mResponseHead->PeekHeader(nsHttp::Location)))
        return AsyncCall(&nsHttpChannel::HandleAsyncRedirect);

    // have we been configured to skip reading from the cache?
    if ((mLoadFlags & LOAD_ONLY_IF_MODIFIED) && !mCachedContentIsPartial) {
        LOG(("skipping read from cache based on LOAD_ONLY_IF_MODIFIED load flag\n"));
        return AsyncCall(&nsHttpChannel::HandleAsyncNotModified);
    }

    // open input stream for reading...
    nsCOMPtr<nsIInputStream> stream;
    rv = mCacheEntry->OpenInputStream(0, getter_AddRefs(stream));
    if (NS_FAILED(rv)) return rv;

    rv = NS_NewInputStreamPump(getter_AddRefs(mCachePump),
                               stream, -1, -1, 0, 0, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    return mCachePump->AsyncRead(this, mListenerContext);
}

nsresult
nsHttpChannel::CloseCacheEntry(nsresult status)
{
    nsresult rv = NS_OK;
    if (mCacheEntry) {
        LOG(("nsHttpChannel::CloseCacheEntry [this=%x status=%x]", this, status));

        // don't doom the cache entry if only reading from it...
        if (NS_FAILED(status)
                && (mCacheAccess & nsICache::ACCESS_WRITE) && !mCachePump) {
            LOG(("dooming cache entry!!"));
            rv = mCacheEntry->Doom();
        }

        if (mCachedResponseHead) {
            delete mCachedResponseHead;
            mCachedResponseHead = 0;
        }

        mCachePump = 0;
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
    nsresult rv;

    NS_ENSURE_TRUE(mCacheEntry, NS_ERROR_UNEXPECTED);
    NS_ENSURE_TRUE(mCacheAccess & nsICache::ACCESS_WRITE, NS_ERROR_UNEXPECTED);

    // Don't cache the response again if already cached...
    if (mCachedContentIsValid)
        return NS_OK;

    LOG(("nsHttpChannel::InitCacheEntry [this=%x entry=%x]\n",
        this, mCacheEntry.get()));

    // The no-store directive within the 'Cache-Control:' header indicates
    // that we must not store the response in a persistent cache.
    if (mResponseHead->NoStore())
        mLoadFlags |= INHIBIT_PERSISTENT_CACHING;

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

    // Store the HTTP authorization scheme used if any...
    rv = StoreAuthorizationMetaData();
    if (NS_FAILED(rv)) return rv;

    // Iterate over the headers listed in the Vary response header, and
    // store the value of the corresponding request header so we can verify
    // that it has not varied when we try to re-use the cached response at
    // a later time.  Take care not to store "Cookie" headers though.  We
    // take care of "Vary: cookie" in ResponseWouldVary.
    //
    // NOTE: if "Vary: accept, cookie", then we will store the "accept" header
    // in the cache.  we could try to avoid needlessly storing the "accept"
    // header in this case, but it doesn't seem worth the extra code to perform
    // the check.
    {
        nsCAutoString buf, metaKey;
        mResponseHead->GetHeader(nsHttp::Vary, buf);
        if (!buf.IsEmpty()) {
            NS_NAMED_LITERAL_CSTRING(prefix, "request-");
           
            char *val = buf.BeginWriting(); // going to munge buf
            char *token = nsCRT::strtok(val, NS_HTTP_HEADER_SEPS, &val);
            while (token) {
                if ((*token != '*') && (PL_strcasecmp(token, "cookie") != 0)) {
                    nsHttpAtom atom = nsHttp::ResolveAtom(token);
                    const char *requestVal = mRequestHead.PeekHeader(atom);
                    if (requestVal) {
                        // build cache meta data key and set meta data element...
                        metaKey = prefix + nsDependentCString(token);
                        mCacheEntry->SetMetaDataElement(metaKey.get(), requestVal);
                    }
                }
                token = nsCRT::strtok(val, NS_HTTP_HEADER_SEPS, &val);
            }
        }
    }


    // Store the received HTTP head with the cache entry as an element of
    // the meta data.
    nsCAutoString head;
    mResponseHead->Flatten(head, PR_TRUE);
    return mCacheEntry->SetMetaDataElement("response-head", head.get());
}

nsresult
nsHttpChannel::StoreAuthorizationMetaData()
{
    // Not applicable to proxy authorization...
    const char *val = mRequestHead.PeekHeader(nsHttp::Authorization);
    if (val) {
        // eg. [Basic realm="wally world"]
        nsCAutoString buf(Substring(val, strchr(val, ' ')));
        return mCacheEntry->SetMetaDataElement("auth", buf.get());
    }
    return NS_OK;
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
        // Set the expiration time for this cache entry
        nsresult rv = UpdateExpirationTime();
        if (NS_FAILED(rv)) return rv;
    }
    return NS_OK;
}

// Open an output stream to the cache entry and insert a listener tee into
// the chain of response listeners.
nsresult
nsHttpChannel::InstallCacheListener(PRUint32 offset)
{
    nsresult rv;

    LOG(("Preparing to write data into the cache [uri=%s]\n", mSpec.get()));

    NS_ASSERTION(mCacheEntry, "no cache entry");
    NS_ASSERTION(mListener, "no listener");

    nsCOMPtr<nsIOutputStream> out;
    rv = mCacheEntry->OpenOutputStream(offset, getter_AddRefs(out));
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
nsHttpChannel::SetupReplacementChannel(nsIURI       *newURI, 
                                       nsIChannel   *newChannel,
                                       PRBool        preserveMethod)
{
    PRUint32 newLoadFlags = mLoadFlags | LOAD_REPLACE;
    // if the original channel was using SSL and this channel is not using
    // SSL, then no need to inhibit persistent caching.  however, if the
    // original channel was not using SSL and has INHIBIT_PERSISTENT_CACHING
    // set, then allow the flag to apply to the redirected channel as well.
    // since we force set INHIBIT_PERSISTENT_CACHING on all HTTPS channels,
    // we only need to check if the original channel was using SSL.
    if (mConnectionInfo->UsingSSL())
        newLoadFlags &= ~INHIBIT_PERSISTENT_CACHING;

    newChannel->SetOriginalURI(mOriginalURI);
    newChannel->SetLoadGroup(mLoadGroup); 
    newChannel->SetNotificationCallbacks(mCallbacks);
    newChannel->SetLoadFlags(newLoadFlags);

    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(newChannel);
    if (!httpChannel)
        return NS_OK; // no other options to set

    if (preserveMethod) {
        nsCOMPtr<nsIUploadChannel> uploadChannel = do_QueryInterface(httpChannel);
        if (mUploadStream && uploadChannel) {
            // rewind upload stream
            nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(mUploadStream);
            if (seekable)
                seekable->Seek(nsISeekableStream::NS_SEEK_SET, 0);

            // replicate original call to SetUploadStream...
            if (mUploadStreamHasHeaders)
                uploadChannel->SetUploadStream(mUploadStream, EmptyCString(), -1);
            else {
                const char *ctype = mRequestHead.PeekHeader(nsHttp::Content_Type);
                const char *clen  = mRequestHead.PeekHeader(nsHttp::Content_Length);
                if (ctype && clen)
                    uploadChannel->SetUploadStream(mUploadStream,
                                                   nsDependentCString(ctype),
                                                   atoi(clen));
            }
        }
        // must happen after setting upload stream since SetUploadStream
        // may change the request method.
        httpChannel->SetRequestMethod(nsDependentCString(mRequestHead.Method()));
    }
    // convey the referrer if one was used for this channel to the next one
    if (mReferrer)
        httpChannel->SetReferrer(mReferrer);
    // convey the mAllowPipelining flag
    httpChannel->SetAllowPipelining(mAllowPipelining);
    // convey the new redirection limit
    httpChannel->SetRedirectionLimit(mRedirectionLimit - 1);

    nsCOMPtr<nsIHttpChannelInternal> httpInternal = do_QueryInterface(newChannel);
    if (httpInternal) {
        // update the DocumentURI indicator since we are being redirected.
        // if this was a top-level document channel, then the new channel
        // should have its mDocumentURI point to newURI; otherwise, we
        // just need to pass along our mDocumentURI to the new channel.
        if (newURI && (mURI == mDocumentURI))
            httpInternal->SetDocumentURI(newURI);
        else
            httpInternal->SetDocumentURI(mDocumentURI);
    } 
    
    // convey the mApplyConversion flag (bug 91862)
    nsCOMPtr<nsIEncodedChannel> encodedChannel = do_QueryInterface(httpChannel);
    if (encodedChannel)
        encodedChannel->SetApplyConversion(mApplyConversion);

    // transfer the resume information
    if (mResuming) {
        nsCOMPtr<nsIResumableChannel> resumableChannel(do_QueryInterface(newChannel));
        if (!resumableChannel) {
            NS_WARNING("Got asked to resume, but redirected to non-resumable channel!");
            return NS_ERROR_NOT_RESUMABLE;
        }
        resumableChannel->ResumeAt(mStartPos, mEntityID);
    }

    return NS_OK;
}

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

    // make sure non-ASCII characters in the location header are escaped.
    nsCAutoString locationBuf;
    if (NS_EscapeURL(location, -1, esc_OnlyNonASCII, locationBuf))
        location = locationBuf.get();

    if (mRedirectionLimit == 0) {
        LOG(("redirection limit reached!\n"));
        // this error code is fatal, and should be conveyed to our listener.
        Cancel(NS_ERROR_REDIRECT_LOOP);
        return NS_ERROR_REDIRECT_LOOP;
    }

    LOG(("redirecting to: %s [redirection-limit=%u]\n",
        location, PRUint32(mRedirectionLimit)));

    nsresult rv;
    nsCOMPtr<nsIChannel> newChannel;
    nsCOMPtr<nsIURI> newURI;

    // create a new URI using the location header and the current URL
    // as a base...
    nsCOMPtr<nsIIOService> ioService;
    rv = gHttpHandler->GetIOService(getter_AddRefs(ioService));
    if (NS_FAILED(rv)) return rv;

    // the new uri should inherit the origin charset of the current uri
    nsCAutoString originCharset;
    rv = mURI->GetOriginCharset(originCharset);
    if (NS_FAILED(rv))
        originCharset.Truncate();

    rv = ioService->NewURI(nsDependentCString(location), originCharset.get(), mURI,
                           getter_AddRefs(newURI));
    if (NS_FAILED(rv)) return rv;

    // verify that this is a legal redirect
    nsCOMPtr<nsIScriptSecurityManager> securityManager = 
             do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID);
    if (securityManager) {
        rv = securityManager->CheckLoadURI(mURI, newURI,
                                           nsIScriptSecurityManager::DISALLOW_FROM_MAIL |
                                           nsIScriptSecurityManager::DISALLOW_SCRIPT_OR_DATA);
        if (NS_FAILED(rv)) return rv;
    }

    // Kill the current cache entry if we are redirecting
    // back to ourself.
    PRBool redirectingBackToSameURI = PR_FALSE;
    if (mCacheEntry && (mCacheAccess & nsICache::ACCESS_WRITE) &&
        NS_SUCCEEDED(mURI->Equals(newURI, &redirectingBackToSameURI)) &&
        redirectingBackToSameURI)
            mCacheEntry->Doom();

    // move the reference of the old location to the new one if the new
    // one has none.
    nsCOMPtr<nsIURL> newURL = do_QueryInterface(newURI, &rv);
    if (NS_SUCCEEDED(rv)) {
        nsCAutoString ref;
        rv = newURL->GetRef(ref);
        if (NS_SUCCEEDED(rv) && ref.IsEmpty()) {
            nsCOMPtr<nsIURL> baseURL( do_QueryInterface(mURI, &rv) );
            if (NS_SUCCEEDED(rv)) {
                baseURL->GetRef(ref);
                if (!ref.IsEmpty())
                    newURL->SetRef(ref);
            }
        }
    }

    // if we need to re-send POST data then be sure to ask the user first.
    PRBool preserveMethod = (redirectType == 307);
    if (preserveMethod && mUploadStream) {
        rv = PromptTempRedirect();
        if (NS_FAILED(rv)) return rv;
    }

    rv = ioService->NewChannelFromURI(newURI, getter_AddRefs(newChannel));
    if (NS_FAILED(rv)) return rv;

    rv = SetupReplacementChannel(newURI, newChannel, preserveMethod);
    if (NS_FAILED(rv)) return rv;

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

    // close down this channel
    Cancel(NS_BINDING_REDIRECTED);
    
    // disconnect from our listener
    mListener = 0;
    mListenerContext = 0;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpChannel <auth>
//-----------------------------------------------------------------------------

// buf contains "domain\user"
static void
ParseUserDomain(PRUnichar *buf,
                const PRUnichar **user,
                const PRUnichar **domain)
{
    PRUnichar *p = buf;
    while (*p && *p != '\\') ++p;
    if (!*p)
        return;
    *p = '\0';
    *domain = buf;
    *user = p + 1;
}

// helper function for setting identity from raw user:pass
static void
SetIdent(nsHttpAuthIdentity &ident,
         PRUint32 authFlags,
         PRUnichar *userBuf,
         PRUnichar *passBuf)
{
    const PRUnichar *user = userBuf;
    const PRUnichar *domain = nsnull;

    if (authFlags & nsIHttpAuthenticator::IDENTITY_INCLUDES_DOMAIN)
        ParseUserDomain(userBuf, &user, &domain);

    ident.Set(domain, user, passBuf);
}

// generate credentials for the given challenge, and update the auth cache.
nsresult
nsHttpChannel::GenCredsAndSetEntry(nsIHttpAuthenticator *auth,
                                   PRBool proxyAuth,
                                   const char *scheme,
                                   const char *host,
                                   PRInt32 port,
                                   const char *directory,
                                   const char *realm,
                                   const char *challenge,
                                   const nsHttpAuthIdentity &ident,
                                   nsCOMPtr<nsISupports> &sessionState,
                                   char **result)
{
    nsresult rv;
    PRUint32 authFlags;

    rv = auth->GetAuthFlags(&authFlags);
    if (NS_FAILED(rv)) return rv;

    nsISupports *ss = sessionState;
    rv = auth->GenerateCredentials(this,
                                   challenge,
                                   proxyAuth,
                                   ident.Domain(),
                                   ident.User(),
                                   ident.Password(),
                                   &ss,
                                   &mAuthContinuationState,
                                   result);
    sessionState.swap(ss);
    if (NS_FAILED(rv)) return rv;

    // don't log this in release build since it could contain sensitive info.
#ifdef DEBUG 
    LOG(("generated creds: %s\n", *result));
#endif

    // find out if this authenticator allows reuse of credentials and/or
    // challenge.
    PRBool saveCreds =
        authFlags & nsIHttpAuthenticator::REUSABLE_CREDENTIALS;
    PRBool saveChallenge =
        authFlags & nsIHttpAuthenticator::REUSABLE_CHALLENGE;

    // this getter never fails
    nsHttpAuthCache *authCache = gHttpHandler->AuthCache();

    // create a cache entry.  we do this even though we don't yet know that
    // these credentials are valid b/c we need to avoid prompting the user
    // more than once in case the credentials are valid.
    //
    // if the credentials are not reusable, then we don't bother sticking
    // them in the auth cache.
    rv = authCache->SetAuthEntry(scheme, host, port, directory, realm,
                                 saveCreds ? *result : nsnull,
                                 saveChallenge ? challenge : nsnull,
                                 ident, sessionState);
    return rv;
}

nsresult
nsHttpChannel::ProcessAuthentication(PRUint32 httpStatus)
{
    LOG(("nsHttpChannel::ProcessAuthentication [this=%x code=%u]\n",
        this, httpStatus));

    const char *challenges;
    PRBool proxyAuth = (httpStatus == 407);

    if (proxyAuth)
        challenges = mResponseHead->PeekHeader(nsHttp::Proxy_Authenticate);
    else
        challenges = mResponseHead->PeekHeader(nsHttp::WWW_Authenticate);
    NS_ENSURE_TRUE(challenges, NS_ERROR_UNEXPECTED);

    nsCAutoString creds;
    nsresult rv = GetCredentials(challenges, proxyAuth, creds);
    if (NS_FAILED(rv))
        LOG(("unable to authenticate\n"));
    else {
        // set the authentication credentials
        if (proxyAuth)
            mRequestHead.SetHeader(nsHttp::Proxy_Authorization, creds);
        else
            mRequestHead.SetHeader(nsHttp::Authorization, creds);

        mAuthRetryPending = PR_TRUE; // see DoAuthRetry
    }
    return rv;
}

nsresult
nsHttpChannel::GetCredentials(const char *challenges,
                              PRBool proxyAuth,
                              nsAFlatCString &creds)
{
    nsCAutoString challenge, scheme;
    nsCOMPtr<nsIHttpAuthenticator> auth;

    nsresult rv = NS_ERROR_NOT_AVAILABLE;
    
    // figure out which challenge we can handle and which authenticator to use.
    for (const char *eol = challenges - 1; eol; ) {
        const char *p = eol + 1;

        // get the challenge string (LF separated -- see nsHttpHeaderArray)
        if ((eol = strchr(p, '\n')) != nsnull)
            challenge.Assign(p, eol - p);
        else
            challenge.Assign(p);

        rv = ParseChallenge(challenge.get(), scheme, getter_AddRefs(auth));
        if (NS_SUCCEEDED(rv)) {
            //
            // we allow the routines to run all the way through before we
            // decide if they are valid.
            //
            // we don't worry about the auth cache being altered because that
            // would have been the last step, and if the error is from updating
            // the authcache it wasn't really altered anyway. -CTN 
            //
            // at this point the code is really only useful for client side
            // errors (it will not automatically fail over to do a different
            // auth type if the server keeps rejecting what is being sent, even
            // if a particular auth method only knows 1 thing, like a
            // non-identity based authentication method)
            //
            rv = GetCredentialsForChallenge(challenge.get(), scheme.get(),
                                            proxyAuth, auth, creds);
            if (NS_SUCCEEDED(rv))
                break;
        }
    }
    return rv;
}

nsresult
nsHttpChannel::GetCredentialsForChallenge(const char *challenge,
                                          const char *authType,
                                          PRBool proxyAuth,
                                          nsIHttpAuthenticator *auth,
                                          nsAFlatCString &creds)
{
    LOG(("nsHttpChannel::GetCredentialsForChallenge [this=%x proxyAuth=%d challenges=%s]\n",
        this, proxyAuth, challenge));

    // this getter never fails
    nsHttpAuthCache *authCache = gHttpHandler->AuthCache();

    PRUint32 authFlags;
    nsresult rv = auth->GetAuthFlags(&authFlags);
    if (NS_FAILED(rv)) return rv;

    nsCAutoString realm;
    ParseRealm(challenge, realm);

    // if no realm, then use the auth type as the realm.  ToUpperCase so the
    // ficticious realm stands out a bit more.
    // XXX this will cause some single signon misses!
    // XXX this will cause problems when we expose the auth cache to OJI!
    // XXX this was meant to be used with NTLM, which supplies no realm.
    /*
    if (realm.IsEmpty()) {
        realm = authType;
        ToUpperCase(realm);
    }
    */

    const char *host;
    PRInt32 port;
    nsHttpAuthIdentity *ident;
    nsCAutoString path, scheme;
    PRBool identFromURI = PR_FALSE;

    // it is possible for the origin server to fake a proxy challenge.  if
    // that happens we need to be sure to use the origin server as the auth
    // domain.  otherwise, we could inadvertantly expose the user's proxy
    // credentials to an origin server.

    if (proxyAuth && mConnectionInfo->UsingHttpProxy()) {
        host = mConnectionInfo->ProxyHost();
        port = mConnectionInfo->ProxyPort();
        ident = &mProxyIdent;
        scheme.AssignLiteral("http");
    }
    else {
        host = mConnectionInfo->Host();
        port = mConnectionInfo->Port();
        ident = &mIdent;

        rv = GetCurrentPath(path);
        if (NS_FAILED(rv)) return rv;

        rv = mURI->GetScheme(scheme);
        if (NS_FAILED(rv)) return rv;

        // if this is the first challenge, then try using the identity
        // specified in the URL.
        if (mIdent.IsEmpty()) {
            GetIdentityFromURI(authFlags, mIdent);
            identFromURI = !mIdent.IsEmpty();
        }
    }

    //
    // if we already tried some credentials for this transaction, then
    // we need to possibly clear them from the cache, unless the credentials
    // in the cache have changed, in which case we'd want to give them a
    // try instead.
    //
    nsHttpAuthEntry *entry = nsnull;
    authCache->GetAuthEntryForDomain(scheme.get(), host, port, realm.get(), &entry);

    // hold reference to the auth session state (in case we clear our
    // reference to the entry).
    nsCOMPtr<nsISupports> sessionStateGrip;
    if (entry)
        sessionStateGrip = entry->mMetaData;

    // for digest auth, maybe our cached nonce value simply timed out...
    PRBool identityInvalid;
    nsISupports *sessionState = sessionStateGrip;
    rv = auth->ChallengeReceived(this,
                                 challenge,
                                 proxyAuth,
                                 &sessionState,
                                 &mAuthContinuationState,
                                 &identityInvalid);
    sessionStateGrip.swap(sessionState);
    if (NS_FAILED(rv)) return rv;

    if (identityInvalid) {
        if (entry) {
            if (ident->Equals(entry->Identity())) {
                LOG(("  clearing bad auth cache entry\n"));
                // ok, we've already tried this user identity, so clear the
                // corresponding entry from the auth cache.
                ClearPasswordManagerEntry(scheme.get(), host, port, realm.get(), entry->User());
                authCache->ClearAuthEntry(scheme.get(), host, port, realm.get());
                entry = nsnull;
                ident->Clear();
            }
            else if (!identFromURI || nsCRT::strcmp(ident->User(), entry->Identity().User()) == 0) {
                LOG(("  taking identity from auth cache\n"));
                // the password from the auth cache is more likely to be
                // correct than the one in the URL.  at least, we know that it
                // works with the given username.  it is possible for a server
                // to distinguish logons based on the supplied password alone,
                // but that would be quite unusual... and i don't think we need
                // to worry about such unorthodox cases.
                ident->Set(entry->Identity());
                identFromURI = PR_FALSE;
                if (entry->Creds()[0] != '\0') {
                    LOG(("    using cached credentials!\n"));
                    creds.Assign(entry->Creds());
                    return entry->AddPath(path.get());
                }
            }
        }
        else if (!identFromURI) {
            // hmm... identity invalid, but no auth entry!  the realm probably
            // changed (see bug 201986).
            ident->Clear();
        }

        if (!entry && ident->IsEmpty()) {
            // at this point we are forced to interact with the user to get
            // their username and password for this domain.
            rv = PromptForIdentity(scheme.get(), host, port, proxyAuth, realm.get(), 
                                   authType, authFlags, *ident);
            if (NS_FAILED(rv)) return rv;
            identFromURI = PR_FALSE;
        }
    }

    if (identFromURI) {
        // Warn the user before automatically using the identity from the URL
        // to automatically log them into a site (see bug 232567).
        if (!ConfirmAuth(NS_LITERAL_STRING("AutomaticAuth"), PR_FALSE)) {
            // calling cancel here sets our mStatus and aborts the HTTP
            // transaction, which prevents OnDataAvailable events.
            Cancel(NS_ERROR_ABORT);
            // this return code alone is not equivalent to Cancel, since
            // it only instructs our caller that authentication failed.
            // without an explicit call to Cancel, our caller would just
            // load the page that accompanies the HTTP auth challenge.
            return NS_ERROR_ABORT;
        }
    }

    //
    // get credentials for the given user:pass
    //
    // always store the credentials we're trying now so that they will be used
    // on subsequent links.  This will potentially remove good credentials from
    // the cache.  This is ok as we don't want to use cached credentials if the
    // user specified something on the URI or in another manner.  This is so
    // that we don't transparently authenticate as someone they're not
    // expecting to authenticate as.
    //
    nsXPIDLCString result;
    rv = GenCredsAndSetEntry(auth, proxyAuth, scheme.get(), host, port, path.get(),
                             realm.get(), challenge, *ident, sessionStateGrip,
                             getter_Copies(result));
    if (NS_SUCCEEDED(rv))
        creds = result;
    return rv;
}

nsresult
nsHttpChannel::ParseChallenge(const char *challenge,
                              nsCString &authType,
                              nsIHttpAuthenticator **auth)
{
    LOG(("nsHttpChannel::ParseChallenge [this=%x]\n", this));

    const char *p;
  
    // get the challenge type
    if ((p = strchr(challenge, ' ')) != nsnull)
        authType.Assign(challenge, p - challenge);
    else
        authType.Assign(challenge);
  
    // normalize to lowercase
    ToLowerCase(authType);

    nsCAutoString contractid;
    contractid.Assign(NS_HTTP_AUTHENTICATOR_CONTRACTID_PREFIX);
    contractid.Append(authType);

    return CallGetService(contractid.get(), auth);
}

void
nsHttpChannel::GetIdentityFromURI(PRUint32 authFlags, nsHttpAuthIdentity &ident)
{
    LOG(("nsHttpChannel::GetIdentityFromURI [this=%x]\n", this));

    nsAutoString userBuf;
    nsAutoString passBuf;

    // XXX i18n
    nsCAutoString buf;
    mURI->GetUsername(buf);
    if (!buf.IsEmpty()) {
        NS_UnescapeURL(buf);
        CopyASCIItoUCS2(buf, userBuf);
        mURI->GetPassword(buf);
        if (!buf.IsEmpty()) {
            NS_UnescapeURL(buf);
            CopyASCIItoUCS2(buf, passBuf);
        }
    }

    if (!userBuf.IsEmpty())
        SetIdent(ident, authFlags, (PRUnichar *) userBuf.get(), (PRUnichar *) passBuf.get());
}

void
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
}

nsresult
nsHttpChannel::PromptForIdentity(const char *scheme,
                                 const char *host,
                                 PRInt32     port,
                                 PRBool      proxyAuth,
                                 const char *realm,
                                 const char *authType,
                                 PRUint32 authFlags,
                                 nsHttpAuthIdentity &ident)
{
    LOG(("nsHttpChannel::PromptForIdentity [this=%x]\n", this));

    // XXX authType should be included in the prompt

    // XXX i18n: IDN not supported.

    nsCOMPtr<nsIAuthPrompt> authPrompt;
    GetCallback(NS_GET_IID(nsIAuthPrompt), getter_AddRefs(authPrompt));
    if (!authPrompt)
        return NS_ERROR_NO_INTERFACE;

    //
    // construct the single signon key
    //
    // we always add the port to domain since it is used as the key for storing
    // in password maanger.  THE FORMAT OF THIS KEY IS SACROSANCT!!  do not
    // even think about changing the format of this key.
    //
    // XXX we need to prefix this with "scheme://" at some point.  however, that
    // has to be done very carefully and probably with some cooperation from the
    // password manager to ensure that passwords remembered under the old key
    // format are not lost.
    //
    nsAutoString key;
    CopyASCIItoUTF16(host, key); // XXX IDN?
    key.Append(PRUnichar(':'));
    key.AppendInt(port);
    key.AppendLiteral(" (");
    AppendASCIItoUTF16(realm, key);
    key.Append(PRUnichar(')'));

    nsresult rv;

    // construct the message string
    nsCOMPtr<nsIStringBundleService> bundleSvc =
            do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIStringBundle> bundle;
    rv = bundleSvc->CreateBundle(NECKO_MSGS_URL, getter_AddRefs(bundle));
    if (NS_FAILED(rv)) return rv;

    // figure out what message to display...
    nsAutoString displayHost;
    CopyASCIItoUTF16(host, displayHost); // XXX IDN?
    // Add port only if it was originally specified in the URI
    PRInt32 uriPort = -1;
    mURI->GetPort(&uriPort);
    if (uriPort != -1) {
        displayHost.Append(PRUnichar(':'));
        displayHost.AppendInt(port);
    }
    nsXPIDLString message;
    if (proxyAuth) {
        const PRUnichar *strings[] = { displayHost.get() };
        rv = bundle->FormatStringFromName(
                        NS_LITERAL_STRING("EnterUserPasswordForProxy").get(),
                        strings, 1,
                        getter_Copies(message));
    }
    else {
        nsAutoString realmU;
        realmU.Assign(PRUnichar('\"'));
        AppendASCIItoUTF16(realm, realmU);
        realmU.Append(PRUnichar('\"'));

        // prepend "scheme://" displayHost
        nsAutoString schemeU;
        CopyASCIItoUTF16(scheme, schemeU);
        schemeU.AppendLiteral("://");
        displayHost.Insert(schemeU, 0);

        const PRUnichar *strings[] = { realmU.get(), displayHost.get() };
        rv = bundle->FormatStringFromName(
                        NS_LITERAL_STRING("EnterUserPasswordForRealm").get(),
                        strings, 2,
                        getter_Copies(message));
    }
    if (NS_FAILED(rv)) return rv;

    // prompt the user...
    PRBool retval = PR_FALSE;
    PRUnichar *user = nsnull, *pass = nsnull;
    rv = authPrompt->PromptUsernameAndPassword(nsnull, message.get(),
                                               key.get(),
                                               nsIAuthPrompt::SAVE_PASSWORD_PERMANENTLY,
                                               &user, &pass, &retval);
    if (NS_FAILED(rv)) return rv;

    // remember that we successfully showed the user an auth dialog
    if (!proxyAuth)
        mSuppressDefensiveAuth = PR_TRUE;

    if (!retval || !user || !pass)
        rv = NS_ERROR_ABORT;
    else
        SetIdent(ident, authFlags, user, pass);
  
    if (user) nsMemory::Free(user);
    if (pass) nsMemory::Free(pass);
    return rv;
}

PRBool
nsHttpChannel::ConfirmAuth(const nsString &bundleKey, PRBool doYesNoPrompt)
{
    // skip prompting the user if
    //   1) we've already prompted the user
    //   2) we're not a toplevel channel
    //   3) the userpass length is less than the "phishy" threshold

    if (mSuppressDefensiveAuth || !(mLoadFlags & LOAD_INITIAL_DOCUMENT_URI))
        return PR_TRUE;

    nsresult rv;
    nsCAutoString userPass;
    rv = mURI->GetUserPass(userPass);
    if (NS_FAILED(rv) || (userPass.Length() < gHttpHandler->PhishyUserPassLength()))
        return PR_TRUE;

    // we try to confirm by prompting the user.  if we cannot do so, then
    // assume the user said ok.  this is done to keep things working in
    // embedded builds, where the string bundle might not be present, etc.

    nsCOMPtr<nsIStringBundleService> bundleService =
            do_GetService(NS_STRINGBUNDLE_CONTRACTID);
    if (!bundleService)
        return PR_TRUE;

    nsCOMPtr<nsIStringBundle> bundle;
    bundleService->CreateBundle(NECKO_MSGS_URL, getter_AddRefs(bundle));
    if (!bundle)
        return PR_TRUE;

    nsCAutoString host;
    rv = mURI->GetHost(host);
    if (NS_FAILED(rv))
        return PR_TRUE;

    nsCAutoString user;
    rv = mURI->GetUsername(user);
    if (NS_FAILED(rv))
        return PR_TRUE;

    NS_ConvertUTF8toUTF16 ucsHost(host), ucsUser(user);
    const PRUnichar *strs[2] = { ucsHost.get(), ucsUser.get() };

    nsXPIDLString msg;
    bundle->FormatStringFromName(bundleKey.get(), strs, 2, getter_Copies(msg));
    if (!msg)
        return PR_TRUE;
    
    nsCOMPtr<nsIPrompt> prompt;
    GetCallback(NS_GET_IID(nsIPrompt), getter_AddRefs(prompt));
    if (!prompt)
        return PR_TRUE;

    // do not prompt again
    mSuppressDefensiveAuth = PR_TRUE;

    PRBool confirmed;
    if (doYesNoPrompt) {
        PRInt32 choice;
        rv = prompt->ConfirmEx(nsnull, msg,
                               nsIPrompt::BUTTON_TITLE_YES * nsIPrompt::BUTTON_POS_0 +
                               nsIPrompt::BUTTON_TITLE_NO  * nsIPrompt::BUTTON_POS_1,
                               nsnull, nsnull, nsnull, nsnull, nsnull, &choice);
        if (NS_FAILED(rv))
            return PR_TRUE;

        confirmed = choice == 0;
    }
    else {
        rv = prompt->Confirm(nsnull, msg, &confirmed);
        if (NS_FAILED(rv))
            return PR_TRUE;
    }

    return confirmed;
}

void
nsHttpChannel::CheckForSuperfluousAuth()
{
    // we've been called because it has been determined that this channel is
    // getting loaded without taking the userpass from the URL.  if the URL
    // contained a userpass, then (provided some other conditions are true),
    // we'll give the user an opportunity to abort the channel as this might be
    // an attempt to spoof a different site (see bug 232567).
    if (!mAuthRetryPending) {
        // ask user...
        if (!ConfirmAuth(NS_LITERAL_STRING("SuperfluousAuth"), PR_TRUE)) {
            // calling cancel here sets our mStatus and aborts the HTTP
            // transaction, which prevents OnDataAvailable events.
            Cancel(NS_ERROR_ABORT);
        }
    }
}

void
nsHttpChannel::SetAuthorizationHeader(nsHttpAuthCache *authCache,
                                      nsHttpAtom header,
                                      const char *scheme,
                                      const char *host,
                                      PRInt32 port,
                                      const char *path,
                                      nsHttpAuthIdentity &ident)
{
    nsCOMPtr<nsIHttpAuthenticator> auth;
    nsHttpAuthEntry *entry = nsnull;
    nsresult rv;

    rv = authCache->GetAuthEntryForPath(scheme, host, port, path, &entry);
    if (NS_SUCCEEDED(rv)) {
        // if we are trying to add a header for origin server auth and if the
        // URL contains an explicit username, then try the given username first.
        // we only want to do this, however, if we know the URL requires auth
        // based on the presence of an auth cache entry for this URL (which is
        // true since we are here).  but, if the username from the URL matches
        // the username from the cache, then we should prefer the password
        // stored in the cache since that is most likely to be valid.
        if (header == nsHttp::Authorization && entry->Domain()[0] == '\0') {
            GetIdentityFromURI(0, ident);
            // if the usernames match, then clear the ident so we will pick
            // up the one from the auth cache instead.
            if (nsCRT::strcmp(ident.User(), entry->User()) == 0)
                ident.Clear();
        }
        PRBool identFromURI;
        if (ident.IsEmpty()) {
            ident.Set(entry->Identity());
            identFromURI = PR_FALSE;
        }
        else
            identFromURI = PR_TRUE;

        nsXPIDLCString temp;
        const char *creds     = entry->Creds();
        const char *challenge = entry->Challenge();
        // we can only send a preemptive Authorization header if we have either
        // stored credentials or a stored challenge from which to derive
        // credentials.  if the identity is from the URI, then we cannot use
        // the stored credentials.
        if ((!creds[0] || identFromURI) && challenge[0]) {
            nsCAutoString unused;
            rv = ParseChallenge(challenge, unused, getter_AddRefs(auth));
            if (NS_SUCCEEDED(rv)) {
                PRBool proxyAuth = (header == nsHttp::Proxy_Authorization);
                rv = GenCredsAndSetEntry(auth, proxyAuth, scheme, host, port, path,
                                         entry->Realm(), challenge, ident,
                                         entry->mMetaData, getter_Copies(temp));
                if (NS_SUCCEEDED(rv))
                    creds = temp.get();
            }
        }
        if (creds[0]) {
            LOG(("   adding \"%s\" request header\n", header.get()));
            mRequestHead.SetHeader(header, nsDependentCString(creds));

            // suppress defensive auth prompting for this channel since we know
            // that we already prompted at least once this session.  we only do
            // this for non-proxy auth since the URL's userpass is not used for
            // proxy auth.
            if (header == nsHttp::Authorization)
                mSuppressDefensiveAuth = PR_TRUE;
        }
        else
            ident.Clear(); // don't remember the identity
    }
}

void
nsHttpChannel::AddAuthorizationHeaders()
{
    LOG(("nsHttpChannel::AddAuthorizationHeaders? [this=%x]\n", this));

    // this getter never fails
    nsHttpAuthCache *authCache = gHttpHandler->AuthCache();

    // check if proxy credentials should be sent
    const char *proxyHost = mConnectionInfo->ProxyHost();
    if (proxyHost && mConnectionInfo->UsingHttpProxy())
        SetAuthorizationHeader(authCache, nsHttp::Proxy_Authorization,
                               "http", proxyHost, mConnectionInfo->ProxyPort(),
                               nsnull, // proxy has no path
                               mProxyIdent);

    // check if server credentials should be sent
    nsCAutoString path, scheme;
    if (NS_SUCCEEDED(GetCurrentPath(path)) &&
        NS_SUCCEEDED(mURI->GetScheme(scheme))) {
        SetAuthorizationHeader(authCache, nsHttp::Authorization,
                               scheme.get(),
                               mConnectionInfo->Host(),
                               mConnectionInfo->Port(),
                               path.get(),
                               mIdent);
    }
}

nsresult
nsHttpChannel::GetCurrentPath(nsACString &path)
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

NS_IMPL_THREADSAFE_ADDREF(nsHttpChannel)
NS_IMPL_THREADSAFE_RELEASE(nsHttpChannel)

NS_INTERFACE_MAP_BEGIN(nsHttpChannel)
    NS_INTERFACE_MAP_ENTRY(nsIRequest)
    NS_INTERFACE_MAP_ENTRY(nsIChannel)
    NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
    NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
    NS_INTERFACE_MAP_ENTRY(nsIHttpChannel)
    NS_INTERFACE_MAP_ENTRY(nsICachingChannel)
    NS_INTERFACE_MAP_ENTRY(nsIUploadChannel)
    NS_INTERFACE_MAP_ENTRY(nsICacheListener)
    NS_INTERFACE_MAP_ENTRY(nsIEncodedChannel)
    NS_INTERFACE_MAP_ENTRY(nsIHttpChannelInternal)
    NS_INTERFACE_MAP_ENTRY(nsIResumableChannel)
    NS_INTERFACE_MAP_ENTRY(nsITransportEventSink)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIChannel)
NS_INTERFACE_MAP_END

//-----------------------------------------------------------------------------
// nsHttpChannel::nsIRequest
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::GetName(nsACString &aName)
{
    aName = mSpec;
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
    mCanceled = PR_TRUE;
    mStatus = status;
    if (mTransaction)
        gHttpHandler->CancelTransaction(mTransaction, status);
    else if (mCachePump)
        mCachePump->Cancel(status);
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::Suspend()
{
    LOG(("nsHttpChannel::Suspend [this=%x]\n", this));
    if (mTransactionPump)
        return mTransactionPump->Suspend();
    if (mCachePump)
        return mCachePump->Suspend();

    return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsHttpChannel::Resume()
{
    LOG(("nsHttpChannel::Resume [this=%x]\n", this));
    if (mTransactionPump)
        return mTransactionPump->Resume();
    if (mCachePump)
        return mCachePump->Resume();

    return NS_ERROR_UNEXPECTED;
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
    if (mConnectionInfo && mConnectionInfo->UsingSSL() 
        && !gHttpHandler->IsPersistentHttpsCachingEnabled())
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
nsHttpChannel::GetContentType(nsACString &value)
{
    if (!mResponseHead) {
        // We got no data, we got no headers, we got nothing
        value.Truncate();
        return NS_ERROR_NOT_AVAILABLE;
    }

    if (!mResponseHead->ContentType().IsEmpty()) {
        value = mResponseHead->ContentType();
        return NS_OK;
    }

    
    value.AssignLiteral(UNKNOWN_CONTENT_TYPE);
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::SetContentType(const nsACString &value)
{
    if (mListener) {
        if (!mResponseHead)
            return NS_ERROR_NOT_AVAILABLE;

        nsCAutoString contentTypeBuf, charsetBuf;
        NS_ParseContentType(value, contentTypeBuf, charsetBuf);

        mResponseHead->SetContentType(contentTypeBuf);

        // take care not to stomp on an existing charset
        if (!charsetBuf.IsEmpty())
            mResponseHead->SetContentCharset(charsetBuf);
    } else {
        // We are being given a content-type hint.
        NS_ParseContentType(value, mContentTypeHint, mContentCharsetHint);
    }
    
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetContentCharset(nsACString &value)
{
    if (!mResponseHead)
        return NS_ERROR_NOT_AVAILABLE;

    value = mResponseHead->ContentCharset();
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::SetContentCharset(const nsACString &value)
{
    if (mListener) {
        if (!mResponseHead)
            return NS_ERROR_NOT_AVAILABLE;

        mResponseHead->SetContentCharset(value);
    } else {
        // Charset hint
        mContentCharsetHint = value;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetContentLength(PRInt32 *value)
{
    NS_ENSURE_ARG_POINTER(value);

    if (!mResponseHead)
        return NS_ERROR_NOT_AVAILABLE;

    // XXX truncates to 32 bit
    LL_L2I(*value, mResponseHead->ContentLength());
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::SetContentLength(PRInt32 value)
{
    NS_NOTYETIMPLEMENTED("nsHttpChannel::SetContentLength");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHttpChannel::Open(nsIInputStream **_retval)
{
    return NS_ImplementChannelOpen(this, _retval);
}

NS_IMETHODIMP
nsHttpChannel::AsyncOpen(nsIStreamListener *listener, nsISupports *context)
{
    LOG(("nsHttpChannel::AsyncOpen [this=%x]\n", this));

    NS_ENSURE_ARG_POINTER(listener);
    NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);

    nsresult rv;

    // we want to grab a reference to the calling thread's event queue at
    // this point.  we will proxy all events back to the current thread via
    // this event queue.
    if (!mEventQ) {
        rv = gHttpHandler->GetCurrentEventQ(getter_AddRefs(mEventQ));
        if (NS_FAILED(rv)) return rv;
    }

    PRInt32 port;
    rv = mURI->GetPort(&port);
    if (NS_FAILED(rv))
        return rv;
 
    nsCOMPtr<nsIIOService> ioService;
    rv = gHttpHandler->GetIOService(getter_AddRefs(ioService));
    if (NS_FAILED(rv)) return rv;

    rv = NS_CheckPortSafety(port, "http", ioService); // this works for https
    if (NS_FAILED(rv))
        return rv;

    // fetch cookies, and add them to the request header
    AddCookiesToRequest();

    // notify "http-on-modify-request" observers
    gHttpHandler->OnModifyRequest(this);
    
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
nsHttpChannel::GetRequestMethod(nsACString &method)
{
    method = mRequestHead.Method();
    return NS_OK;
}
NS_IMETHODIMP
nsHttpChannel::SetRequestMethod(const nsACString &method)
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

NS_IMETHODIMP
nsHttpChannel::SetReferrer(nsIURI *referrer)
{
    NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);

    // clear existing referrer, if any
    mReferrer = nsnull;
    mRequestHead.ClearHeader(nsHttp::Referer);

    if (!referrer)
        return NS_OK;

    // check referrer blocking pref
    PRUint32 referrerLevel;
    if (mLoadFlags & LOAD_INITIAL_DOCUMENT_URI)
        referrerLevel = 1; // user action
    else
        referrerLevel = 2; // inline content
    if (gHttpHandler->ReferrerLevel() < referrerLevel)
        return NS_OK;

    nsCOMPtr<nsIURI> referrerGrip;
    nsresult rv;
    PRBool match;

    //
    // Strip off "wyciwyg://123/" from wyciwyg referrers.
    //
    // XXX this really belongs elsewhere since wyciwyg URLs aren't part of necko.
    //     perhaps some sort of generic nsINestedURI could be used.  then, if an URI
    //     fails the whitelist test, then we could check for an inner URI and try
    //     that instead.  though, that might be too automatic.
    // 
    rv = referrer->SchemeIs("wyciwyg", &match);
    if (NS_FAILED(rv)) return rv;
    if (match) {
        nsCAutoString path;
        rv = referrer->GetPath(path);
        if (NS_FAILED(rv)) return rv;

        PRUint32 pathLength = path.Length();
        if (pathLength <= 2) return NS_ERROR_FAILURE;

        // Path is of the form "//123/http://foo/bar", with a variable number of digits.
        // To figure out where the "real" URL starts, search path for a '/', starting at 
        // the third character.
        PRInt32 slashIndex = path.FindChar('/', 2);
        if (slashIndex == kNotFound) return NS_ERROR_FAILURE;

        // Get the charset of the original URI so we can pass it to our fixed up URI.
        nsCAutoString charset;
        referrer->GetOriginCharset(charset);

        // Replace |referrer| with a URI without wyciwyg://123/.
        rv = NS_NewURI(getter_AddRefs(referrerGrip),
                       Substring(path, slashIndex + 1, pathLength - slashIndex - 1),
                       charset.get());
        if (NS_FAILED(rv)) return rv;

        referrer = referrerGrip.get();
    }

    //
    // block referrer if not on our white list...
    //
    static const char *const referrerWhiteList[] = {
        "http",
        "https",
        "ftp",
        "gopher",
        nsnull
    };
    match = PR_FALSE;
    const char *const *scheme = referrerWhiteList;
    for (; *scheme && !match; ++scheme) {
        rv = referrer->SchemeIs(*scheme, &match);
        if (NS_FAILED(rv)) return rv;
    }
    if (!match)
        return NS_OK; // kick out....

    //
    // Handle secure referrals.
    //
    // Support referrals from a secure server if this is a secure site
    // and (optionally) if the host names are the same.
    //
    rv = referrer->SchemeIs("https", &match);
    if (NS_FAILED(rv)) return rv;
    if (match) {
        rv = mURI->SchemeIs("https", &match);
        if (NS_FAILED(rv)) return rv;
        if (!match)
            return NS_OK;

        if (!gHttpHandler->SendSecureXSiteReferrer()) {
            nsCAutoString referrerHost;
            nsCAutoString host;

            rv = referrer->GetAsciiHost(referrerHost);
            if (NS_FAILED(rv)) return rv;

            rv = mURI->GetAsciiHost(host);
            if (NS_FAILED(rv)) return rv;

            // GetAsciiHost returns lowercase hostname.
            if (!referrerHost.Equals(host))
                return NS_OK;
        }
    }

    nsCOMPtr<nsIURI> clone;
    //
    // we need to clone the referrer, so we can:
    //  (1) modify it
    //  (2) keep a reference to it after returning from this function
    //
    rv = referrer->Clone(getter_AddRefs(clone));
    if (NS_FAILED(rv)) return rv;

    // strip away any userpass; we don't want to be giving out passwords ;-)
    clone->SetUserPass(EmptyCString());

    // strip away any fragment per RFC 2616 section 14.36
    nsCOMPtr<nsIURL> url = do_QueryInterface(clone);
    if (url)
        url->SetRef(EmptyCString());

    nsCAutoString spec;
    rv = clone->GetAsciiSpec(spec);
    if (NS_FAILED(rv)) return rv;

    // finally, remember the referrer URI and set the Referer header.
    mReferrer = clone;
    mRequestHead.SetHeader(nsHttp::Referer, spec);
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetRequestHeader(const nsACString &header, nsACString &value)
{
    // XXX might be better to search the header list directly instead of
    // hitting the http atom hash table.

    nsHttpAtom atom = nsHttp::ResolveAtom(header);
    if (!atom)
        return NS_ERROR_NOT_AVAILABLE;

    return mRequestHead.GetHeader(atom, value);
}

NS_IMETHODIMP
nsHttpChannel::SetRequestHeader(const nsACString &header,
                                const nsACString &value,
                                PRBool merge)
{
    NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);

    LOG(("nsHttpChannel::SetRequestHeader [this=%x header=\"%s\" value=\"%s\" merge=%u]\n",
        this, PromiseFlatCString(header).get(), PromiseFlatCString(value).get(), merge));

    nsHttpAtom atom = nsHttp::ResolveAtom(header);
    if (!atom) {
        NS_WARNING("failed to resolve atom");
        return NS_ERROR_NOT_AVAILABLE;
    }

    return mRequestHead.SetHeader(atom, value, merge);
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
nsHttpChannel::SetUploadStream(nsIInputStream *stream, const nsACString &contentType, PRInt32 contentLength)
{
    // NOTE: for backwards compatibility and for compatibility with old style
    // plugins, |stream| may include headers, specifically Content-Type and
    // Content-Length headers.  in this case, |contentType| and |contentLength|
    // would be unspecified.  this is traditionally the case of a POST request,
    // and so we select POST as the request method if contentType and
    // contentLength are unspecified.
    
    if (stream) {
        if (!contentType.IsEmpty()) {
            if (contentLength < 0) {
                stream->Available((PRUint32 *) &contentLength);
                if (contentLength < 0) {
                    NS_ERROR("unable to determine content length");
                    return NS_ERROR_FAILURE;
                }
            }
            mRequestHead.SetHeader(nsHttp::Content_Length, nsPrintfCString("%d", contentLength));
            mRequestHead.SetHeader(nsHttp::Content_Type, contentType);
            mUploadStreamHasHeaders = PR_FALSE;
            mRequestHead.SetMethod(nsHttp::Put); // PUT request
        }
        else {
            mUploadStreamHasHeaders = PR_TRUE;
            mRequestHead.SetMethod(nsHttp::Post); // POST request
        }
    }
    else {
        mUploadStreamHasHeaders = PR_FALSE;
        mRequestHead.SetMethod(nsHttp::Get); // revert to GET request
    }
    mUploadStream = stream;    
    return NS_OK;
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
nsHttpChannel::GetResponseStatusText(nsACString &value)
{
    if (!mResponseHead)
        return NS_ERROR_NOT_AVAILABLE;
    value = mResponseHead->StatusText();
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetRequestSucceeded(PRBool *value)
{
    NS_PRECONDITION(value, "Don't ever pass a null arg to this function");
    if (!mResponseHead)
        return NS_ERROR_NOT_AVAILABLE;
    PRUint32 status = mResponseHead->Status();
    *value = (status / 100 == 2);
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetResponseHeader(const nsACString &header, nsACString &value)
{
    if (!mResponseHead)
        return NS_ERROR_NOT_AVAILABLE;
    nsHttpAtom atom = nsHttp::ResolveAtom(header);
    if (!atom)
        return NS_ERROR_NOT_AVAILABLE;
    return mResponseHead->GetHeader(atom, value);
}

NS_IMETHODIMP
nsHttpChannel::SetResponseHeader(const nsACString &header,
                                 const nsACString &value,
                                 PRBool merge)
{
    LOG(("nsHttpChannel::SetResponseHeader [this=%x header=\"%s\" value=\"%s\" merge=%u]\n",
        this, PromiseFlatCString(header).get(), PromiseFlatCString(value).get(), merge));

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

    mResponseHeadersModified = PR_TRUE;

    return mResponseHead->SetHeader(atom, value, merge);
}

NS_IMETHODIMP
nsHttpChannel::VisitResponseHeaders(nsIHttpHeaderVisitor *visitor)
{
    if (!mResponseHead)
        return NS_ERROR_NOT_AVAILABLE;
    return mResponseHead->Headers().VisitHeaders(visitor);
}

NS_IMETHODIMP
nsHttpChannel::IsNoStoreResponse(PRBool *value)
{
    if (!mResponseHead)
        return NS_ERROR_NOT_AVAILABLE;
    *value = mResponseHead->NoStore();
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::IsNoCacheResponse(PRBool *value)
{
    if (!mResponseHead)
        return NS_ERROR_NOT_AVAILABLE;
    *value = mResponseHead->NoCache();
    if (!*value)
        *value = mResponseHead->ExpiresInPast();
    return NS_OK;
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

NS_IMETHODIMP
nsHttpChannel::GetAllowPipelining(PRBool *value)
{
    NS_ENSURE_ARG_POINTER(value);
    *value = mAllowPipelining;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::SetAllowPipelining(PRBool value)
{
    if (mIsPending)
        return NS_ERROR_FAILURE;
    mAllowPipelining = value;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetRedirectionLimit(PRUint32 *value)
{
    NS_ENSURE_ARG_POINTER(value);
    *value = PRUint32(mRedirectionLimit);
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::SetRedirectionLimit(PRUint32 value)
{
    mRedirectionLimit = PR_MIN(value, 0xff);
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetContentEncodings(nsIUTF8StringEnumerator** aEncodings)
{
    NS_PRECONDITION(aEncodings, "Null out param");
    const char *encoding = mResponseHead->PeekHeader(nsHttp::Content_Encoding);
    if (!encoding) {
        *aEncodings = nsnull;
        return NS_OK;
    }
    nsContentEncodings* enumerator = new nsContentEncodings(this, encoding);
    if (!enumerator)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aEncodings = enumerator);
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsIHttpChannelInternal
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::GetDocumentURI(nsIURI **aDocumentURI)
{
    NS_ENSURE_ARG_POINTER(aDocumentURI);
    *aDocumentURI = mDocumentURI;
    NS_IF_ADDREF(*aDocumentURI);
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::SetDocumentURI(nsIURI *aDocumentURI)
{
    mDocumentURI = aDocumentURI;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetRequestVersion(PRUint32 *major, PRUint32 *minor)
{
  int version = mRequestHead.Version();

  if (major) { *major = version / 10; }
  if (minor) { *minor = version % 10; }

  return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetResponseVersion(PRUint32 *major, PRUint32 *minor)
{
  if (!mResponseHead)
  {
    *major = *minor = 0;                   // we should at least be kind about it
    return NS_ERROR_NOT_AVAILABLE;
  }

  int version = mResponseHead->Version();

  if (major) { *major = version / 10; }
  if (minor) { *minor = version % 10; }

  return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::SetCookie(const char *aCookieHeader)
{
    // empty header isn't an error
    if (!(aCookieHeader && *aCookieHeader))
        return NS_OK;

    nsICookieService *cs = gHttpHandler->GetCookieService();
    NS_ENSURE_TRUE(cs, NS_ERROR_FAILURE);

    nsCOMPtr<nsIPrompt> prompt;
    GetCallback(NS_GET_IID(nsIPrompt), getter_AddRefs(prompt));

    return cs->SetCookieStringFromHttp(mURI,
                                       mDocumentURI ? mDocumentURI : mOriginalURI,
                                       prompt,
                                       aCookieHeader,
                                       mResponseHead->PeekHeader(nsHttp::Date),
                                       this);
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsIRequestObserver
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::OnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
    if (!(mCanceled || NS_FAILED(mStatus))) {
        // capture the request's status, so our consumers will know ASAP of any
        // connection failures, etc - bug 93581
        request->GetStatus(&mStatus);
    }

    LOG(("nsHttpChannel::OnStartRequest [this=%x request=%x status=%x]\n",
        this, request, mStatus));

    // don't enter this block if we're reading from the cache...
    if (NS_SUCCEEDED(mStatus) && !mCachePump && mTransaction) {
        // grab the security info from the connection object; the transaction
        // is guaranteed to own a reference to the connection.
        mSecurityInfo = mTransaction->SecurityInfo();

        NS_ASSERTION(mResponseHead == nsnull, "leaking mResponseHead");

        // all of the response headers have been acquired, so we can take ownership
        // of them from the transaction.
        mResponseHead = mTransaction->TakeResponseHead();
        // the response head may be null if the transaction was cancelled.  in
        // which case we just need to call OnStartRequest/OnStopRequest.
        if (mResponseHead)
            return ProcessResponse();
    }

    // avoid crashing if mListener happens to be null...
    if (!mListener) {
        NS_NOTREACHED("mListener is null");
        return NS_OK;
    }

    // on proxy errors, try to failover
    if (mConnectionInfo->ProxyInfo() &&
           (mStatus == NS_ERROR_PROXY_CONNECTION_REFUSED ||
            mStatus == NS_ERROR_UNKNOWN_PROXY_HOST ||
            mStatus == NS_ERROR_NET_TIMEOUT)) {
        if (NS_SUCCEEDED(ProxyFailover()))
            return NS_OK;
    }

    return CallOnStartRequest();
}

NS_IMETHODIMP
nsHttpChannel::OnStopRequest(nsIRequest *request, nsISupports *ctxt, nsresult status)
{
    LOG(("nsHttpChannel::OnStopRequest [this=%x request=%x status=%x]\n",
        this, request, status));

    // honor the cancelation status even if the underlying transaction completed.
    if (mCanceled || NS_FAILED(mStatus))
        status = mStatus;

    if (mCachedContentIsPartial && NS_SUCCEEDED(status)) {
        // mTransactionPump should be suspended
        NS_ASSERTION(request != mTransactionPump,
            "byte-range transaction finished prematurely");

        if (request == mCachePump) {
            PRBool streamDone;
            status = OnDoneReadingPartialCacheEntry(&streamDone);
            if (NS_SUCCEEDED(status) && !streamDone)
                return status;
            // otherwise, fall through and fire OnStopRequest...
        }
        else
            NS_NOTREACHED("unexpected request");
    }

    PRBool isPartial = PR_FALSE;
    if (mTransaction) {
        // find out if the transaction ran to completion...
        if (mCacheEntry)
            isPartial = !mTransaction->ResponseIsComplete();

        // grab reference to connection in case we need to retry an 
        // authentication request over it.
        nsRefPtr<nsAHttpConnection> conn = mTransaction->Connection();

        // at this point, we're done with the transaction
        NS_RELEASE(mTransaction);
        mTransaction = nsnull;
        mTransactionPump = 0;

        // handle auth retry...
        if (mAuthRetryPending && NS_SUCCEEDED(status)) {
            mAuthRetryPending = PR_FALSE;
            status = DoAuthRetry(conn);
            if (NS_SUCCEEDED(status))
                return NS_OK;
        }

        // if this transaction has been replaced, then bail.
        if (mTransactionReplaced)
            return NS_OK;
    }

    mIsPending = PR_FALSE;
    mStatus = status;

    // perform any final cache operations before we close the cache entry.
    if (mCacheEntry && (mCacheAccess & nsICache::ACCESS_WRITE))
        FinalizeCacheEntry();
    
    if (mListener) {
        LOG(("  calling OnStopRequest\n"));
        mListener->OnStopRequest(this, mListenerContext, status);
        mListener = 0;
        mListenerContext = 0;
    }

    if (mCacheEntry) {
        nsresult closeStatus = status;
        if (mCanceled) {
            // we don't want to discard the cache entry if canceled and
            // reading from the cache.
            if (request == mCachePump)
                closeStatus = NS_OK;
            // we also don't want to discard the cache entry if the
            // server supports byte range requests, because we could always
            // complete the download at a later time.
            else if (isPartial && mResponseHead && mResponseHead->IsResumable()) {
                LOG(("keeping partial response that is resumable!\n"));
                closeStatus = NS_OK; 
            }
        }
        CloseCacheEntry(closeStatus);
    }

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

    // don't send out OnDataAvailable notifications if we've been canceled.
    if (mCanceled)
        return mStatus;

    NS_ASSERTION(!(mCachedContentIsPartial && (request == mTransactionPump)),
            "transaction pump not suspended");

    if (mAuthRetryPending || (request == mTransactionPump && mTransactionReplaced)) {
        PRUint32 n;
        return input->ReadSegments(DiscardSegments, nsnull, count, &n);
    }

    if (mListener) {
        //
        // synthesize transport progress event.  we do this here since we want
        // to delay OnProgress events until we start streaming data.  this is
        // crucially important since it impacts the lock icon (see bug 240053).
        //
        nsresult transportStatus;
        if (request == mCachePump)
            transportStatus = nsITransport::STATUS_READING;
        else
            transportStatus = nsISocketTransport::STATUS_RECEIVING_FROM;

        // mResponseHead may reference new or cached headers, but either way it
        // holds our best estimate of the total content length.  Even in the case
        // of a byte range request, the content length stored in the cached
        // response headers is what we want to use here.

        nsUint64 progressMax(PRUint64(mResponseHead->ContentLength()));
        nsUint64 progress = mLogicalOffset + nsUint64(count);
        NS_ASSERTION(progress <= progressMax, "unexpected progress values");

        OnTransportStatus(nsnull, transportStatus, progress, progressMax);

        //
        // we have to manually keep the logical offset of the stream up-to-date.
        // we cannot depend soley on the offset provided, since we may have 
        // already streamed some data from another source (see, for example,
        // OnDoneReadingPartialCacheEntry).
        //
        nsresult rv =  mListener->OnDataAvailable(this,
                                                  mListenerContext,
                                                  input,
                                                  mLogicalOffset,
                                                  count);
        if (NS_SUCCEEDED(rv))
            mLogicalOffset = progress;
        return rv;
    }

    return NS_ERROR_ABORT;
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsITransportEventSink
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::OnTransportStatus(nsITransport *trans, nsresult status,
                                 PRUint32 progress, PRUint32 progressMax)
{
    // block socket status event after Cancel or OnStopRequest has been called.
    if (mProgressSink && NS_SUCCEEDED(mStatus) && mIsPending && !(mLoadFlags & LOAD_BACKGROUND)) {
        LOG(("sending status notification [this=%x status=%x progress=%u/%u]\n",
            this, status, progress, progressMax));

        NS_ConvertASCIItoUCS2 host(mConnectionInfo->Host());
        mProgressSink->OnStatus(this, nsnull, status, host.get());

        if (progress > 0)
            mProgressSink->OnProgress(this, nsnull, progress, progressMax);
    }
#ifdef DEBUG
    else
        LOG(("skipping status notification [this=%x sink=%x pending=%u background=%x]\n",
            this, mProgressSink.get(), mIsPending, (mLoadFlags & LOAD_BACKGROUND)));
#endif

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
nsHttpChannel::SetCacheKey(nsISupports *key)
{
    nsresult rv;

    LOG(("nsHttpChannel::SetCacheKey [this=%x key=%x]\n", this, key));

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

NS_IMETHODIMP
nsHttpChannel::IsFromCache(PRBool *value)
{
    if (!mIsPending)
        return NS_ERROR_NOT_AVAILABLE;

    // return false if reading a partial cache entry; the data isn't entirely
    // from the cache!

    *value = (mCachePump || (mLoadFlags & LOAD_ONLY_IF_MODIFIED)) &&
              mCachedContentIsValid && !mCachedContentIsPartial;

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsIResumableChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::ResumeAt(PRUint64 aStartPos,
                        nsIResumableEntityID* aEntityID)
{
    mEntityID = aEntityID;
    mStartPos = aStartPos;
    mResuming = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetEntityID(nsIResumableEntityID** aEntityID)
{
    // Don't return an entity ID for HTTP/1.0 servers
    if (mResponseHead && (mResponseHead->Version() < NS_HTTP_VERSION_1_1)) {
        *aEntityID = nsnull;
        return NS_OK;
    }
    // Neither return one for Non-GET requests which require additional data
    if (mRequestHead.Method() != nsHttp::Get) {
        *aEntityID = nsnull;
        return NS_OK;
    }

    PRUint64 size = LL_MaxUint();
    nsCAutoString etag, lastmod;
    if (mResponseHead) {
        size = mResponseHead->TotalEntitySize();
        const char* cLastMod = mResponseHead->PeekHeader(nsHttp::Last_Modified);
        if (cLastMod)
            lastmod = cLastMod;
        const char* cEtag = mResponseHead->PeekHeader(nsHttp::ETag);
        if (cEtag)
            etag = cEtag;
    }

    return NS_NewResumableEntityID(aEntityID, size, lastmod, etag);
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

    if (mCanceled && NS_FAILED(mStatus)) {
        LOG(("channel was canceled [this=%x status=%x]\n", this, mStatus));
        rv = mStatus;
    }
    else if ((mLoadFlags & LOAD_ONLY_FROM_CACHE) && NS_FAILED(status))
        // if this channel is only allowed to pull from the cache, then
        // we must fail if we were unable to open a cache entry.
        rv = NS_ERROR_DOCUMENT_NOT_CACHED;
    else
        // advance to the next state...
        rv = Connect(PR_FALSE);

    // a failure from Connect means that we have to abort the channel.
    if (NS_FAILED(rv)) {
        CloseCacheEntry(rv);
        AsyncAbort(rv);
    }

    return NS_OK;
}

void
nsHttpChannel::ClearPasswordManagerEntry(const char      *scheme,
                                         const char      *host,
                                         PRInt32          port,
                                         const char      *realm,
                                         const PRUnichar *user)
{
    // XXX scheme is currently unused.  see comments in PromptForIdentity

    nsresult rv;
    nsCOMPtr<nsIPasswordManager> passWordManager = do_GetService(NS_PASSWORDMANAGER_CONTRACTID, &rv);
    if (passWordManager) {
        nsCAutoString domain;
        domain.Assign(host);
        domain.Append(':');
        domain.AppendInt(port);

        domain.Append(" (");
        domain.Append(realm);
        domain.Append(')');

        passWordManager->RemoveUser(domain, nsDependentString(user));
    }
} 

nsresult
nsHttpChannel::DoAuthRetry(nsAHttpConnection *conn)
{
    LOG(("nsHttpChannel::DoAuthRetry [this=%x]\n", this));

    NS_ASSERTION(!mTransaction, "should not have a transaction");
    nsresult rv;

    // toggle mIsPending to allow nsIObserver implementations to modify
    // the request headers (bug 95044).
    mIsPending = PR_FALSE;

    // fetch cookies, and add them to the request header.
    // the server response could have included cookies that must be sent with
    // this authentication attempt (bug 84794).
    AddCookiesToRequest();

    // notify "http-on-modify-request" observers
    gHttpHandler->OnModifyRequest(this);

    mIsPending = PR_TRUE;

    // get rid of the old response headers
    delete mResponseHead;
    mResponseHead = nsnull;

    // set sticky connection flag and disable pipelining.
    mCaps |=  NS_HTTP_STICKY_CONNECTION;
    mCaps &= ~NS_HTTP_ALLOW_PIPELINING;
   
    // and create a new one...
    rv = SetupTransaction();
    if (NS_FAILED(rv)) return rv;

    // transfer ownership of connection to transaction
    if (conn)
        mTransaction->SetConnection(conn);

    // rewind the upload stream
    if (mUploadStream) {
        nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(mUploadStream);
        if (seekable)
            seekable->Seek(nsISeekableStream::NS_SEEK_SET, 0);
    }

    rv = gHttpHandler->InitiateTransaction(mTransaction);
    if (NS_FAILED(rv)) return rv;

    return mTransactionPump->AsyncRead(this, nsnull);
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsContentEncodings <public>
//-----------------------------------------------------------------------------

nsHttpChannel::nsContentEncodings::nsContentEncodings(nsIHttpChannel* aChannel,
                                                          const char* aEncodingHeader) :
    mEncodingHeader(aEncodingHeader), mChannel(aChannel), mReady(PR_FALSE)
{
    mCurEnd = aEncodingHeader + strlen(aEncodingHeader);
    mCurStart = mCurEnd;
}
    
nsHttpChannel::nsContentEncodings::~nsContentEncodings()
{
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsContentEncodings::nsISimpleEnumerator
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::nsContentEncodings::HasMore(PRBool* aMoreEncodings)
{
    if (mReady) {
        *aMoreEncodings = PR_TRUE;
        return NS_OK;
    }
    
    nsresult rv = PrepareForNext();
    *aMoreEncodings = NS_SUCCEEDED(rv);
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::nsContentEncodings::GetNext(nsACString& aNextEncoding)
{
    aNextEncoding.Truncate();
    if (!mReady) {
        nsresult rv = PrepareForNext();
        if (NS_FAILED(rv)) {
            return NS_ERROR_FAILURE;
        }
    }

    const nsACString & encoding = Substring(mCurStart, mCurEnd);

    nsACString::const_iterator start, end;
    encoding.BeginReading(start);
    encoding.EndReading(end);

    PRBool haveType = PR_FALSE;
    if (CaseInsensitiveFindInReadable(NS_LITERAL_CSTRING("gzip"),
                                      start,
                                      end)) {
        aNextEncoding.AssignLiteral(APPLICATION_GZIP);
        haveType = PR_TRUE;
    }

    if (!haveType) {
        encoding.BeginReading(start);
        if (CaseInsensitiveFindInReadable(NS_LITERAL_CSTRING("compress"),
                                          start,
                                          end)) {
            aNextEncoding.AssignLiteral(APPLICATION_COMPRESS);
                                           
            haveType = PR_TRUE;
        }
    }
    
    if (! haveType) {
        encoding.BeginReading(start);
        if (CaseInsensitiveFindInReadable(NS_LITERAL_CSTRING("deflate"),
                                          start,
                                          end)) {
            aNextEncoding.AssignLiteral(APPLICATION_ZIP);
            haveType = PR_TRUE;
        }
    }

    // Prepare to fetch the next encoding
    mCurEnd = mCurStart;
    mReady = PR_FALSE;
    
    if (haveType)
        return NS_OK;

    NS_WARNING("Unknown encoding type");
    return NS_ERROR_FAILURE;
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsContentEncodings::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS1(nsHttpChannel::nsContentEncodings, nsIUTF8StringEnumerator)

//-----------------------------------------------------------------------------
// nsHttpChannel::nsContentEncodings <private>
//-----------------------------------------------------------------------------

nsresult
nsHttpChannel::nsContentEncodings::PrepareForNext(void)
{
    NS_PRECONDITION(mCurStart == mCurEnd, "Indeterminate state");
    
    // At this point both mCurStart and mCurEnd point to somewhere
    // past the end of the next thing we want to return
    
    while (mCurEnd != mEncodingHeader) {
        --mCurEnd;
        if (*mCurEnd != ',' && !nsCRT::IsAsciiSpace(*mCurEnd))
            break;
    }
    if (mCurEnd == mEncodingHeader)
        return NS_ERROR_NOT_AVAILABLE; // no more encodings
    ++mCurEnd;
        
    // At this point mCurEnd points to the first char _after_ the
    // header we want.  Furthermore, mCurEnd - 1 != mEncodingHeader
    
    mCurStart = mCurEnd - 1;
    while (mCurStart != mEncodingHeader &&
           *mCurStart != ',' && !nsCRT::IsAsciiSpace(*mCurStart))
        --mCurStart;
    if (*mCurStart == ',' || nsCRT::IsAsciiSpace(*mCurStart))
        ++mCurStart; // we stopped because of a weird char, so move up one
        
    // At this point mCurStart and mCurEnd bracket the encoding string
    // we want.  Check that it's not "identity"
    if (Substring(mCurStart, mCurEnd).Equals("identity",
                                             nsCaseInsensitiveCStringComparator())) {
        mCurEnd = mCurStart;
        return PrepareForNext();
    }
        
    mReady = PR_TRUE;
    return NS_OK;
}
