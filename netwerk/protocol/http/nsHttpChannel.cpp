/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set expandtab ts=4 sw=4 sts=4 cin: */
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
 *   Google Inc.
 *   Jan Wrobel <wrobel@blues.ath.cx>
 *   Jan Odvarko <odvarko@gmail.com>
 *   Dave Camp <dcamp@mozilla.com>
 *   Honza Bambas <honzab@firemni.cz>
 *   Daniel Witte <dwitte@mozilla.com>
 *   Jason Duell <jduell.mcbugs@gmail.com>
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

#include "base/basictypes.h"

#include "nsHttpChannel.h"
#include "nsHttpHandler.h"
#include "nsIApplicationCacheService.h"
#include "nsIApplicationCacheContainer.h"
#include "nsIAuthInformation.h"
#include "nsIStringBundle.h"
#include "nsIIDNService.h"
#include "nsIStreamListenerTee.h"
#include "nsISeekableStream.h"
#include "nsMimeTypes.h"
#include "nsPrintfCString.h"
#include "nsNetUtil.h"
#include "prprf.h"
#include "prnetdb.h"
#include "nsEscape.h"
#include "nsStreamUtils.h"
#include "nsIOService.h"
#include "nsICacheService.h"
#include "nsDNSPrefetch.h"
#include "nsChannelClassifier.h"
#include "nsIRedirectResultListener.h"

// True if the local cache should be bypassed when processing a request.
#define BYPASS_LOCAL_CACHE(loadFlags) \
        (loadFlags & (nsIRequest::LOAD_BYPASS_CACHE | \
                      nsICachingChannel::LOAD_BYPASS_LOCAL_CACHE))

static NS_DEFINE_CID(kStreamListenerTeeCID, NS_STREAMLISTENERTEE_CID);

class AutoRedirectVetoNotifier
{
public:
    AutoRedirectVetoNotifier(nsHttpChannel* channel) : mChannel(channel) {}
    ~AutoRedirectVetoNotifier() {ReportRedirectResult(false);}
    void RedirectSucceeded() {ReportRedirectResult(true);}

private:
    nsHttpChannel* mChannel;
    void ReportRedirectResult(bool succeeded);
};

void
AutoRedirectVetoNotifier::ReportRedirectResult(bool succeeded)
{
    if (!mChannel)
        return;

    mChannel->mRedirectChannel = nsnull;

    nsCOMPtr<nsIRedirectResultListener> vetoHook;
    NS_QueryNotificationCallbacks(mChannel, 
                                  NS_GET_IID(nsIRedirectResultListener), 
                                  getter_AddRefs(vetoHook));
    mChannel = nsnull;
    if (vetoHook)
        vetoHook->OnRedirectResult(succeeded);
}

//-----------------------------------------------------------------------------
// nsHttpChannel <public>
//-----------------------------------------------------------------------------

nsHttpChannel::nsHttpChannel()
    : mLogicalOffset(0)
    , mCacheAccess(0)
    , mPostID(0)
    , mRequestTime(0)
    , mOnCacheEntryAvailableCallback(nsnull)
    , mAsyncCacheOpen(PR_FALSE)
    , mPendingAsyncCallOnResume(nsnull)
    , mSuspendCount(0)
    , mCachedContentIsValid(PR_FALSE)
    , mCachedContentIsPartial(PR_FALSE)
    , mTransactionReplaced(PR_FALSE)
    , mAuthRetryPending(PR_FALSE)
    , mResuming(PR_FALSE)
    , mInitedCacheEntry(PR_FALSE)
    , mCacheForOfflineUse(PR_FALSE)
    , mCachingOpportunistically(PR_FALSE)
    , mFallbackChannel(PR_FALSE)
    , mTracingEnabled(PR_TRUE)
    , mCustomConditionalRequest(PR_FALSE)
    , mFallingBack(PR_FALSE)
    , mWaitingForRedirectCallback(PR_FALSE)
    , mRequestTimeInitialized(PR_FALSE)
{
    LOG(("Creating nsHttpChannel [this=%p]\n", this));
}

nsHttpChannel::~nsHttpChannel()
{
    LOG(("Destroying nsHttpChannel [this=%p]\n", this));

    if (mAuthProvider) 
        mAuthProvider->Disconnect(NS_ERROR_ABORT);
}

nsresult
nsHttpChannel::Init(nsIURI *uri,
                    PRUint8 caps,
                    nsProxyInfo *proxyInfo)
{
    nsresult rv = HttpBaseChannel::Init(uri, caps, proxyInfo);
    if (NS_FAILED(rv)) 
        return rv;

    LOG(("nsHttpChannel::Init [this=%p]\n", this));

    mAuthProvider =
        do_CreateInstance("@mozilla.org/network/http-channel-auth-provider;1",
                          &rv);
    if (NS_FAILED(rv)) 
        return rv;
    rv = mAuthProvider->Init(this);

    return rv;
}
//-----------------------------------------------------------------------------
// nsHttpChannel <private>
//-----------------------------------------------------------------------------

nsresult
nsHttpChannel::AsyncCall(nsAsyncCallback funcPtr,
                         nsRunnableMethod<nsHttpChannel> **retval)
{
    nsresult rv;

    nsRefPtr<nsRunnableMethod<nsHttpChannel> > event =
        NS_NewRunnableMethod(this, funcPtr);
    rv = NS_DispatchToCurrentThread(event);
    if (NS_SUCCEEDED(rv) && retval) {
        *retval = event;
    }

    return rv;
}

nsresult
nsHttpChannel::Connect(PRBool firstTime)
{
    nsresult rv;

    LOG(("nsHttpChannel::Connect [this=%p]\n", this));

    // Even if we're in private browsing mode, we still enforce existing STS
    // data (it is read-only).
    // if the connection is not using SSL and either the exact host matches or
    // a superdomain wants to force HTTPS, do it.
    PRBool usingSSL = PR_FALSE;
    rv = mURI->SchemeIs("https", &usingSSL);
    NS_ENSURE_SUCCESS(rv,rv);

    if (!usingSSL) {
        // enforce Strict-Transport-Security
        nsIStrictTransportSecurityService* stss = gHttpHandler->GetSTSService();
        NS_ENSURE_TRUE(stss, NS_ERROR_OUT_OF_MEMORY);

        PRBool isStsHost = PR_FALSE;
        rv = stss->IsStsURI(mURI, &isStsHost);

        // if STS fails, there's no reason to cancel the load, but it's
        // worrisome.
        NS_ASSERTION(NS_SUCCEEDED(rv),
                     "Something is wrong with STS: IsStsURI failed.");

        if (NS_SUCCEEDED(rv) && isStsHost) {
            LOG(("nsHttpChannel::Connect() STS permissions found\n"));
            return AsyncCall(&nsHttpChannel::HandleAsyncRedirectChannelToHttps);
        }
    }

    // ensure that we are using a valid hostname
    if (!net_IsValidHostName(nsDependentCString(mConnectionInfo->Host())))
        return NS_ERROR_UNKNOWN_HOST;

    // true when called from AsyncOpen
    if (firstTime) {
        // are we offline?
        PRBool offline = gIOService->IsOffline();
        if (offline)
            mLoadFlags |= LOAD_ONLY_FROM_CACHE;
        else if (PL_strcmp(mConnectionInfo->ProxyType(), "unknown") == 0)
            return ResolveProxy();  // Lazily resolve proxy info

        // Don't allow resuming when cache must be used
        if (mResuming && (mLoadFlags & LOAD_ONLY_FROM_CACHE)) {
            LOG(("Resuming from cache is not supported yet"));
            return NS_ERROR_DOCUMENT_NOT_CACHED;
        }

        // open a cache entry for this channel...
        rv = OpenCacheEntry();

        if (NS_FAILED(rv)) {
            LOG(("OpenCacheEntry failed [rv=%x]\n", rv));
            // if this channel is only allowed to pull from the cache, then
            // we must fail if we were unable to open a cache entry.
            if (mLoadFlags & LOAD_ONLY_FROM_CACHE) {
                // If we have a fallback URI (and we're not already
                // falling back), process the fallback asynchronously.
                if (!mFallbackChannel && !mFallbackKey.IsEmpty()) {
                    return AsyncCall(&nsHttpChannel::HandleAsyncFallback);
                }
                return NS_ERROR_DOCUMENT_NOT_CACHED;
            }
            // otherwise, let's just proceed without using the cache.
        }

        // if cacheForOfflineUse has been set, open up an offline cache
        // entry to update
        if (mCacheForOfflineUse) {
            rv = OpenOfflineCacheEntryForWriting();
            if (NS_FAILED(rv)) return rv;
        }

        if (NS_SUCCEEDED(rv) && mAsyncCacheOpen)
            return NS_OK;
    }

    // we may or may not have a cache entry at this point
    if (mCacheEntry) {
        // inspect the cache entry to determine whether or not we need to go
        // out to net to validate it.  this call sets mCachedContentIsValid
        // and may set request headers as required for cache validation.
        rv = CheckCache();
        if (NS_FAILED(rv))
            NS_WARNING("cache check failed");

        // read straight from the cache if possible...
        if (mCachedContentIsValid) {
            nsRunnableMethod<nsHttpChannel> *event = nsnull;
            if (!mCachedContentIsPartial) {
                AsyncCall(&nsHttpChannel::AsyncOnExamineCachedResponse, &event);
            }
            rv = ReadFromCache();
            if (NS_FAILED(rv) && event) {
                event->Revoke();
            }
            return rv;
        }
        else if (mLoadFlags & LOAD_ONLY_FROM_CACHE) {
            // the cache contains the requested resource, but it must be 
            // validated before we can reuse it.  since we are not allowed
            // to hit the net, there's nothing more to do.  the document
            // is effectively not in the cache.
            return NS_ERROR_DOCUMENT_NOT_CACHED;
        }
    }
    else if (mLoadFlags & LOAD_ONLY_FROM_CACHE) {
        // If we have a fallback URI (and we're not already
        // falling back), process the fallback asynchronously.
        if (!mFallbackChannel && !mFallbackKey.IsEmpty()) {
            return AsyncCall(&nsHttpChannel::HandleAsyncFallback);
        }
        return NS_ERROR_DOCUMENT_NOT_CACHED;
    }

    // check to see if authorization headers should be included
    mAuthProvider->AddAuthorizationHeaders();

    if (mLoadFlags & LOAD_NO_NETWORK_IO) {
        return NS_ERROR_DOCUMENT_NOT_CACHED;
    }

    // hit the net...
    rv = SetupTransaction();
    if (NS_FAILED(rv)) return rv;

    rv = gHttpHandler->InitiateTransaction(mTransaction, mPriority);
    if (NS_FAILED(rv)) return rv;

    rv = mTransactionPump->AsyncRead(this, nsnull);
    if (NS_FAILED(rv)) return rv;

    PRUint32 suspendCount = mSuspendCount;
    while (suspendCount--)
        mTransactionPump->Suspend();

    return NS_OK;
}

// called when Connect fails
nsresult
nsHttpChannel::AsyncAbort(nsresult status)
{
    LOG(("nsHttpChannel::AsyncAbort [this=%p status=%x]\n", this, status));

    mStatus = status;
    mIsPending = PR_FALSE;

    nsresult rv = AsyncCall(&nsHttpChannel::HandleAsyncNotifyListener);
    // And if that fails?  Callers ignore our return value anyway....
    
    // finally remove ourselves from the load group.
    if (mLoadGroup)
        mLoadGroup->RemoveRequest(this, nsnull, status);

    return rv;
}

void
nsHttpChannel::HandleAsyncNotifyListener()
{
    NS_PRECONDITION(!mPendingAsyncCallOnResume, "How did that happen?");
    
    if (mSuspendCount) {
        LOG(("Waiting until resume to do async notification [this=%p]\n",
             this));
        mPendingAsyncCallOnResume = &nsHttpChannel::HandleAsyncNotifyListener;
        return;
    }

    DoNotifyListener();
}

void
nsHttpChannel::DoNotifyListener()
{
    // Make sure mIsPending is set to PR_FALSE. At this moment we are done from
    // the point of view of our consumer and we have to report our self
    // as not-pending.
    if (mListener) {
        mListener->OnStartRequest(this, mListenerContext);
        mIsPending = PR_FALSE;
        mListener->OnStopRequest(this, mListenerContext, mStatus);
        mListener = 0;
        mListenerContext = 0;
    }
    else {
        mIsPending = PR_FALSE;
    }
    // We have to make sure to drop the reference to the callbacks too
    mCallbacks = nsnull;
    mProgressSink = nsnull;

    // We don't need this info anymore
    CleanRedirectCacheChainIfNecessary();
}

void
nsHttpChannel::HandleAsyncRedirect()
{
    NS_PRECONDITION(!mPendingAsyncCallOnResume, "How did that happen?");
    
    if (mSuspendCount) {
        LOG(("Waiting until resume to do async redirect [this=%p]\n", this));
        mPendingAsyncCallOnResume = &nsHttpChannel::HandleAsyncRedirect;
        return;
    }

    nsresult rv = NS_OK;

    LOG(("nsHttpChannel::HandleAsyncRedirect [this=%p]\n", this));

    // since this event is handled asynchronously, it is possible that this
    // channel could have been canceled, in which case there would be no point
    // in processing the redirect.
    if (NS_SUCCEEDED(mStatus)) {
        PushRedirectAsyncFunc(&nsHttpChannel::ContinueHandleAsyncRedirect);
        rv = AsyncProcessRedirection(mResponseHead->Status());
        if (NS_FAILED(rv)) {
            PopRedirectAsyncFunc(&nsHttpChannel::ContinueHandleAsyncRedirect);
            ContinueHandleAsyncRedirect(rv);
        }
    }
    else {
        ContinueHandleAsyncRedirect(NS_OK);
    }
}

nsresult
nsHttpChannel::ContinueHandleAsyncRedirect(nsresult rv)
{
    if (NS_FAILED(rv)) {
        // If AsyncProcessRedirection fails, then we have to send out the
        // OnStart/OnStop notifications.
        LOG(("ContinueHandleAsyncRedirect got failure result [rv=%x]\n", rv));
        mStatus = rv;
        DoNotifyListener();
    }

    // close the cache entry.  Blow it away if we couldn't process the redirect
    // for some reason (the cache entry might be corrupt).
    if (mCacheEntry) {
        if (NS_FAILED(rv))
            mCacheEntry->Doom();
        CloseCacheEntry(PR_FALSE);
    }

    mIsPending = PR_FALSE;

    if (mLoadGroup)
        mLoadGroup->RemoveRequest(this, nsnull, mStatus);

    return NS_OK;
}

void
nsHttpChannel::HandleAsyncNotModified()
{
    NS_PRECONDITION(!mPendingAsyncCallOnResume, "How did that happen?");
    
    if (mSuspendCount) {
        LOG(("Waiting until resume to do async not-modified [this=%p]\n",
             this));
        mPendingAsyncCallOnResume = &nsHttpChannel::HandleAsyncNotModified;
        return;
    }
    
    LOG(("nsHttpChannel::HandleAsyncNotModified [this=%p]\n", this));

    DoNotifyListener();

    CloseCacheEntry(PR_TRUE);

    mIsPending = PR_FALSE;

    if (mLoadGroup)
        mLoadGroup->RemoveRequest(this, nsnull, mStatus);
}

void
nsHttpChannel::HandleAsyncFallback()
{
    NS_PRECONDITION(!mPendingAsyncCallOnResume, "How did that happen?");

    if (mSuspendCount) {
        LOG(("Waiting until resume to do async fallback [this=%p]\n", this));
        mPendingAsyncCallOnResume = &nsHttpChannel::HandleAsyncFallback;
        return;
    }

    nsresult rv = NS_OK;

    LOG(("nsHttpChannel::HandleAsyncFallback [this=%p]\n", this));

    // since this event is handled asynchronously, it is possible that this
    // channel could have been canceled, in which case there would be no point
    // in processing the fallback.
    if (!mCanceled) {
        PushRedirectAsyncFunc(&nsHttpChannel::ContinueHandleAsyncFallback);
        PRBool waitingForRedirectCallback;
        rv = ProcessFallback(&waitingForRedirectCallback);
        if (waitingForRedirectCallback)
            return;
        PopRedirectAsyncFunc(&nsHttpChannel::ContinueHandleAsyncFallback);
    }

    ContinueHandleAsyncFallback(rv);
}

nsresult
nsHttpChannel::ContinueHandleAsyncFallback(nsresult rv)
{
    if (!mCanceled && (NS_FAILED(rv) || !mFallingBack)) {
        // If ProcessFallback fails, then we have to send out the
        // OnStart/OnStop notifications.
        LOG(("ProcessFallback failed [rv=%x, %d]\n", rv, mFallingBack));
        mStatus = NS_FAILED(rv) ? rv : NS_ERROR_DOCUMENT_NOT_CACHED;
        DoNotifyListener();
    }

    mIsPending = PR_FALSE;

    if (mLoadGroup)
        mLoadGroup->RemoveRequest(this, nsnull, mStatus);

    return rv;
}

nsresult
nsHttpChannel::SetupTransaction()
{
    LOG(("nsHttpChannel::SetupTransaction [this=%p]\n", this));

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
              mRequestHead.Method() == nsHttp::Head ||
              mRequestHead.Method() == nsHttp::Propfind ||
              mRequestHead.Method() == nsHttp::Proppatch)) {
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
    mRequestTimeInitialized = PR_TRUE;

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
        char byteRange[32];
        PR_snprintf(byteRange, sizeof(byteRange), "bytes=%llu-", mStartPos);
        mRequestHead.SetHeader(nsHttp::Range, nsDependentCString(byteRange));

        if (!mEntityID.IsEmpty()) {
            // Also, we want an error if this resource changed in the meantime
            // Format of the entity id is: escaped_etag/size/lastmod
            nsCString::const_iterator start, end, slash;
            mEntityID.BeginReading(start);
            mEntityID.EndReading(end);
            mEntityID.BeginReading(slash);

            if (FindCharInReadable('/', slash, end)) {
                nsCAutoString ifMatch;
                mRequestHead.SetHeader(nsHttp::If_Match,
                        NS_UnescapeURL(Substring(start, slash), 0, ifMatch));

                ++slash; // Incrementing, so that searching for '/' won't find
                         // the same slash again
            }

            if (FindCharInReadable('/', slash, end)) {
                mRequestHead.SetHeader(nsHttp::If_Unmodified_Since,
                        Substring(++slash, end));
            }
        }
    }

    // create wrapper for this channel's notification callbacks
    nsCOMPtr<nsIInterfaceRequestor> callbacks;
    NS_NewNotificationCallbacksAggregation(mCallbacks, mLoadGroup,
                                           getter_AddRefs(callbacks));
    if (!callbacks)
        return NS_ERROR_OUT_OF_MEMORY;

    // create the transaction object
    mTransaction = new nsHttpTransaction();
    if (!mTransaction)
        return NS_ERROR_OUT_OF_MEMORY;

    // See bug #466080. Transfer LOAD_ANONYMOUS flag to socket-layer.
    if (mLoadFlags & LOAD_ANONYMOUS)
        mCaps |= NS_HTTP_LOAD_ANONYMOUS;

    mConnectionInfo->SetAnonymous((mLoadFlags & LOAD_ANONYMOUS) != 0);

    nsCOMPtr<nsIAsyncInputStream> responseStream;
    rv = mTransaction->Init(mCaps, mConnectionInfo, &mRequestHead,
                            mUploadStream, mUploadStreamHasHeaders,
                            NS_GetCurrentThread(), callbacks, this,
                            getter_AddRefs(responseStream));
    if (NS_FAILED(rv)) {
        mTransaction = nsnull;
        return rv;
    }

    rv = nsInputStreamPump::Create(getter_AddRefs(mTransactionPump),
                                   responseStream);
    return rv;
}

// NOTE: This function duplicates code from nsBaseChannel. This will go away
// once HTTP uses nsBaseChannel (part of bug 312760)
static void
CallTypeSniffers(void *aClosure, const PRUint8 *aData, PRUint32 aCount)
{
  nsIChannel *chan = static_cast<nsIChannel*>(aClosure);

  const nsCOMArray<nsIContentSniffer>& sniffers =
    gIOService->GetContentSniffers();
  PRUint32 length = sniffers.Count();
  for (PRUint32 i = 0; i < length; ++i) {
    nsCAutoString newType;
    nsresult rv =
      sniffers[i]->GetMIMETypeFromContent(chan, aData, aCount, newType);
    if (NS_SUCCEEDED(rv) && !newType.IsEmpty()) {
      chan->SetContentType(newType);
      break;
    }
  }
}

nsresult
nsHttpChannel::CallOnStartRequest()
{
    mTracingEnabled = PR_FALSE;

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

    if (mResponseHead) {
        SetPropertyAsInt64(NS_CHANNEL_PROP_CONTENT_LENGTH,
                           mResponseHead->ContentLength());
        // If we have a cache entry, set its predicted size to ContentLength to
        // avoid caching an entry that will exceed the max size limit.
        if (mCacheEntry) {
            nsresult rv;
            PRInt64 predictedDataSize = -1; // -1 in case GetAsInt64 fails.
            GetPropertyAsInt64(NS_CHANNEL_PROP_CONTENT_LENGTH, 
                               &predictedDataSize);
            rv = mCacheEntry->SetPredictedDataSize(predictedDataSize);
            if (NS_FAILED(rv)) return rv;
        }
    }
    // Allow consumers to override our content type
    if ((mLoadFlags & LOAD_CALL_CONTENT_SNIFFERS) &&
        gIOService->GetContentSniffers().Count() != 0) {
        // NOTE: We can have both a txn pump and a cache pump when the cache
        // content is partial. In that case, we need to read from the cache,
        // because that's the one that has the initial contents. If that fails
        // then give the transaction pump a shot.

        nsIChannel* thisChannel = static_cast<nsIChannel*>(this);

        PRBool typeSniffersCalled = PR_FALSE;
        if (mCachePump) {
          typeSniffersCalled =
            NS_SUCCEEDED(mCachePump->PeekStream(CallTypeSniffers, thisChannel));
        }
        
        if (!typeSniffersCalled && mTransactionPump) {
          mTransactionPump->PeekStream(CallTypeSniffers, thisChannel);
        }
    }

    LOG(("  calling mListener->OnStartRequest\n"));
    nsresult rv = mListener->OnStartRequest(this, mListenerContext);
    if (NS_FAILED(rv)) return rv;

    // install stream converter if required
    rv = ApplyContentConversions();
    if (NS_FAILED(rv)) return rv;

    // if this channel is for a download, close off access to the cache.
    if (mCacheEntry && mChannelIsForDownload) {
        mCacheEntry->Doom();
        CloseCacheEntry(PR_FALSE);
    }

    if (!mCanceled) {
        // create offline cache entry if offline caching was requested
        if (mCacheForOfflineUse) {
            PRBool shouldCacheForOfflineUse;
            rv = ShouldUpdateOfflineCacheEntry(&shouldCacheForOfflineUse);
            if (NS_FAILED(rv)) return rv;
            
            if (shouldCacheForOfflineUse) {
                LOG(("writing to the offline cache"));
                rv = InitOfflineCacheEntry();
                if (NS_FAILED(rv)) return rv;
                
                if (mOfflineCacheEntry) {
                  rv = InstallOfflineCacheListener();
                  if (NS_FAILED(rv)) return rv;
                }
            } else {
                LOG(("offline cache is up to date, not updating"));
                CloseOfflineCacheEntry();
            }
        }
    }

    return NS_OK;
}

nsresult
nsHttpChannel::ProcessFailedSSLConnect(PRUint32 httpStatus)
{
    // Failure to set up SSL proxy tunnel means one of the following:
    // 1) Proxy wants authorization, or forbids.
    // 2) DNS at proxy couldn't resolve target URL.
    // 3) Proxy connection to target failed or timed out.
    // 4) Eve noticed our proxy CONNECT, and is replying with malicious HTML.
    // 
    // Our current architecture will parse response content with the
    // permission of the target URL!  Given #4, we must avoid rendering the
    // body of the reply, and instead give the user a (hopefully helpful) 
    // boilerplate error page, based on just the HTTP status of the reply.

    NS_ABORT_IF_FALSE(mConnectionInfo->UsingSSL(),
                      "SSL connect failed but not using SSL?");
    nsresult rv;
    switch (httpStatus) 
    {
    case 300: case 301: case 302: case 303: case 307:
        // Bad redirect: not top-level, or it's a POST, bad/missing Location,
        // or ProcessRedirect() failed for some other reason.  Legal
        // redirects that fail because site not available, etc., are handled
        // elsewhere, in the regular codepath.
        rv = NS_ERROR_CONNECTION_REFUSED;
        break;
    case 403: // HTTP/1.1: "Forbidden"
    case 407: // ProcessAuthentication() failed
    case 501: // HTTP/1.1: "Not Implemented"
        // user sees boilerplate Mozilla "Proxy Refused Connection" page.
        rv = NS_ERROR_PROXY_CONNECTION_REFUSED; 
        break;
    // Squid sends 404 if DNS fails (regular 404 from target is tunneled)
    case 404: // HTTP/1.1: "Not Found"
    // RFC 2616: "some deployed proxies are known to return 400 or 500 when
    // DNS lookups time out."  (Squid uses 500 if it runs out of sockets: so
    // we have a conflict here).
    case 400: // HTTP/1.1 "Bad Request"
    case 500: // HTTP/1.1: "Internal Server Error"
        /* User sees: "Address Not Found: Firefox can't find the server at
         * www.foo.com."
         */
        rv = NS_ERROR_UNKNOWN_HOST; 
        break;
    case 502: // HTTP/1.1: "Bad Gateway" (invalid resp from target server)
    // Squid returns 503 if target request fails for anything but DNS.
    case 503: // HTTP/1.1: "Service Unavailable"
        /* User sees: "Failed to Connect:
         *  Firefox can't establish a connection to the server at
         *  www.foo.com.  Though the site seems valid, the browser
         *  was unable to establish a connection."
         */
        rv = NS_ERROR_CONNECTION_REFUSED;
        break;
    // RFC 2616 uses 504 for both DNS and target timeout, so not clear what to
    // do here: picking target timeout, as DNS covered by 400/404/500
    case 504: // HTTP/1.1: "Gateway Timeout" 
        // user sees: "Network Timeout: The server at www.foo.com
        //              is taking too long to respond."
        rv = NS_ERROR_NET_TIMEOUT;
        break;
    // Confused proxy server or malicious response
    default:
        rv = NS_ERROR_PROXY_CONNECTION_REFUSED; 
        break;
    }
    LOG(("Cancelling failed SSL proxy connection [this=%p httpStatus=%u]\n",
         this, httpStatus)); 
    Cancel(rv);
    CallOnStartRequest();
    return rv;
}

PRBool
nsHttpChannel::ShouldSSLProxyResponseContinue(PRUint32 httpStatus)
{
    // When SSL connect has failed, allow proxy reply to continue only if it's
    // an auth request, or a redirect of a non-POST top-level document load.
    switch (httpStatus) {
    case 407:
        return PR_TRUE;
    case 300: case 301: case 302: case 303: case 307:
      {
        return ( (mLoadFlags & nsIChannel::LOAD_DOCUMENT_URI) &&
                 mURI == mDocumentURI &&
                 mRequestHead.Method() != nsHttp::Post);
      }
    }
    return PR_FALSE;
}

/**
 * Decide whether or not to remember Strict-Transport-Security, and whether
 * or not to enforce channel integrity.
 *
 * @return NS_ERROR_FAILURE if there's security information missing even though
 *             it's an HTTPS connection.
 */
nsresult
nsHttpChannel::ProcessSTSHeader()
{
    nsresult rv;
    PRBool isHttps = PR_FALSE;
    rv = mURI->SchemeIs("https", &isHttps);
    NS_ENSURE_SUCCESS(rv, rv);

    // If this channel is not loading securely, STS doesn't do anything.
    // The upgrade to HTTPS takes place earlier in the channel load process.
    if (!isHttps)
        return NS_OK;

    nsCAutoString asciiHost;
    rv = mURI->GetAsciiHost(asciiHost);
    NS_ENSURE_SUCCESS(rv, NS_OK);

    // If the channel is not a hostname, but rather an IP, STS doesn't do
    // anything.
    PRNetAddr hostAddr;
    if (PR_SUCCESS == PR_StringToNetAddr(asciiHost.get(), &hostAddr))
        return NS_OK;

    nsIStrictTransportSecurityService* stss = gHttpHandler->GetSTSService();
    NS_ENSURE_TRUE(stss, NS_ERROR_OUT_OF_MEMORY);

    // mSecurityInfo may not always be present, and if it's not then it is okay
    // to just disregard any STS headers since we know nothing about the
    // security of the connection.
    NS_ENSURE_TRUE(mSecurityInfo, NS_OK);

    // Check the trustworthiness of the channel (are there any cert errors?)
    // If there are certificate errors, we still load the data, we just ignore
    // any STS headers that are present.
    PRBool tlsIsBroken = PR_FALSE;
    rv = stss->ShouldIgnoreStsHeader(mSecurityInfo, &tlsIsBroken);
    NS_ENSURE_SUCCESS(rv, NS_OK);

    // If this was already an STS host, the connection should have been aborted
    // by the bad cert handler in the case of cert errors.  If it didn't abort the connection,
    // there's probably something funny going on.
    // If this wasn't an STS host, errors are allowed, but no more STS processing
    // will happen during the session.
    PRBool wasAlreadySTSHost;
    rv = stss->IsStsURI(mURI, &wasAlreadySTSHost);
    // Failure here means STS is broken.  Don't prevent the load, but this
    // shouldn't fail.
    NS_ENSURE_SUCCESS(rv, NS_OK);
    NS_ASSERTION(!(wasAlreadySTSHost && tlsIsBroken),
                 "connection should have been aborted by nss-bad-cert-handler");

    // Any STS header is ignored if the channel is not trusted due to
    // certificate errors (STS Spec 7.1) -- there is nothing else to do, and
    // the load may progress.
    if (tlsIsBroken) {
        LOG(("STS: Transport layer is not trustworthy, ignoring "
             "STS headers and continuing load\n"));
        return NS_OK;
    }

    // If there's a STS header, process it (STS Spec 7.1).  At this point in
    // processing, the channel is trusted, so the header should not be ignored.
    const nsHttpAtom atom = nsHttp::ResolveAtom("Strict-Transport-Security");
    nsCAutoString stsHeader;
    rv = mResponseHead->GetHeader(atom, stsHeader);
    if (rv == NS_ERROR_NOT_AVAILABLE) {
        LOG(("STS: No STS header, continuing load.\n"));
        return NS_OK;
    }
    // All other failures are fatal.
    NS_ENSURE_SUCCESS(rv, rv);

    rv = stss->ProcessStsHeader(mURI, stsHeader.get());
    if (NS_FAILED(rv)) {
        LOG(("STS: Failed to parse STS header, continuing load.\n"));
        return NS_OK;
    }

    return NS_OK;
}

nsresult
nsHttpChannel::ProcessResponse()
{
    nsresult rv;
    PRUint32 httpStatus = mResponseHead->Status();

    LOG(("nsHttpChannel::ProcessResponse [this=%p httpStatus=%u]\n",
        this, httpStatus));

    if (mTransaction->SSLConnectFailed()) {
        if (!ShouldSSLProxyResponseContinue(httpStatus))
            return ProcessFailedSSLConnect(httpStatus);
        // If SSL proxy response needs to complete, wait to process connection
        // for Strict-Transport-Security.
    } else {
        // Given a successful connection, process any STS data that's relevant.
        rv = ProcessSTSHeader();
        NS_ASSERTION(NS_SUCCEEDED(rv), "ProcessSTSHeader failed, continuing load.");
    }

    // notify "http-on-examine-response" observers
    gHttpHandler->OnExamineResponse(this);

    SetCookie(mResponseHead->PeekHeader(nsHttp::Set_Cookie));

    // handle unused username and password in url (see bug 232567)
    if (httpStatus != 401 && httpStatus != 407) {
        if (!mAuthRetryPending)
            mAuthProvider->CheckForSuperfluousAuth();
        if (mCanceled)
            return CallOnStartRequest();

        // reset the authentication's current continuation state because our
        // last authentication attempt has been completed successfully
        mAuthProvider->Disconnect(NS_ERROR_ABORT);
        mAuthProvider = nsnull;
        LOG(("  continuation state has been reset"));
    }

    // handle different server response categories.  Note that we handle
    // caching or not caching of error pages in
    // nsHttpResponseHead::MustValidate; if you change this switch, update that
    // one
    switch (httpStatus) {
    case 200:
    case 203:
        // Per RFC 2616, 14.35.2, "A server MAY ignore the Range header".
        // So if a server does that and sends 200 instead of 206 that we
        // expect, notify our caller.
        // However, if we wanted to start from the beginning, let it go through
        if (mResuming && mStartPos != 0) {
            LOG(("Server ignored our Range header, cancelling [this=%p]\n", this));
            Cancel(NS_ERROR_NOT_RESUMABLE);
            rv = CallOnStartRequest();
            break;
        }
        // these can normally be cached
        rv = ProcessNormal();
        MaybeInvalidateCacheEntryForSubsequentGet();
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
    case 303:
#if 0
    case 305: // disabled as a security measure (see bug 187996).
#endif
        // don't store the response body for redirects
        MaybeInvalidateCacheEntryForSubsequentGet();
        PushRedirectAsyncFunc(&nsHttpChannel::ContinueProcessResponse);
        rv = AsyncProcessRedirection(httpStatus);
        if (NS_FAILED(rv)) {
            PopRedirectAsyncFunc(&nsHttpChannel::ContinueProcessResponse);
            LOG(("AsyncProcessRedirection failed [rv=%x]\n", rv));
            rv = ContinueProcessResponse(rv);
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
        rv = mAuthProvider->ProcessAuthentication(
            httpStatus, mConnectionInfo->UsingSSL() &&
                        mTransaction->SSLConnectFailed());
        if (rv == NS_ERROR_IN_PROGRESS)  {
            // authentication prompt has been invoked and result
            // is expected asynchronously
            mAuthRetryPending = PR_TRUE;
            // suspend the transaction pump to stop receiving the
            // unauthenticated content data. We will throw that data
            // away when user provides credentials or resume the pump
            // when user refuses to authenticate.
            LOG(("Suspending the transaction, asynchronously prompting for credentials"));
            mTransactionPump->Suspend();
            rv = NS_OK;
        }
        else if (NS_FAILED(rv)) {
            LOG(("ProcessAuthentication failed [rv=%x]\n", rv));
            if (mTransaction->SSLConnectFailed())
                return ProcessFailedSSLConnect(httpStatus);
            if (!mAuthRetryPending)
                mAuthProvider->CheckForSuperfluousAuth();
            rv = ProcessNormal();
        }
        else
            mAuthRetryPending = PR_TRUE; // see DoAuthRetry
        break;
    default:
        rv = ProcessNormal();
        MaybeInvalidateCacheEntryForSubsequentGet();
        break;
    }

    return rv;
}

nsresult
nsHttpChannel::ContinueProcessResponse(nsresult rv)
{
    if (NS_SUCCEEDED(rv)) {
        InitCacheEntry();
        CloseCacheEntry(PR_FALSE);

        if (mCacheForOfflineUse) {
            // Store response in the offline cache
            InitOfflineCacheEntry();
            CloseOfflineCacheEntry();
        }
        return NS_OK;
    }

    LOG(("ContinueProcessResponse got failure result [rv=%x]\n", rv));
    if (mTransaction->SSLConnectFailed()) {
        return ProcessFailedSSLConnect(mRedirectType);
    }
    return ProcessNormal();
}

nsresult
nsHttpChannel::ProcessNormal()
{
    nsresult rv;

    LOG(("nsHttpChannel::ProcessNormal [this=%p]\n", this));

    PRBool succeeded;
    rv = GetRequestSucceeded(&succeeded);
    if (NS_SUCCEEDED(rv) && !succeeded) {
        PushRedirectAsyncFunc(&nsHttpChannel::ContinueProcessNormal);
        PRBool waitingForRedirectCallback;
        (void)ProcessFallback(&waitingForRedirectCallback);
        if (waitingForRedirectCallback) {
            // The transaction has been suspended by ProcessFallback.
            return NS_OK;
        }
        PopRedirectAsyncFunc(&nsHttpChannel::ContinueProcessNormal);
    }

    return ContinueProcessNormal(NS_OK);
}

nsresult
nsHttpChannel::ContinueProcessNormal(nsresult rv)
{
    if (NS_FAILED(rv)) {
        // Fill the failure status here, we have failed to fall back, thus we
        // have to report our status as failed.
        mStatus = rv;
        DoNotifyListener();
        return rv;
    }

    if (mFallingBack) {
        // Do not continue with normal processing, fallback is in
        // progress now.
        return NS_OK;
    }

    // if we're here, then any byte-range requests failed to result in a partial
    // response.  we must clear this flag to prevent BufferPartialContent from
    // being called inside our OnDataAvailable (see bug 136678).
    mCachedContentIsPartial = PR_FALSE;

    ClearBogusContentEncodingIfNeeded();

    // this must be called before firing OnStartRequest, since http clients,
    // such as imagelib, expect our cache entry to already have the correct
    // expiration time (bug 87710).
    if (mCacheEntry) {
        rv = InitCacheEntry();
        if (NS_FAILED(rv))
            CloseCacheEntry(PR_TRUE);
    }

    // Check that the server sent us what we were asking for
    if (mResuming) {
        // Create an entity id from the response
        nsCAutoString id;
        rv = GetEntityID(id);
        if (NS_FAILED(rv)) {
            // If creating an entity id is not possible -> error
            Cancel(NS_ERROR_NOT_RESUMABLE);
        }
        else if (mResponseHead->Status() != 206 &&
                 mResponseHead->Status() != 200) {
            // Probably 404 Not Found, 412 Precondition Failed or
            // 416 Invalid Range -> error
            LOG(("Unexpected response status while resuming, aborting [this=%p]\n",
                 this));
            Cancel(NS_ERROR_ENTITY_CHANGED);
        }
        // If we were passed an entity id, verify it's equal to the server's
        else if (!mEntityID.IsEmpty()) {
            if (!mEntityID.Equals(id)) {
                LOG(("Entity mismatch, expected '%s', got '%s', aborting [this=%p]",
                     mEntityID.get(), id.get(), this));
                Cancel(NS_ERROR_ENTITY_CHANGED);
            }
        }
    }

    rv = CallOnStartRequest();
    if (NS_FAILED(rv)) return rv;

    // install cache listener if we still have a cache entry open
    if (mCacheEntry && (mCacheAccess & nsICache::ACCESS_WRITE)) {
        rv = InstallCacheListener();
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}

nsresult
nsHttpChannel::PromptTempRedirect()
{
    if (!gHttpHandler->PromptTempRedirect()) {
        return NS_OK;
    }
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
        GetCallback(prompt);
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
    LOG(("nsHttpChannel::ProxyFailover [this=%p]\n", this));

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

    // XXXbz so where does this codepath remove us from the loadgroup,
    // exactly?
    return AsyncDoReplaceWithProxy(pi);
}

void
nsHttpChannel::HandleAsyncReplaceWithProxy()
{
    NS_PRECONDITION(!mPendingAsyncCallOnResume, "How did that happen?");

    if (mSuspendCount) {
        LOG(("Waiting until resume to do async proxy replacement [this=%p]\n",
             this));
        mPendingAsyncCallOnResume =
            &nsHttpChannel::HandleAsyncReplaceWithProxy;
        return;
    }

    nsresult status = mStatus;
    
    nsCOMPtr<nsIProxyInfo> pi;
    pi.swap(mTargetProxyInfo);
    if (!mCanceled) {
        PushRedirectAsyncFunc(&nsHttpChannel::ContinueHandleAsyncReplaceWithProxy);
        status = AsyncDoReplaceWithProxy(pi);
        if (NS_SUCCEEDED(status))
            return;
        PopRedirectAsyncFunc(&nsHttpChannel::ContinueHandleAsyncReplaceWithProxy);
    }

    if (NS_FAILED(status)) {
        ContinueHandleAsyncReplaceWithProxy(status);
    }
}

nsresult
nsHttpChannel::ContinueHandleAsyncReplaceWithProxy(nsresult status)
{
    if (mLoadGroup && NS_SUCCEEDED(status)) {
        mLoadGroup->RemoveRequest(this, nsnull, mStatus);
    }
    else if (NS_FAILED(status)) {
       AsyncAbort(status);
    }

    // Return NS_OK here, even it seems to be breaking the async function stack
    // contract (i.e. passing the result code to a function bellow).
    // ContinueHandleAsyncReplaceWithProxy will always be at the bottom of the
    // stack. If we would return the failure code, the async function stack
    // logic would cancel the channel synchronously, which is undesired after
    // invoking AsyncAbort above.
    return NS_OK;
}

void
nsHttpChannel::HandleAsyncRedirectChannelToHttps()
{
    NS_PRECONDITION(!mPendingAsyncCallOnResume, "How did that happen?");

    if (mSuspendCount) {
        LOG(("Waiting until resume to do async redirect to https [this=%p]\n", this));
        mPendingAsyncCallOnResume = &nsHttpChannel::HandleAsyncRedirectChannelToHttps;
        return;
    }

    nsresult rv = AsyncRedirectChannelToHttps();
    if (NS_FAILED(rv))
        ContinueAsyncRedirectChannelToHttps(rv);
}

nsresult
nsHttpChannel::AsyncRedirectChannelToHttps()
{
    nsresult rv = NS_OK;
    LOG(("nsHttpChannel::HandleAsyncRedirectChannelToHttps() [STS]\n"));

    nsCOMPtr<nsIChannel> newChannel;
    nsCOMPtr<nsIURI> upgradedURI;

    rv = mURI->Clone(getter_AddRefs(upgradedURI));
    NS_ENSURE_SUCCESS(rv,rv);

    upgradedURI->SetScheme(NS_LITERAL_CSTRING("https"));

    PRInt32 oldPort = -1;
    rv = mURI->GetPort(&oldPort);
    if (NS_FAILED(rv)) return rv;

    // Keep any nonstandard ports so only the scheme is changed.
    // For example:
    //  http://foo.com:80 -> https://foo.com:443
    //  http://foo.com:81 -> https://foo.com:81

    if (oldPort == 80 || oldPort == -1)
        upgradedURI->SetPort(-1);
    else
        upgradedURI->SetPort(oldPort);

    nsCOMPtr<nsIIOService> ioService;
    rv = gHttpHandler->GetIOService(getter_AddRefs(ioService));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = ioService->NewChannelFromURI(upgradedURI, getter_AddRefs(newChannel));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = SetupReplacementChannel(upgradedURI, newChannel, PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);

    // Inform consumers about this fake redirect
    mRedirectChannel = newChannel;
    PRUint32 flags = nsIChannelEventSink::REDIRECT_PERMANENT;

    PushRedirectAsyncFunc(
        &nsHttpChannel::ContinueAsyncRedirectChannelToHttps);
    rv = gHttpHandler->AsyncOnChannelRedirect(this, newChannel, flags);

    if (NS_SUCCEEDED(rv))
        rv = WaitForRedirectCallback();

    if (NS_FAILED(rv)) {
        AutoRedirectVetoNotifier notifier(this);
        PopRedirectAsyncFunc(
            &nsHttpChannel::ContinueAsyncRedirectChannelToHttps);
    }

    return rv;
}

nsresult
nsHttpChannel::ContinueAsyncRedirectChannelToHttps(nsresult rv)
{
    AutoRedirectVetoNotifier notifier(this);

    if (NS_FAILED(rv)) {
        // Fill the failure status here, the update to https had been vetoed
        // but from the security reasons we have to discard the whole channel
        // load.
        mStatus = rv;
    }

    if (mLoadGroup)
        mLoadGroup->RemoveRequest(this, nsnull, mStatus);

    if (NS_FAILED(rv)) {
        // We have to manually notify the listener because there is not any pump
        // that would call our OnStart/StopRequest after resume from waiting for
        // the redirect callback.
        DoNotifyListener();
        return rv;
    }

    // Make sure to do this _after_ calling OnChannelRedirect
    mRedirectChannel->SetOriginalURI(mOriginalURI);

    // And now, notify observers the deprecated way
    nsCOMPtr<nsIHttpEventSink> httpEventSink;
    GetCallback(httpEventSink);
    if (httpEventSink) {
        // NOTE: nsIHttpEventSink is only used for compatibility with pre-1.8
        // versions.
        rv = httpEventSink->OnRedirect(this, mRedirectChannel);
        if (NS_FAILED(rv)) {
            mStatus = rv;
            DoNotifyListener();
            return rv;
        }
    }

    // open new channel
    rv = mRedirectChannel->AsyncOpen(mListener, mListenerContext);
    if (NS_FAILED(rv)) {
        mStatus = rv;
        DoNotifyListener();
        return rv;
    }

    mStatus = NS_BINDING_REDIRECTED;

    notifier.RedirectSucceeded();

    // disconnect from the old listeners...
    mListener = nsnull;
    mListenerContext = nsnull;

    // ...and the old callbacks
    mCallbacks = nsnull;
    mProgressSink = nsnull;

    return rv;
}

nsresult
nsHttpChannel::AsyncDoReplaceWithProxy(nsIProxyInfo* pi)
{
    LOG(("nsHttpChannel::AsyncDoReplaceWithProxy [this=%p pi=%p]", this, pi));
    nsresult rv;

    nsCOMPtr<nsIChannel> newChannel;
    rv = gHttpHandler->NewProxiedChannel(mURI, pi, getter_AddRefs(newChannel));
    if (NS_FAILED(rv))
        return rv;

    rv = SetupReplacementChannel(mURI, newChannel, PR_TRUE);
    if (NS_FAILED(rv))
        return rv;

    // Inform consumers about this fake redirect
    mRedirectChannel = newChannel;
    PRUint32 flags = nsIChannelEventSink::REDIRECT_INTERNAL;

    PushRedirectAsyncFunc(&nsHttpChannel::ContinueDoReplaceWithProxy);
    rv = gHttpHandler->AsyncOnChannelRedirect(this, newChannel, flags);

    if (NS_SUCCEEDED(rv))
        rv = WaitForRedirectCallback();

    if (NS_FAILED(rv)) {
        AutoRedirectVetoNotifier notifier(this);
        PopRedirectAsyncFunc(&nsHttpChannel::ContinueDoReplaceWithProxy);
    }

    return rv;
}

nsresult
nsHttpChannel::ContinueDoReplaceWithProxy(nsresult rv)
{
    AutoRedirectVetoNotifier notifier(this);

    if (NS_FAILED(rv))
        return rv;

    NS_PRECONDITION(mRedirectChannel, "No redirect channel?");

    // Make sure to do this _after_ calling OnChannelRedirect
    mRedirectChannel->SetOriginalURI(mOriginalURI);

    // open new channel
    rv = mRedirectChannel->AsyncOpen(mListener, mListenerContext);
    if (NS_FAILED(rv))
        return rv;

    mStatus = NS_BINDING_REDIRECTED;

    notifier.RedirectSucceeded();

    // disconnect from the old listeners...
    mListener = nsnull;
    mListenerContext = nsnull;

    // ...and the old callbacks
    mCallbacks = nsnull;
    mProgressSink = nsnull;

    return rv;
}

nsresult
nsHttpChannel::ResolveProxy()
{
    LOG(("nsHttpChannel::ResolveProxy [this=%p]\n", this));

    nsresult rv;

    nsCOMPtr<nsIProtocolProxyService> pps =
            do_GetService(NS_PROTOCOLPROXYSERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv))
        return rv;

    return pps->AsyncResolve(mURI, 0, this, getter_AddRefs(mProxyRequest));
}

PRBool
nsHttpChannel::ResponseWouldVary()
{
    nsresult rv;
    nsCAutoString buf, metaKey;
    mCachedResponseHead->GetHeader(nsHttp::Vary, buf);
    if (!buf.IsEmpty()) {
        NS_NAMED_LITERAL_CSTRING(prefix, "request-");

        // enumerate the elements of the Vary header...
        char *val = buf.BeginWriting(); // going to munge buf
        char *token = nsCRT::strtok(val, NS_HTTP_HEADER_SEPS, &val);
        while (token) {
            LOG(("nsHttpChannel::ResponseWouldVary [this=%x] " \
                 "processing %s\n",
                 this, token));
            //
            // if "*", then assume response would vary.  technically speaking,
            // "Vary: header, *" is not permitted, but we allow it anyways.
            //
            // We hash values of cookie-headers for the following reasons:
            //
            //   1- cookies can be very large in size
            //
            //   2- cookies may contain sensitive information.  (for parity with
            //      out policy of not storing Set-cookie headers in the cache
            //      meta data, we likewise do not want to store cookie headers
            //      here.)
            //
            if (*token == '*')
                return PR_TRUE; // if we encounter this, just get out of here

            // build cache meta data key...
            metaKey = prefix + nsDependentCString(token);

            // check the last value of the given request header to see if it has
            // since changed.  if so, then indeed the cached response is invalid.
            nsXPIDLCString lastVal;
            mCacheEntry->GetMetaDataElement(metaKey.get(), getter_Copies(lastVal));
            LOG(("nsHttpChannel::ResponseWouldVary [this=%x] " \
                    "stored value = %c%s%c\n", this, '"', lastVal.get(), '"'));

            // Look for value of "Cookie" in the request headers
            nsHttpAtom atom = nsHttp::ResolveAtom(token);
            const char *newVal = mRequestHead.PeekHeader(atom);
            if (!lastVal.IsEmpty()) {
                // value for this header in cache, but no value in request
                if (!newVal)
                    return PR_TRUE; // yes - response would vary

                // If this is a cookie-header, stored metadata is not
                // the value itself but the hash. So we also hash the
                // outgoing value here in order to compare the hashes
                nsCAutoString hash;
                if (atom == nsHttp::Cookie) {
                    rv = Hash(newVal, hash);
                    // If hash failed, be conservative (the cached hash
                    // exists at this point) and claim response would vary
                    if (NS_FAILED(rv))
                        return PR_TRUE;
                    newVal = hash.get();

                    LOG(("nsHttpChannel::ResponseWouldVary [this=%x] " \
                            "set-cookie value hashed to %s\n",
                         this, newVal));
                }

                if (strcmp(newVal, lastVal))
                    return PR_TRUE; // yes, response would vary

            } else if (newVal) { // old value is empty, but newVal is set
                return PR_TRUE;
            }

            // next token...
            token = nsCRT::strtok(val, NS_HTTP_HEADER_SEPS, &val);
        }
    }
    return PR_FALSE;
}

nsresult
nsHttpChannel::Hash(const char *buf, nsACString &hash)
{
    nsresult rv;
    if (!mHasher) {
        mHasher = do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID, &rv);
        if (NS_FAILED(rv)) {
            LOG(("nsHttpChannel: Failed to instantiate crypto-hasher"));
            return rv;
        }
    }

    rv = mHasher->Init(nsICryptoHash::SHA1);
    NS_ENSURE_SUCCESS(rv, rv);

   rv = mHasher->Update(reinterpret_cast<unsigned const char*>(buf),
                         strlen(buf));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mHasher->Finish(PR_TRUE, hash);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
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
    // ok, we've just received a 206
    //
    // we need to stream whatever data is in the cache out first, and then
    // pick up whatever data is on the wire, writing it into the cache.

    LOG(("nsHttpChannel::ProcessPartialContent [this=%p]\n", this)); 

    NS_ENSURE_TRUE(mCachedResponseHead, NS_ERROR_NOT_INITIALIZED);
    NS_ENSURE_TRUE(mCacheEntry, NS_ERROR_NOT_INITIALIZED);

    // Make sure to clear bogus content-encodings before looking at the header
    ClearBogusContentEncodingIfNeeded();
    
    // Check if the content-encoding we now got is different from the one we
    // got before
    if (PL_strcasecmp(mResponseHead->PeekHeader(nsHttp::Content_Encoding),
                      mCachedResponseHead->PeekHeader(nsHttp::Content_Encoding))
                      != 0) {
        Cancel(NS_ERROR_INVALID_CONTENT_ENCODING);
        return CallOnStartRequest();
    }


    // suspend the current transaction
    nsresult rv = mTransactionPump->Suspend();
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
    mResponseHead = mCachedResponseHead;

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

    LOG(("nsHttpChannel::OnDoneReadingPartialCacheEntry [this=%p]", this));

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

    LOG(("nsHttpChannel::ProcessNotModified [this=%p]\n", this)); 

    if (mCustomConditionalRequest) {
        LOG(("Bypassing ProcessNotModified due to custom conditional headers")); 
        return NS_ERROR_FAILURE;
    }

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
    mResponseHead = mCachedResponseHead;

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
nsHttpChannel::ProcessFallback(PRBool *waitingForRedirectCallback)
{
    LOG(("nsHttpChannel::ProcessFallback [this=%p]\n", this));
    nsresult rv;

    *waitingForRedirectCallback = PR_FALSE;
    mFallingBack = PR_FALSE;

    // At this point a load has failed (either due to network problems
    // or an error returned on the server).  Perform an application
    // cache fallback if we have a URI to fall back to.
    if (!mApplicationCache || mFallbackKey.IsEmpty() || mFallbackChannel) {
        LOG(("  choosing not to fallback [%p,%s,%d]",
             mApplicationCache.get(), mFallbackKey.get(), mFallbackChannel));
        return NS_OK;
    }

    // Make sure the fallback entry hasn't been marked as a foreign
    // entry.
    PRUint32 fallbackEntryType;
    rv = mApplicationCache->GetTypes(mFallbackKey, &fallbackEntryType);
    NS_ENSURE_SUCCESS(rv, rv);

    if (fallbackEntryType & nsIApplicationCache::ITEM_FOREIGN) {
        // This cache points to a fallback that refers to a different
        // manifest.  Refuse to fall back.
        return NS_OK;
    }

    NS_ASSERTION(fallbackEntryType & nsIApplicationCache::ITEM_FALLBACK,
                 "Fallback entry not marked correctly!");

    // Kill any opportunistic cache entry, and disable opportunistic
    // caching for the fallback.
    if (mOfflineCacheEntry) {
        mOfflineCacheEntry->Doom();
        mOfflineCacheEntry = 0;
        mOfflineCacheAccess = 0;
    }

    mCacheForOfflineUse = PR_FALSE;
    mCachingOpportunistically = PR_FALSE;
    mOfflineCacheClientID.Truncate();
    mOfflineCacheEntry = 0;
    mOfflineCacheAccess = 0;

    // Close the current cache entry.
    if (mCacheEntry)
        CloseCacheEntry(PR_TRUE);

    // Create a new channel to load the fallback entry.
    nsRefPtr<nsIChannel> newChannel;
    rv = gHttpHandler->NewChannel(mURI, getter_AddRefs(newChannel));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = SetupReplacementChannel(mURI, newChannel, PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);

    // Make sure the new channel loads from the fallback key.
    nsCOMPtr<nsIHttpChannelInternal> httpInternal =
        do_QueryInterface(newChannel, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = httpInternal->SetupFallbackChannel(mFallbackKey.get());
    NS_ENSURE_SUCCESS(rv, rv);

    // ... and fallbacks should only load from the cache.
    PRUint32 newLoadFlags = mLoadFlags | LOAD_REPLACE | LOAD_ONLY_FROM_CACHE;
    rv = newChannel->SetLoadFlags(newLoadFlags);

    // Inform consumers about this fake redirect
    mRedirectChannel = newChannel;
    PRUint32 redirectFlags = nsIChannelEventSink::REDIRECT_INTERNAL;

    PushRedirectAsyncFunc(&nsHttpChannel::ContinueProcessFallback);
    rv = gHttpHandler->AsyncOnChannelRedirect(this, newChannel, redirectFlags);

    if (NS_SUCCEEDED(rv))
        rv = WaitForRedirectCallback();

    if (NS_FAILED(rv)) {
        AutoRedirectVetoNotifier notifier(this);
        PopRedirectAsyncFunc(&nsHttpChannel::ContinueProcessFallback);
        return rv;
    }

    // Indicate we are now waiting for the asynchronous redirect callback
    // if all went OK.
    *waitingForRedirectCallback = PR_TRUE;
    return NS_OK;
}

nsresult
nsHttpChannel::ContinueProcessFallback(nsresult rv)
{
    AutoRedirectVetoNotifier notifier(this);

    if (NS_FAILED(rv))
        return rv;

    NS_PRECONDITION(mRedirectChannel, "No redirect channel?");

    // Make sure to do this _after_ calling OnChannelRedirect
    mRedirectChannel->SetOriginalURI(mOriginalURI);

    rv = mRedirectChannel->AsyncOpen(mListener, mListenerContext);
    if (NS_FAILED(rv))
        return rv;

    // close down this channel
    Cancel(NS_BINDING_REDIRECTED);

    notifier.RedirectSucceeded();

    // disconnect from our listener
    mListener = 0;
    mListenerContext = 0;

    // and from our callbacks
    mCallbacks = nsnull;
    mProgressSink = nsnull;

    mFallingBack = PR_TRUE;

    return NS_OK;
}

// Determines if a request is a byte range request for a subrange,
// i.e. is a byte range request, but not a 0- byte range request.
static PRBool
IsSubRangeRequest(nsHttpRequestHead &aRequestHead)
{
    if (!aRequestHead.PeekHeader(nsHttp::Range))
        return PR_FALSE;
    nsCAutoString byteRange;
    aRequestHead.GetHeader(nsHttp::Range, byteRange);
    return !byteRange.EqualsLiteral("bytes=0-");
}

nsresult
nsHttpChannel::OpenCacheEntry()
{
    nsresult rv;

    mAsyncCacheOpen = PR_FALSE;
    mLoadedFromApplicationCache = PR_FALSE;

    LOG(("nsHttpChannel::OpenCacheEntry [this=%p]", this));

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

    if (mResuming) {
        // We don't support caching for requests initiated
        // via nsIResumableChannel.
        return NS_OK;
    }

    // Don't cache byte range requests which are subranges, only cache 0-
    // byte range requests.
    if (IsSubRangeRequest(mRequestHead))
        return NS_OK;

    GenerateCacheKey(mPostID, cacheKey);

    // Set the desired cache access mode accordingly...
    nsCacheAccessMode accessRequested;
    rv = DetermineCacheAccess(&accessRequested);
    if (NS_FAILED(rv)) return rv;

    if (!mApplicationCache && mInheritApplicationCache) {
        // Pick up an application cache from the notification
        // callbacks if available
        nsCOMPtr<nsIApplicationCacheContainer> appCacheContainer;
        GetCallback(appCacheContainer);

        if (appCacheContainer) {
            appCacheContainer->GetApplicationCache(getter_AddRefs(mApplicationCache));
        }
    }

    if (!mApplicationCache &&
        (mChooseApplicationCache || (mLoadFlags & LOAD_CHECK_OFFLINE_CACHE))) {
        // We're supposed to load from an application cache, but
        // one was not supplied by the load group.  Ask the
        // application cache service to choose one for us.
        nsCOMPtr<nsIApplicationCacheService> appCacheService =
            do_GetService(NS_APPLICATIONCACHESERVICE_CONTRACTID);
        if (appCacheService) {
            nsresult rv = appCacheService->ChooseApplicationCache
                (cacheKey, getter_AddRefs(mApplicationCache));
            NS_ENSURE_SUCCESS(rv, rv);
        }
    }

    nsCOMPtr<nsICacheSession> session;

    // If we have an application cache, we check it first.
    if (mApplicationCache) {
        nsCAutoString appCacheClientID;
        mApplicationCache->GetClientID(appCacheClientID);

        nsCOMPtr<nsICacheService> serv =
            do_GetService(NS_CACHESERVICE_CONTRACTID, &rv);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = serv->CreateSession(appCacheClientID.get(),
                                 nsICache::STORE_OFFLINE,
                                 nsICache::STREAM_BASED,
                                 getter_AddRefs(session));
        NS_ENSURE_SUCCESS(rv, rv);

        if (mLoadFlags & LOAD_BYPASS_LOCAL_CACHE_IF_BUSY) {
            // must use synchronous open for LOAD_BYPASS_LOCAL_CACHE_IF_BUSY
            rv = session->OpenCacheEntry(cacheKey,
                                         nsICache::ACCESS_READ, PR_FALSE,
                                         getter_AddRefs(mCacheEntry));
            if (NS_SUCCEEDED(rv)) {
                mCacheEntry->GetAccessGranted(&mCacheAccess);
                LOG(("nsHttpChannel::OpenCacheEntry [this=%p grantedAccess=%d]",
                    this, mCacheAccess));
                mLoadedFromApplicationCache = PR_TRUE;
                return NS_OK;
            } else if (rv == NS_ERROR_CACHE_WAIT_FOR_VALIDATION) {
                LOG(("bypassing local cache since it is busy\n"));
                // Don't try to load normal cache entry
                return NS_ERROR_NOT_AVAILABLE;
            }
        } else {
            mOnCacheEntryAvailableCallback =
                &nsHttpChannel::OnOfflineCacheEntryAvailable;
            // We open with ACCESS_READ only, because we don't want to
            // overwrite the offline cache entry non-atomically.
            // ACCESS_READ will prevent us from writing to the offline
            // cache as a normal cache entry.
            rv = session->AsyncOpenCacheEntry(cacheKey,
                                              nsICache::ACCESS_READ,
                                              this);

            if (NS_SUCCEEDED(rv)) {
                mAsyncCacheOpen = PR_TRUE;
                return NS_OK;
            }
        }

        // sync or async opening failed
        return OnOfflineCacheEntryAvailable(nsnull, nsICache::ACCESS_NONE,
                                            rv, PR_TRUE);
    }

    return OpenNormalCacheEntry(PR_TRUE);
}

nsresult
nsHttpChannel::OnOfflineCacheEntryAvailable(nsICacheEntryDescriptor *aEntry,
                                            nsCacheAccessMode aAccess,
                                            nsresult aEntryStatus,
                                            PRBool aIsSync)
{
    nsresult rv;

    if (NS_SUCCEEDED(aEntryStatus)) {
        // We successfully opened an offline cache session and the entry,
        // so indicate we will load from the offline cache.
        mLoadedFromApplicationCache = PR_TRUE;
        mCacheEntry = aEntry;
        mCacheAccess = aAccess;
    }

    if (mCanceled && NS_FAILED(mStatus)) {
        LOG(("channel was canceled [this=%p status=%x]\n", this, mStatus));
        return mStatus;
    }

    if (NS_SUCCEEDED(aEntryStatus))
        // Called from OnCacheEntryAvailable, advance to the next state
        return Connect(PR_FALSE);

    if (!mCacheForOfflineUse && !mFallbackChannel) {
        nsCAutoString cacheKey;
        GenerateCacheKey(mPostID, cacheKey);

        // Check for namespace match.
        nsCOMPtr<nsIApplicationCacheNamespace> namespaceEntry;
        rv = mApplicationCache->GetMatchingNamespace
            (cacheKey, getter_AddRefs(namespaceEntry));
        if (NS_FAILED(rv) && !aIsSync)
            return Connect(PR_FALSE);
        NS_ENSURE_SUCCESS(rv, rv);

        PRUint32 namespaceType = 0;
        if (!namespaceEntry ||
            NS_FAILED(namespaceEntry->GetItemType(&namespaceType)) ||
            (namespaceType &
             (nsIApplicationCacheNamespace::NAMESPACE_FALLBACK |
              nsIApplicationCacheNamespace::NAMESPACE_OPPORTUNISTIC |
              nsIApplicationCacheNamespace::NAMESPACE_BYPASS)) == 0) {
            // When loading from an application cache, only items
            // on the whitelist or matching a
            // fallback/opportunistic namespace should hit the
            // network...
            mLoadFlags |= LOAD_ONLY_FROM_CACHE;

            // ... and if there were an application cache entry,
            // we would have found it earlier.
            return aIsSync ? NS_ERROR_CACHE_KEY_NOT_FOUND : Connect(PR_FALSE);
        }

        if (namespaceType &
            nsIApplicationCacheNamespace::NAMESPACE_FALLBACK) {
            rv = namespaceEntry->GetData(mFallbackKey);
            if (NS_FAILED(rv) && !aIsSync)
                return Connect(PR_FALSE);
            NS_ENSURE_SUCCESS(rv, rv);
        }

        if ((namespaceType &
             nsIApplicationCacheNamespace::NAMESPACE_OPPORTUNISTIC) &&
            mLoadFlags & LOAD_DOCUMENT_URI) {
            // Document loads for items in an opportunistic namespace
            // should be placed in the offline cache.
            nsCString clientID;
            mApplicationCache->GetClientID(clientID);

            mCacheForOfflineUse = !clientID.IsEmpty();
            SetOfflineCacheClientID(clientID);
            mCachingOpportunistically = PR_TRUE;
        }
    }

    return OpenNormalCacheEntry(aIsSync);
}


nsresult
nsHttpChannel::OpenNormalCacheEntry(PRBool aIsSync)
{
    NS_ASSERTION(!mCacheEntry, "We have already mCacheEntry");

    nsresult rv;

    nsCAutoString cacheKey;
    GenerateCacheKey(mPostID, cacheKey);

    nsCacheStoragePolicy storagePolicy = DetermineStoragePolicy();

    nsCOMPtr<nsICacheSession> session;
    rv = gHttpHandler->GetCacheSession(storagePolicy,
                                       getter_AddRefs(session));
    if (NS_FAILED(rv)) return rv;

    nsCacheAccessMode accessRequested;
    rv = DetermineCacheAccess(&accessRequested);
    if (NS_FAILED(rv)) return rv;

    if (mLoadFlags & LOAD_BYPASS_LOCAL_CACHE_IF_BUSY) {
        if (!aIsSync) {
            // Unexpected state: we were called from OnCacheEntryAvailable(),
            // so LOAD_BYPASS_LOCAL_CACHE_IF_BUSY shouldn't be set. Unless
            // somebody altered mLoadFlags between OpenCacheEntry() and
            // OnCacheEntryAvailable()...
            NS_WARNING(
                "OpenNormalCacheEntry() called from OnCacheEntryAvailable() "
                "when LOAD_BYPASS_LOCAL_CACHE_IF_BUSY was specified");
        }

        // must use synchronous open for LOAD_BYPASS_LOCAL_CACHE_IF_BUSY
        rv = session->OpenCacheEntry(cacheKey, accessRequested, PR_FALSE,
                                     getter_AddRefs(mCacheEntry));
        if (NS_SUCCEEDED(rv)) {
            mCacheEntry->GetAccessGranted(&mCacheAccess);
            LOG(("nsHttpChannel::OpenCacheEntry [this=%p grantedAccess=%d]",
                this, mCacheAccess));
        }
        else if (rv == NS_ERROR_CACHE_WAIT_FOR_VALIDATION) {
            LOG(("bypassing local cache since it is busy\n"));
            rv = NS_ERROR_NOT_AVAILABLE;
        }
    }
    else {
        mOnCacheEntryAvailableCallback =
            &nsHttpChannel::OnNormalCacheEntryAvailable;
        rv = session->AsyncOpenCacheEntry(cacheKey, accessRequested, this);
        if (NS_SUCCEEDED(rv)) {
            mAsyncCacheOpen = PR_TRUE;
            return NS_OK;
        }
    }

    if (!aIsSync)
        // Called from OnCacheEntryAvailable, advance to the next state
        rv = Connect(PR_FALSE);

    return rv;
}

nsresult
nsHttpChannel::OnNormalCacheEntryAvailable(nsICacheEntryDescriptor *aEntry,
                                           nsCacheAccessMode aAccess,
                                           nsresult aEntryStatus,
                                           PRBool aIsSync)
{
    NS_ASSERTION(!aIsSync, "aIsSync should be false");

    if (NS_SUCCEEDED(aEntryStatus)) {
        mCacheEntry = aEntry;
        mCacheAccess = aAccess;
    }

    if (mCanceled && NS_FAILED(mStatus)) {
        LOG(("channel was canceled [this=%p status=%x]\n", this, mStatus));
        return mStatus;
    }

    if ((mLoadFlags & LOAD_ONLY_FROM_CACHE) && NS_FAILED(aEntryStatus))
        // if this channel is only allowed to pull from the cache, then
        // we must fail if we were unable to open a cache entry.
        return NS_ERROR_DOCUMENT_NOT_CACHED;

    // advance to the next state...
    return Connect(PR_FALSE);
}


nsresult
nsHttpChannel::OpenOfflineCacheEntryForWriting()
{
    nsresult rv;

    LOG(("nsHttpChannel::OpenOfflineCacheEntryForWriting [this=%p]", this));

    // make sure we're not abusing this function
    NS_PRECONDITION(!mOfflineCacheEntry, "cache entry already open");

    PRBool offline = gIOService->IsOffline();
    if (offline) {
        // only put things in the offline cache while online
        return NS_OK;
    }

    if (mRequestHead.Method() != nsHttp::Get) {
        // only cache complete documents offline
        return NS_OK;
    }

    // Don't cache byte range requests which are subranges, only cache 0-
    // byte range requests.
    if (IsSubRangeRequest(mRequestHead))
        return NS_OK;

    nsCAutoString cacheKey;
    GenerateCacheKey(mPostID, cacheKey);

    NS_ENSURE_TRUE(!mOfflineCacheClientID.IsEmpty(),
                   NS_ERROR_NOT_AVAILABLE);

    nsCOMPtr<nsICacheSession> session;
    nsCOMPtr<nsICacheService> serv =
        do_GetService(NS_CACHESERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = serv->CreateSession(mOfflineCacheClientID.get(),
                             nsICache::STORE_OFFLINE,
                             nsICache::STREAM_BASED,
                             getter_AddRefs(session));
    if (NS_FAILED(rv)) return rv;

    rv = session->OpenCacheEntry(cacheKey, nsICache::ACCESS_READ_WRITE,
                                 PR_FALSE, getter_AddRefs(mOfflineCacheEntry));

    if (rv == NS_ERROR_CACHE_WAIT_FOR_VALIDATION) {
        // access to the cache entry has been denied (because the cache entry
        // is probably in use by another channel).  Either the cache is being
        // read from (we're offline) or it's being updated elsewhere.
        return NS_OK;
    }

    if (NS_SUCCEEDED(rv)) {
        mOfflineCacheEntry->GetAccessGranted(&mOfflineCacheAccess);
        LOG(("got offline cache entry [access=%x]\n", mOfflineCacheAccess));
    }

    return rv;
}

nsresult
nsHttpChannel::GenerateCacheKey(PRUint32 postID, nsACString &cacheKey)
{
    cacheKey.Truncate();

    if (mLoadFlags & LOAD_ANONYMOUS) {
      cacheKey.AssignLiteral("anon&");
    }

    if (postID) {
        char buf[32];
        PR_snprintf(buf, sizeof(buf), "id=%x&", postID);
        cacheKey.Append(buf);
    }

    if (!cacheKey.IsEmpty()) {
      cacheKey.AppendLiteral("uri=");
    }

    // Strip any trailing #ref from the URL before using it as the key
    const char *spec = mFallbackChannel ? mFallbackKey.get() : mSpec.get();
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

    nsresult rv;

    PRUint32 expirationTime = 0;
    if (!mResponseHead->MustValidate()) {
        PRUint32 freshnessLifetime = 0;

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

    rv = mCacheEntry->SetExpirationTime(expirationTime);
    NS_ENSURE_SUCCESS(rv, rv);

    if (mOfflineCacheEntry) {
        rv = mOfflineCacheEntry->SetExpirationTime(expirationTime);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    return NS_OK;
}

// CheckCache is called from Connect after a cache entry has been opened for
// this URL but before going out to net.  It's purpose is to set or clear the 
// mCachedContentIsValid flag, and to configure an If-Modified-Since request
// if validation is required.
nsresult
nsHttpChannel::CheckCache()
{
    nsresult rv = NS_OK;

    LOG(("nsHTTPChannel::CheckCache enter [this=%p entry=%p access=%d]",
        this, mCacheEntry.get(), mCacheAccess));
    
    // Be pessimistic: assume the cache entry has no useful data.
    mCachedContentIsValid = PR_FALSE;

    // Don't proceed unless we have opened a cache entry for reading.
    if (!mCacheEntry || !(mCacheAccess & nsICache::ACCESS_READ))
        return NS_OK;

    nsXPIDLCString buf;

    // Get the method that was used to generate the cached response
    rv = mCacheEntry->GetMetaDataElement("request-method", getter_Copies(buf));
    NS_ENSURE_SUCCESS(rv, rv);

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
    NS_ENSURE_SUCCESS(rv, rv);

    // Determine if this is the first time that this cache entry
    // has been accessed during this session.
    PRBool fromPreviousSession =
            (gHttpHandler->SessionStartTime() > lastModifiedTime);

    // Get the cached HTTP response headers
    rv = mCacheEntry->GetMetaDataElement("response-head", getter_Copies(buf));
    NS_ENSURE_SUCCESS(rv, rv);

    // Parse the cached HTTP response headers
    mCachedResponseHead = new nsHttpResponseHead();
    if (!mCachedResponseHead)
        return NS_ERROR_OUT_OF_MEMORY;
    rv = mCachedResponseHead->Parse((char *) buf.get());
    NS_ENSURE_SUCCESS(rv, rv);
    buf.Adopt(0);

    // Don't bother to validate items that are read-only,
    // unless they are read-only because of INHIBIT_CACHING or because
    // we're updating the offline cache.
    // Don't bother to validate if this is a fallback entry.
    if (!mCacheForOfflineUse &&
        (mLoadedFromApplicationCache ||
         (mCacheAccess == nsICache::ACCESS_READ &&
          !(mLoadFlags & INHIBIT_CACHING)) ||
         mFallbackChannel)) {
        mCachedContentIsValid = PR_TRUE;
        return NS_OK;
    }

    PRUint16 isCachedRedirect = mCachedResponseHead->Status()/100 == 3;

    mCustomConditionalRequest =
        mRequestHead.PeekHeader(nsHttp::If_Modified_Since) ||
        mRequestHead.PeekHeader(nsHttp::If_None_Match) ||
        mRequestHead.PeekHeader(nsHttp::If_Unmodified_Since) ||
        mRequestHead.PeekHeader(nsHttp::If_Match) ||
        mRequestHead.PeekHeader(nsHttp::If_Range);

    if (method != nsHttp::Head && !isCachedRedirect) {
        // If the cached content-length is set and it does not match the data
        // size of the cached content, then the cached response is partial...
        // either we need to issue a byte range request or we need to refetch
        // the entire document.
        PRInt64 contentLength = mCachedResponseHead->ContentLength();
        if (contentLength != PRInt64(-1)) {
            PRUint32 size;
            rv = mCacheEntry->GetDataSize(&size);
            NS_ENSURE_SUCCESS(rv, rv);

            if (PRInt64(size) != contentLength) {
                LOG(("Cached data size does not match the Content-Length header "
                     "[content-length=%lld size=%u]\n", PRInt64(contentLength), size));

                PRBool hasContentEncoding =
                    mCachedResponseHead->PeekHeader(nsHttp::Content_Encoding)
                    != nsnull;
                if ((PRInt64(size) < contentLength) &&
                     size > 0 &&
                     !hasContentEncoding &&
                     mCachedResponseHead->IsResumable() &&
                     !mCustomConditionalRequest &&
                     !mCachedResponseHead->NoStore()) {
                    // looks like a partial entry we can reuse
                    rv = SetupByteRangeRequest(size);
                    NS_ENSURE_SUCCESS(rv, rv);
                    mCachedContentIsPartial = PR_TRUE;
                }
                return NS_OK;
            }
        }
    }

    PRBool doValidation = PR_FALSE;
    PRBool canAddImsHeader = PR_TRUE;

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
        canAddImsHeader = PR_FALSE;
        doValidation = PR_TRUE;
    }
    
    else if (MustValidateBasedOnQueryUrl()) {
        LOG(("Validating based on RFC 2616 section 13.9 "
             "(query-url w/o explicit expiration-time)\n"));
        doValidation = PR_TRUE;
    }
    // Check if the cache entry has expired...
    else {
        PRUint32 time = 0; // a temporary variable for storing time values...

        rv = mCacheEntry->GetExpirationTime(&time);
        NS_ENSURE_SUCCESS(rv, rv);

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
            NS_ENSURE_SUCCESS(rv, rv);

            if (time == 0)
                doValidation = PR_TRUE;
            else
                doValidation = fromPreviousSession;
        }
        else
            doValidation = PR_TRUE;

        LOG(("%salidating based on expiration time\n", doValidation ? "V" : "Not v"));
    }

    if (!doValidation && mRequestHead.PeekHeader(nsHttp::If_Match) &&
        (method == nsHttp::Get || method == nsHttp::Head)) {
        const char *requestedETag, *cachedETag;
        cachedETag = mCachedResponseHead->PeekHeader(nsHttp::ETag);
        requestedETag = mRequestHead.PeekHeader(nsHttp::If_Match);
        if (cachedETag && (!strncmp(cachedETag, "W/", 2) ||
            strcmp(requestedETag, cachedETag))) {
            // User has defined If-Match header, if the cached entry is not 
            // matching the provided header value or the cached ETag is weak,
            // force validation.
            doValidation = PR_TRUE;
        }
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

    // Bug #561276: We maintain a chain of cache-keys which returns cached
    // 3xx-responses (redirects) in order to detect cycles. If a cycle is
    // found, ignore the cached response and hit the net. Otherwise, use
    // the cached response and add the cache-key to the chain. Note that
    // a limited number of redirects (cached or not) is allowed and is
    // enforced independently of this mechanism
    if (!doValidation && isCachedRedirect) {
        nsCAutoString cacheKey;
        GenerateCacheKey(mPostID, cacheKey);

        if (!mRedirectedCachekeys)
            mRedirectedCachekeys = new nsTArray<nsCString>();
        else if (mRedirectedCachekeys->Contains(cacheKey))
            doValidation = PR_TRUE;

        LOG(("Redirection-chain %s key %s\n",
             doValidation ? "contains" : "does not contain", cacheKey.get()));

        // Append cacheKey if not in the chain already
        if (!doValidation)
            mRedirectedCachekeys->AppendElement(cacheKey);
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
        // do not override conditional headers when consumer has defined its own
        if (!mCachedResponseHead->NoStore() &&
            (mRequestHead.Method() == nsHttp::Get ||
             mRequestHead.Method() == nsHttp::Head) &&
             !mCustomConditionalRequest) {
            const char *val;
            // Add If-Modified-Since header if a Last-Modified was given
            // and we are allowed to do this (see bugs 510359 and 269303)
            if (canAddImsHeader) {
                val = mCachedResponseHead->PeekHeader(nsHttp::Last_Modified);
                if (val)
                    mRequestHead.SetHeader(nsHttp::If_Modified_Since,
                                           nsDependentCString(val));
            }
            // Add If-None-Match header if an ETag was given in the response
            val = mCachedResponseHead->PeekHeader(nsHttp::ETag);
            if (val)
                mRequestHead.SetHeader(nsHttp::If_None_Match,
                                       nsDependentCString(val));
        }

        // We don't need this info anymore
        CleanRedirectCacheChainIfNecessary();
    }

    LOG(("nsHTTPChannel::CheckCache exit [this=%p doValidation=%d]\n", this, doValidation));
    return NS_OK;
}

PRBool
nsHttpChannel::MustValidateBasedOnQueryUrl()
{
    // RFC 2616, section 13.9 states that GET-requests with a query-url
    // MUST NOT be treated as fresh unless the server explicitly provides
    // an expiration-time in the response. See bug #468594
    // Section 13.2.1 (6th paragraph) defines "explicit expiration time"
    if (mRequestHead.Method() == nsHttp::Get)
    {
        nsCAutoString query;
        nsCOMPtr<nsIURL> url = do_QueryInterface(mURI);
        nsresult rv = url->GetQuery(query);
        if (NS_SUCCEEDED(rv) && !query.IsEmpty()) {
            PRUint32 tmp; // we don't need the value, just whether it's set
            rv = mCachedResponseHead->GetExpiresValue(&tmp);
            if (NS_FAILED(rv)) {
                rv = mCachedResponseHead->GetMaxAgeValue(&tmp);
                if (NS_FAILED(rv)) {
                    return PR_TRUE;
                }
            }
        }
    }
    return PR_FALSE;
}


nsresult
nsHttpChannel::ShouldUpdateOfflineCacheEntry(PRBool *shouldCacheForOfflineUse)
{
    *shouldCacheForOfflineUse = PR_FALSE;

    if (!mOfflineCacheEntry) {
        return NS_OK;
    }

    // if we're updating the cache entry, update the offline cache entry too
    if (mCacheEntry && (mCacheAccess & nsICache::ACCESS_WRITE)) {
        *shouldCacheForOfflineUse = PR_TRUE;
        return NS_OK;
    }

    // if there's nothing in the offline cache, add it
    if (mOfflineCacheEntry && (mOfflineCacheAccess == nsICache::ACCESS_WRITE)) {
        *shouldCacheForOfflineUse = PR_TRUE;
        return NS_OK;
    }

    // if the document is newer than the offline entry, update it
    PRUint32 docLastModifiedTime;
    nsresult rv = mResponseHead->GetLastModifiedValue(&docLastModifiedTime);
    if (NS_FAILED(rv)) {
        *shouldCacheForOfflineUse = PR_TRUE;
        return NS_OK;
    }

    PRUint32 offlineLastModifiedTime;
    rv = mOfflineCacheEntry->GetLastModified(&offlineLastModifiedTime);
    NS_ENSURE_SUCCESS(rv, rv);

    if (docLastModifiedTime > offlineLastModifiedTime) {
        *shouldCacheForOfflineUse = PR_TRUE;
        return NS_OK;
    }

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

    LOG(("nsHttpChannel::ReadFromCache [this=%p] "
         "Using cached copy of: %s\n", this, mSpec.get()));

    if (mCachedResponseHead)
        mResponseHead = mCachedResponseHead;

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
        // if offline caching has been requested and the offline cache needs
        // updating, complete the call even if the main cache entry is
        // up-to-date
        PRBool shouldUpdateOffline;
        if (!mCacheForOfflineUse ||
            NS_FAILED(ShouldUpdateOfflineCacheEntry(&shouldUpdateOffline)) ||
            !shouldUpdateOffline) {

            LOG(("skipping read from cache based on LOAD_ONLY_IF_MODIFIED "
                 "load flag\n"));
            return AsyncCall(&nsHttpChannel::HandleAsyncNotModified);
        }
    }

    // open input stream for reading...
    nsCOMPtr<nsIInputStream> stream;
    rv = mCacheEntry->OpenInputStream(0, getter_AddRefs(stream));
    if (NS_FAILED(rv)) return rv;

    rv = nsInputStreamPump::Create(getter_AddRefs(mCachePump),
                                   stream, PRInt64(-1), PRInt64(-1), 0, 0,
                                   PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    rv = mCachePump->AsyncRead(this, mListenerContext);
    if (NS_FAILED(rv)) return rv;

    PRUint32 suspendCount = mSuspendCount;
    while (suspendCount--)
        mCachePump->Suspend();

    return NS_OK;
}

void
nsHttpChannel::CloseCacheEntry(PRBool doomOnFailure)
{
    if (!mCacheEntry)
        return;

    LOG(("nsHttpChannel::CloseCacheEntry [this=%p] mStatus=%x mCacheAccess=%x",
         this, mStatus, mCacheAccess));

    // If we have begun to create or replace a cache entry, and that cache
    // entry is not complete and not resumable, then it needs to be doomed.
    // Otherwise, CheckCache will make the mistake of thinking that the
    // partial cache entry is complete.

    PRBool doom = PR_FALSE;
    if (mInitedCacheEntry) {
        NS_ASSERTION(mResponseHead, "oops");
        if (NS_FAILED(mStatus) && doomOnFailure &&
            (mCacheAccess & nsICache::ACCESS_WRITE) &&
            !mResponseHead->IsResumable())
            doom = PR_TRUE;
    }
    else if (mCacheAccess == nsICache::ACCESS_WRITE)
        doom = PR_TRUE;

    if (doom) {
        LOG(("  dooming cache entry!!"));
        mCacheEntry->Doom();
    }

    mCachedResponseHead = nsnull;

    mCachePump = 0;
    mCacheEntry = 0;
    mCacheAccess = 0;
    mInitedCacheEntry = PR_FALSE;
}


void
nsHttpChannel::CloseOfflineCacheEntry()
{
    if (!mOfflineCacheEntry)
        return;

    LOG(("nsHttpChannel::CloseOfflineCacheEntry [this=%p]", this));

    if (NS_FAILED(mStatus)) {
        mOfflineCacheEntry->Doom();
    }
    else {
        PRBool succeeded;
        if (NS_SUCCEEDED(GetRequestSucceeded(&succeeded)) && !succeeded)
            mOfflineCacheEntry->Doom();
    }

    mOfflineCacheEntry = 0;
    mOfflineCacheAccess = 0;

    if (mCachingOpportunistically) {
        nsCOMPtr<nsIApplicationCacheService> appCacheService =
            do_GetService(NS_APPLICATIONCACHESERVICE_CONTRACTID);
        if (appCacheService) {
            nsCAutoString cacheKey;
            GenerateCacheKey(mPostID, cacheKey);
            appCacheService->CacheOpportunistically(mApplicationCache,
                                                    cacheKey);
        }
    }
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
    // if only reading, nothing to be done here.
    if (mCacheAccess == nsICache::ACCESS_READ)
        return NS_OK;

    // Don't cache the response again if already cached...
    if (mCachedContentIsValid)
        return NS_OK;

    LOG(("nsHttpChannel::InitCacheEntry [this=%p entry=%p]\n",
        this, mCacheEntry.get()));

    // The no-store directive within the 'Cache-Control:' header indicates
    // that we must not store the response in a persistent cache.
    if (mResponseHead->NoStore())
        mLoadFlags |= INHIBIT_PERSISTENT_CACHING;

    // Only cache SSL content on disk if the pref is set
    if (!gHttpHandler->IsPersistentHttpsCachingEnabled() &&
        mConnectionInfo->UsingSSL())
        mLoadFlags |= INHIBIT_PERSISTENT_CACHING;

    if (mLoadFlags & INHIBIT_PERSISTENT_CACHING) {
        rv = mCacheEntry->SetStoragePolicy(nsICache::STORE_IN_MEMORY);
        if (NS_FAILED(rv)) return rv;
    }

    // Set the expiration time for this cache entry
    rv = UpdateExpirationTime();
    if (NS_FAILED(rv)) return rv;

    rv = AddCacheEntryHeaders(mCacheEntry);
    if (NS_FAILED(rv)) return rv;

    mInitedCacheEntry = PR_TRUE;
    return NS_OK;
}


nsresult
nsHttpChannel::InitOfflineCacheEntry()
{
    // This function can be called even when we fail to connect (bug 551990)

    if (!mOfflineCacheEntry) {
        return NS_OK;
    }

    if (mResponseHead && mResponseHead->NoStore()) {
        CloseOfflineCacheEntry();

        return NS_OK;
    }

    // This entry's expiration time should match the main entry's expiration
    // time.  UpdateExpirationTime() will keep it in sync once the offline
    // cache entry has been created.
    if (mCacheEntry) {
        PRUint32 expirationTime;
        nsresult rv = mCacheEntry->GetExpirationTime(&expirationTime);
        NS_ENSURE_SUCCESS(rv, rv);

        mOfflineCacheEntry->SetExpirationTime(expirationTime);
    }

    return AddCacheEntryHeaders(mOfflineCacheEntry);
}


nsresult
nsHttpChannel::AddCacheEntryHeaders(nsICacheEntryDescriptor *entry)
{
    nsresult rv;

    LOG(("nsHttpChannel::AddCacheEntryHeaders [this=%x] begin", this));
    // Store secure data in memory only
    if (mSecurityInfo)
        entry->SetSecurityInfo(mSecurityInfo);

    // Store the HTTP request method with the cache entry so we can distinguish
    // for example GET and HEAD responses.
    rv = entry->SetMetaDataElement("request-method",
                                   mRequestHead.Method().get());
    if (NS_FAILED(rv)) return rv;

    // Store the HTTP authorization scheme used if any...
    rv = StoreAuthorizationMetaData(entry);
    if (NS_FAILED(rv)) return rv;

    // Iterate over the headers listed in the Vary response header, and
    // store the value of the corresponding request header so we can verify
    // that it has not varied when we try to re-use the cached response at
    // a later time.  Take care to store "Cookie" headers only as hashes
    // due to security considerations and the fact that they can be pretty
    // large (bug 468426). We take care of "Vary: cookie" in ResponseWouldVary.
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
                LOG(("nsHttpChannel::AddCacheEntryHeaders [this=%x] " \
                        "processing %s", this, token));
                if (*token != '*') {
                    nsHttpAtom atom = nsHttp::ResolveAtom(token);
                    const char *val = mRequestHead.PeekHeader(atom);
                    nsCAutoString hash;
                    if (val) {
                        // If cookie-header, store a hash of the value
                        if (atom == nsHttp::Cookie) {
                            LOG(("nsHttpChannel::AddCacheEntryHeaders [this=%x] " \
                                    "cookie-value %s", this, val));
                            rv = Hash(val, hash);
                            // If hash failed, store a string not very likely
                            // to be the result of subsequent hashes
                            if (NS_FAILED(rv))
                                val = "<hash failed>";
                            else
                                val = hash.get();

                            LOG(("   hashed to %s\n", val));
                        }

                        // build cache meta data key and set meta data element...
                        metaKey = prefix + nsDependentCString(token);
                        entry->SetMetaDataElement(metaKey.get(), val);
                    } else {
                        LOG(("nsHttpChannel::AddCacheEntryHeaders [this=%x] " \
                                "clearing metadata for %s", this, token));
                        metaKey = prefix + nsDependentCString(token);
                        entry->SetMetaDataElement(metaKey.get(), nsnull);
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
    rv = entry->SetMetaDataElement("response-head", head.get());

    return rv;
}

inline void
GetAuthType(const char *challenge, nsCString &authType)
{
    const char *p;

    // get the challenge type
    if ((p = strchr(challenge, ' ')) != nsnull)
        authType.Assign(challenge, p - challenge);
    else
        authType.Assign(challenge);
}

nsresult
nsHttpChannel::StoreAuthorizationMetaData(nsICacheEntryDescriptor *entry)
{
    // Not applicable to proxy authorization...
    const char *val = mRequestHead.PeekHeader(nsHttp::Authorization);
    if (!val)
        return NS_OK;

    // eg. [Basic realm="wally world"]
    nsCAutoString buf;
    GetAuthType(val, buf);
    return entry->SetMetaDataElement("auth", buf.get());
}

// Finalize the cache entry
//  - may need to rewrite response headers if any headers changed
//  - may need to recalculate the expiration time if any headers changed
//  - called only for freshly written cache entries
nsresult
nsHttpChannel::FinalizeCacheEntry()
{
    LOG(("nsHttpChannel::FinalizeCacheEntry [this=%p]\n", this));

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

    nsCOMPtr<nsICacheService> serv =
        do_GetService(NS_CACHESERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIEventTarget> cacheIOTarget;
    serv->GetCacheIOTarget(getter_AddRefs(cacheIOTarget));

    nsCacheStoragePolicy policy;
    rv = mCacheEntry->GetStoragePolicy(&policy);

    if (NS_FAILED(rv) || policy == nsICache::STORE_ON_DISK_AS_FILE ||
        !cacheIOTarget) {
        LOG(("nsHttpChannel::InstallCacheListener sync tee %p rv=%x policy=%d "
             "cacheIOTarget=%p", tee.get(), rv, policy, cacheIOTarget.get()));
        rv = tee->Init(mListener, out, nsnull);
    } else {
        LOG(("nsHttpChannel::InstallCacheListener async tee %p", tee.get()));
        rv = tee->InitAsync(mListener, cacheIOTarget, out, nsnull);
    }

    if (NS_FAILED(rv)) return rv;
    mListener = tee;
    return NS_OK;
}

nsresult
nsHttpChannel::InstallOfflineCacheListener()
{
    nsresult rv;

    LOG(("Preparing to write data into the offline cache [uri=%s]\n",
         mSpec.get()));

    NS_ASSERTION(mOfflineCacheEntry, "no offline cache entry");
    NS_ASSERTION(mListener, "no listener");

    nsCOMPtr<nsIOutputStream> out;
    rv = mOfflineCacheEntry->OpenOutputStream(0, getter_AddRefs(out));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIStreamListenerTee> tee =
        do_CreateInstance(kStreamListenerTeeCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = tee->Init(mListener, out, nsnull);
    if (NS_FAILED(rv)) return rv;

    mListener = tee;

    return NS_OK;
}

void
nsHttpChannel::ClearBogusContentEncodingIfNeeded()
{
    // For .gz files, apache sends both a Content-Type: application/x-gzip
    // as well as Content-Encoding: gzip, which is completely wrong.  In
    // this case, we choose to ignore the rogue Content-Encoding header. We
    // must do this early on so as to prevent it from being seen up stream.
    // The same problem exists for Content-Encoding: compress in default
    // Apache installs.
    if (mResponseHead->HasHeaderValue(nsHttp::Content_Encoding, "gzip") && (
        mResponseHead->ContentType().EqualsLiteral(APPLICATION_GZIP) ||
        mResponseHead->ContentType().EqualsLiteral(APPLICATION_GZIP2) ||
        mResponseHead->ContentType().EqualsLiteral(APPLICATION_GZIP3))) {
        // clear the Content-Encoding header
        mResponseHead->ClearHeader(nsHttp::Content_Encoding);
    }
    else if (mResponseHead->HasHeaderValue(nsHttp::Content_Encoding, "compress") && (
             mResponseHead->ContentType().EqualsLiteral(APPLICATION_COMPRESS) ||
             mResponseHead->ContentType().EqualsLiteral(APPLICATION_COMPRESS2))) {
        // clear the Content-Encoding header
        mResponseHead->ClearHeader(nsHttp::Content_Encoding);
    }
}

//-----------------------------------------------------------------------------
// nsHttpChannel <redirect>
//-----------------------------------------------------------------------------

nsresult
nsHttpChannel::SetupReplacementChannel(nsIURI       *newURI, 
                                       nsIChannel   *newChannel,
                                       PRBool        preserveMethod)
{
    LOG(("nsHttpChannel::SetupReplacementChannel "
         "[this=%p newChannel=%p preserveMethod=%d]",
         this, newChannel, preserveMethod));

    nsresult rv = HttpBaseChannel::SetupReplacementChannel(newURI, newChannel, preserveMethod);
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(newChannel);
    if (!httpChannel)
        return NS_OK; // no other options to set

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
nsHttpChannel::AsyncProcessRedirection(PRUint32 redirectType)
{
    LOG(("nsHttpChannel::AsyncProcessRedirection [this=%p type=%u]\n",
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

    mRedirectType = redirectType;

    LOG(("redirecting to: %s [redirection-limit=%u]\n",
        location, PRUint32(mRedirectionLimit)));

    nsresult rv;

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

    rv = ioService->NewURI(nsDependentCString(location),
                           originCharset.get(),
                           mURI,
                           getter_AddRefs(mRedirectURI));
    if (NS_FAILED(rv)) return rv;

    if (mApplicationCache) {
        // if we are redirected to a different origin check if there is a fallback
        // cache entry to fall back to. we don't care about file strict 
        // checking, at least mURI is not a file URI.
        if (!NS_SecurityCompareURIs(mURI, mRedirectURI, PR_FALSE)) {
            PushRedirectAsyncFunc(&nsHttpChannel::ContinueProcessRedirectionAfterFallback);
            PRBool waitingForRedirectCallback;
            (void)ProcessFallback(&waitingForRedirectCallback);
            if (waitingForRedirectCallback)
                return NS_OK;
            PopRedirectAsyncFunc(&nsHttpChannel::ContinueProcessRedirectionAfterFallback);
        }
    }

    return ContinueProcessRedirectionAfterFallback(NS_OK);
}

nsresult
nsHttpChannel::ContinueProcessRedirectionAfterFallback(nsresult rv)
{
    if (NS_SUCCEEDED(rv) && mFallingBack) {
        // do not continue with redirect processing, fallback is in
        // progress now.
        return NS_OK;
    }

    // Kill the current cache entry if we are redirecting
    // back to ourself.
    PRBool redirectingBackToSameURI = PR_FALSE;
    if (mCacheEntry && (mCacheAccess & nsICache::ACCESS_WRITE) &&
        NS_SUCCEEDED(mURI->Equals(mRedirectURI, &redirectingBackToSameURI)) &&
        redirectingBackToSameURI)
            mCacheEntry->Doom();

    // move the reference of the old location to the new one if the new
    // one has none.
    nsCOMPtr<nsIURL> newURL = do_QueryInterface(mRedirectURI);
    if (newURL) {
        nsCAutoString ref;
        rv = newURL->GetRef(ref);
        if (NS_SUCCEEDED(rv) && ref.IsEmpty()) {
            nsCOMPtr<nsIURL> baseURL(do_QueryInterface(mURI));
            if (baseURL) {
                baseURL->GetRef(ref);
                if (!ref.IsEmpty())
                    newURL->SetRef(ref);
            }
        }
    }

    // if we need to re-send POST data then be sure to ask the user first.
    PRBool preserveMethod = (mRedirectType == 307);
    if (preserveMethod && mUploadStream) {
        rv = PromptTempRedirect();
        if (NS_FAILED(rv)) return rv;
    }

    nsCOMPtr<nsIIOService> ioService;
    rv = gHttpHandler->GetIOService(getter_AddRefs(ioService));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIChannel> newChannel;
    rv = ioService->NewChannelFromURI(mRedirectURI, getter_AddRefs(newChannel));
    if (NS_FAILED(rv)) return rv;

    rv = SetupReplacementChannel(mRedirectURI, newChannel, preserveMethod);
    if (NS_FAILED(rv)) return rv;

    PRUint32 redirectFlags;
    if (mRedirectType == 301) // Moved Permanently
        redirectFlags = nsIChannelEventSink::REDIRECT_PERMANENT;
    else
        redirectFlags = nsIChannelEventSink::REDIRECT_TEMPORARY;

    // verify that this is a legal redirect
    mRedirectChannel = newChannel;

    PushRedirectAsyncFunc(&nsHttpChannel::ContinueProcessRedirection);
    rv = gHttpHandler->AsyncOnChannelRedirect(this, newChannel, redirectFlags);

    if (NS_SUCCEEDED(rv))
        rv = WaitForRedirectCallback();

    if (NS_FAILED(rv)) {
        AutoRedirectVetoNotifier notifier(this);
        PopRedirectAsyncFunc(&nsHttpChannel::ContinueProcessRedirection);
    }

    return rv;
}

nsresult
nsHttpChannel::ContinueProcessRedirection(nsresult rv)
{
    AutoRedirectVetoNotifier notifier(this);

    LOG(("ContinueProcessRedirection [rv=%x]\n", rv));
    if (NS_FAILED(rv))
        return rv;

    NS_PRECONDITION(mRedirectChannel, "No redirect channel?");

    // Make sure to do this _after_ calling OnChannelRedirect
    mRedirectChannel->SetOriginalURI(mOriginalURI);

    // And now, the deprecated way
    nsCOMPtr<nsIHttpEventSink> httpEventSink;
    GetCallback(httpEventSink);
    if (httpEventSink) {
        // NOTE: nsIHttpEventSink is only used for compatibility with pre-1.8
        // versions.
        rv = httpEventSink->OnRedirect(this, mRedirectChannel);
        if (NS_FAILED(rv))
            return rv;
    }
    // XXX we used to talk directly with the script security manager, but that
    // should really be handled by the event sink implementation.

    // begin loading the new channel
    rv = mRedirectChannel->AsyncOpen(mListener, mListenerContext);

    if (NS_FAILED(rv))
        return rv;

    // close down this channel
    Cancel(NS_BINDING_REDIRECTED);
    
    notifier.RedirectSucceeded();

    // disconnect from our listener
    mListener = 0;
    mListenerContext = 0;

    // and from our callbacks
    mCallbacks = nsnull;
    mProgressSink = nsnull;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpChannel <auth>
//-----------------------------------------------------------------------------

NS_IMETHODIMP nsHttpChannel::OnAuthAvailable()
{
    LOG(("nsHttpChannel::OnAuthAvailable [this=%p]", this));

    // setting mAuthRetryPending flag and resuming the transaction
    // triggers process of throwing away the unauthenticated data already
    // coming from the network
    mAuthRetryPending = PR_TRUE;
    LOG(("Resuming the transaction, we got credentials from user"));
    mTransactionPump->Resume();
  
    return NS_OK;
}

NS_IMETHODIMP nsHttpChannel::OnAuthCancelled(PRBool userCancel)
{
    LOG(("nsHttpChannel::OnAuthCancelled [this=%p]", this));

    if (mTransactionPump) {
        // ensure call of OnStartRequest of the current listener here,
        // it would not be called otherwise at all
        nsresult rv = CallOnStartRequest();

        // drop mAuthRetryPending flag and resume the transaction
        // this resumes load of the unauthenticated content data
        mAuthRetryPending = PR_FALSE;
        LOG(("Resuming the transaction, user cancelled the auth dialog"));
        mTransactionPump->Resume();

        if (NS_FAILED(rv))
            mTransactionPump->Cancel(rv);
    }
    
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ADDREF_INHERITED(nsHttpChannel, HttpBaseChannel)
NS_IMPL_RELEASE_INHERITED(nsHttpChannel, HttpBaseChannel)

NS_INTERFACE_MAP_BEGIN(nsHttpChannel)
    NS_INTERFACE_MAP_ENTRY(nsIRequest)
    NS_INTERFACE_MAP_ENTRY(nsIChannel)
    NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
    NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
    NS_INTERFACE_MAP_ENTRY(nsIHttpChannel)
    NS_INTERFACE_MAP_ENTRY(nsICacheInfoChannel)
    NS_INTERFACE_MAP_ENTRY(nsICachingChannel)
    NS_INTERFACE_MAP_ENTRY(nsIUploadChannel)
    NS_INTERFACE_MAP_ENTRY(nsIUploadChannel2)
    NS_INTERFACE_MAP_ENTRY(nsICacheListener)
    NS_INTERFACE_MAP_ENTRY(nsIHttpChannelInternal)
    NS_INTERFACE_MAP_ENTRY(nsIResumableChannel)
    NS_INTERFACE_MAP_ENTRY(nsITransportEventSink)
    NS_INTERFACE_MAP_ENTRY(nsISupportsPriority)
    NS_INTERFACE_MAP_ENTRY(nsIProtocolProxyCallback)
    NS_INTERFACE_MAP_ENTRY(nsIProxiedChannel)
    NS_INTERFACE_MAP_ENTRY(nsIHttpAuthenticableChannel)
    NS_INTERFACE_MAP_ENTRY(nsITraceableChannel)
    NS_INTERFACE_MAP_ENTRY(nsIApplicationCacheContainer)
    NS_INTERFACE_MAP_ENTRY(nsIApplicationCacheChannel)
    NS_INTERFACE_MAP_ENTRY(nsIAsyncVerifyRedirectCallback)
NS_INTERFACE_MAP_END_INHERITING(HttpBaseChannel)

//-----------------------------------------------------------------------------
// nsHttpChannel::nsIRequest
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::Cancel(nsresult status)
{
    LOG(("nsHttpChannel::Cancel [this=%p status=%x]\n", this, status));
    if (mCanceled) {
        LOG(("  ignoring; already canceled\n"));
        return NS_OK;
    }
    if (mWaitingForRedirectCallback) {
        LOG(("channel canceled during wait for redirect callback"));
    }
    mCanceled = PR_TRUE;
    mStatus = status;
    if (mProxyRequest)
        mProxyRequest->Cancel(status);
    if (mTransaction)
        gHttpHandler->CancelTransaction(mTransaction, status);
    if (mTransactionPump)
        mTransactionPump->Cancel(status);
    if (mCachePump)
        mCachePump->Cancel(status);
    if (mAuthProvider)
        mAuthProvider->Cancel(status);
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::Suspend()
{
    NS_ENSURE_TRUE(mIsPending, NS_ERROR_NOT_AVAILABLE);
    
    LOG(("nsHttpChannel::Suspend [this=%p]\n", this));

    ++mSuspendCount;

    if (mTransactionPump)
        return mTransactionPump->Suspend();
    if (mCachePump)
        return mCachePump->Suspend();

    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::Resume()
{
    NS_ENSURE_TRUE(mSuspendCount > 0, NS_ERROR_UNEXPECTED);
    
    LOG(("nsHttpChannel::Resume [this=%p]\n", this));
        
    if (--mSuspendCount == 0 && mPendingAsyncCallOnResume) {
        nsresult rv = AsyncCall(mPendingAsyncCallOnResume);
        mPendingAsyncCallOnResume = nsnull;
        NS_ENSURE_SUCCESS(rv, rv);
    }

    if (mTransactionPump)
        return mTransactionPump->Resume();
    if (mCachePump)
        return mCachePump->Resume();

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsIChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::GetSecurityInfo(nsISupports **securityInfo)
{
    NS_ENSURE_ARG_POINTER(securityInfo);
    *securityInfo = mSecurityInfo;
    NS_IF_ADDREF(*securityInfo);
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::AsyncOpen(nsIStreamListener *listener, nsISupports *context)
{
    LOG(("nsHttpChannel::AsyncOpen [this=%p]\n", this));

    NS_ENSURE_ARG_POINTER(listener);
    NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);
    NS_ENSURE_TRUE(!mWasOpened, NS_ERROR_ALREADY_OPENED);

    nsresult rv;

    if (mCanceled)
        return mStatus;

    rv = NS_CheckPortSafety(mURI);
    if (NS_FAILED(rv))
        return rv;

    if (!(mConnectionInfo && mConnectionInfo->UsingHttpProxy())) {
        // Start a DNS lookup very early in case the real open is queued the DNS can 
        // happen in parallel. Do not do so in the presence of an HTTP proxy as 
        // all lookups other than for the proxy itself are done by the proxy.
        nsRefPtr<nsDNSPrefetch> prefetch = new nsDNSPrefetch(mURI);
        if (prefetch) {
            prefetch->PrefetchHigh();
        }
    }
    
    // Remember the cookie header that was set, if any
    const char *cookieHeader = mRequestHead.PeekHeader(nsHttp::Cookie);
    if (cookieHeader) {
        mUserSetCookieHeader = cookieHeader;
    }

    AddCookiesToRequest();

    // notify "http-on-modify-request" observers
    gHttpHandler->OnModifyRequest(this);

    // Adjust mCaps according to our request headers:
    //  - If "Connection: close" is set as a request header, then do not bother
    //    trying to establish a keep-alive connection.
    if (mRequestHead.HasHeaderValue(nsHttp::Connection, "close"))
        mCaps &= ~(NS_HTTP_ALLOW_KEEPALIVE | NS_HTTP_ALLOW_PIPELINING);
    
    if ((mLoadFlags & VALIDATE_ALWAYS) || 
        (BYPASS_LOCAL_CACHE(mLoadFlags)))
        mCaps |= NS_HTTP_REFRESH_DNS;

    mIsPending = PR_TRUE;
    mWasOpened = PR_TRUE;

    mListener = listener;
    mListenerContext = context;

    // add ourselves to the load group.  from this point forward, we'll report
    // all failures asynchronously.
    if (mLoadGroup)
        mLoadGroup->AddRequest(this, nsnull);

    // We may have been cancelled already, either by on-modify-request
    // listeners or by load group observers; in that case, we should
    // not send the request to the server
    if (mCanceled)
        rv = mStatus;
    else
        rv = Connect();
    if (NS_FAILED(rv)) {
        LOG(("Calling AsyncAbort [rv=%x mCanceled=%i]\n", rv, mCanceled));
        CloseCacheEntry(PR_TRUE);
        AsyncAbort(rv);
    } else if (mLoadFlags & LOAD_CLASSIFY_URI) {
        nsRefPtr<nsChannelClassifier> classifier = new nsChannelClassifier();
        if (!classifier) {
            Cancel(NS_ERROR_OUT_OF_MEMORY);
            return NS_OK;
        }

        rv = classifier->Start(this);
        if (NS_FAILED(rv)) {
            Cancel(rv);
        }
    }

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsIHttpChannelInternal
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::SetupFallbackChannel(const char *aFallbackKey)
{
    LOG(("nsHttpChannel::SetupFallbackChannel [this=%x, key=%s]",
         this, aFallbackKey));
    mFallbackChannel = PR_TRUE;
    mFallbackKey = aFallbackKey;

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsISupportsPriority
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::SetPriority(PRInt32 value)
{
    PRInt16 newValue = NS_CLAMP(value, PR_INT16_MIN, PR_INT16_MAX);
    if (mPriority == newValue)
        return NS_OK;
    mPriority = newValue;
    if (mTransaction)
        gHttpHandler->RescheduleTransaction(mTransaction, mPriority);
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsIProtocolProxyCallback
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::OnProxyAvailable(nsICancelable *request, nsIURI *uri,
                                nsIProxyInfo *pi, nsresult status)
{
    mProxyRequest = nsnull;

    // If status is a failure code, then it means that we failed to resolve
    // proxy info.  That is a non-fatal error assuming it wasn't because the
    // request was canceled.  We just failover to DIRECT when proxy resolution
    // fails (failure can mean that the PAC URL could not be loaded).
    
    // Need to replace this channel with a new one.  It would be complex to try
    // to change the value of mConnectionInfo since so much of our state may
    // depend on its state.
    mTargetProxyInfo = pi;
    HandleAsyncReplaceWithProxy();
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsIProxiedChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::GetProxyInfo(nsIProxyInfo **result)
{
    if (!mConnectionInfo)
        *result = nsnull;
    else {
        *result = mConnectionInfo->ProxyInfo();
        NS_IF_ADDREF(*result);
    }
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsIHttpAuthenticableChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::GetIsSSL(PRBool *aIsSSL)
{
    *aIsSSL = mConnectionInfo->UsingSSL();
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetProxyMethodIsConnect(PRBool *aProxyMethodIsConnect)
{
    *aProxyMethodIsConnect =
        (mConnectionInfo->UsingHttpProxy() && mConnectionInfo->UsingSSL());
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetServerResponseHeader(nsACString &value)
{
    if (!mResponseHead)
        return NS_ERROR_NOT_AVAILABLE;
    return mResponseHead->GetHeader(nsHttp::Server, value);
}

NS_IMETHODIMP
nsHttpChannel::GetProxyChallenges(nsACString &value)
{
    if (!mResponseHead)
        return NS_ERROR_UNEXPECTED;
    return mResponseHead->GetHeader(nsHttp::Proxy_Authenticate, value);
}

NS_IMETHODIMP
nsHttpChannel::GetWWWChallenges(nsACString &value)
{
    if (!mResponseHead)
        return NS_ERROR_UNEXPECTED;
    return mResponseHead->GetHeader(nsHttp::WWW_Authenticate, value);
}

NS_IMETHODIMP
nsHttpChannel::SetProxyCredentials(const nsACString &value)
{
    return mRequestHead.SetHeader(nsHttp::Proxy_Authorization, value);
}

NS_IMETHODIMP
nsHttpChannel::SetWWWCredentials(const nsACString &value)
{
    return mRequestHead.SetHeader(nsHttp::Authorization, value);
}

//-----------------------------------------------------------------------------
// Methods that nsIHttpAuthenticableChannel dupes from other IDLs, which we
// get from HttpBaseChannel, must be explicitly forwarded, because C++ sucks.
//

NS_IMETHODIMP
nsHttpChannel::GetLoadFlags(nsLoadFlags *aLoadFlags)
{
    return HttpBaseChannel::GetLoadFlags(aLoadFlags);
}

NS_IMETHODIMP
nsHttpChannel::GetURI(nsIURI **aURI)
{
    return HttpBaseChannel::GetURI(aURI);
}

NS_IMETHODIMP
nsHttpChannel::GetNotificationCallbacks(nsIInterfaceRequestor **aCallbacks)
{
    return HttpBaseChannel::GetNotificationCallbacks(aCallbacks);
}

NS_IMETHODIMP
nsHttpChannel::GetLoadGroup(nsILoadGroup **aLoadGroup)
{
    return HttpBaseChannel::GetLoadGroup(aLoadGroup);
}

NS_IMETHODIMP
nsHttpChannel::GetRequestMethod(nsACString& aMethod)
{
    return HttpBaseChannel::GetRequestMethod(aMethod);
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

    LOG(("nsHttpChannel::OnStartRequest [this=%p request=%p status=%x]\n",
        this, request, mStatus));

    // Make sure things are what we expect them to be...
    NS_ASSERTION(request == mCachePump || request == mTransactionPump,
                 "Unexpected request");
    NS_ASSERTION(!(mTransactionPump && mCachePump) || mCachedContentIsPartial,
                 "If we have both pumps, the cache content must be partial");

    if (!mSecurityInfo && !mCachePump && mTransaction) {
        // grab the security info from the connection object; the transaction
        // is guaranteed to own a reference to the connection.
        mSecurityInfo = mTransaction->SecurityInfo();
    }

    // don't enter this block if we're reading from the cache...
    if (NS_SUCCEEDED(mStatus) && !mCachePump && mTransaction) {
        // all of the response headers have been acquired, so we can take ownership
        // of them from the transaction.
        mResponseHead = mTransaction->TakeResponseHead();
        // the response head may be null if the transaction was cancelled.  in
        // which case we just need to call OnStartRequest/OnStopRequest.
        if (mResponseHead)
            return ProcessResponse();

        NS_WARNING("No response head in OnStartRequest");
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

        PushRedirectAsyncFunc(&nsHttpChannel::ContinueOnStartRequest1);
        if (NS_SUCCEEDED(ProxyFailover()))
            return NS_OK;
        PopRedirectAsyncFunc(&nsHttpChannel::ContinueOnStartRequest1);
    }

    return ContinueOnStartRequest2(NS_OK);
}

nsresult
nsHttpChannel::ContinueOnStartRequest1(nsresult result)
{
    // Success indicates we passed ProxyFailover, in that case we must not continue
    // with this code chain.
    if (NS_SUCCEEDED(result))
        return NS_OK;

    return ContinueOnStartRequest2(result);
}

nsresult
nsHttpChannel::ContinueOnStartRequest2(nsresult result)
{
    // on other request errors, try to fall back
    if (NS_FAILED(mStatus)) {
        PushRedirectAsyncFunc(&nsHttpChannel::ContinueOnStartRequest3);
        PRBool waitingForRedirectCallback;
        ProcessFallback(&waitingForRedirectCallback);
        if (waitingForRedirectCallback)
            return NS_OK;
        PopRedirectAsyncFunc(&nsHttpChannel::ContinueOnStartRequest3);
    }

    return ContinueOnStartRequest3(NS_OK);
}

nsresult
nsHttpChannel::ContinueOnStartRequest3(nsresult result)
{
    if (mFallingBack)
        return NS_OK;

    return CallOnStartRequest();
}

NS_IMETHODIMP
nsHttpChannel::OnStopRequest(nsIRequest *request, nsISupports *ctxt, nsresult status)
{
    LOG(("nsHttpChannel::OnStopRequest [this=%p request=%p status=%x]\n",
        this, request, status));

     // allow content to be cached if it was loaded successfully (bug #482935)
     PRBool contentComplete = NS_SUCCEEDED(status);

    // honor the cancelation status even if the underlying transaction completed.
    if (mCanceled || NS_FAILED(mStatus))
        status = mStatus;

    if (mCachedContentIsPartial) {
        if (NS_SUCCEEDED(status)) {
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
        // Do not to leave the transaction in a suspended state in error cases.
        if (NS_FAILED(status) && mTransaction)
            gHttpHandler->CancelTransaction(mTransaction, status); 
    }

    if (mTransaction) {
        // determine if we should call DoAuthRetry
        PRBool authRetry = mAuthRetryPending && NS_SUCCEEDED(status);

        //
        // grab reference to connection in case we need to retry an
        // authentication request over it.  this applies to connection based
        // authentication schemes only.  for request based schemes, conn is not
        // needed, so it may be null.
        // 
        // this code relies on the code in nsHttpTransaction::Close, which
        // tests for NS_HTTP_STICKY_CONNECTION to determine whether or not to
        // keep the connection around after the transaction is finished.
        //
        nsRefPtr<nsAHttpConnection> conn;
        if (authRetry && (mCaps & NS_HTTP_STICKY_CONNECTION)) {
            conn = mTransaction->Connection();
            // This is so far a workaround to fix leak when reusing unpersistent
            // connection for authentication retry. See bug 459620 comment 4
            // for details.
            if (conn && !conn->IsPersistent())
                conn = nsnull;
        }

        // at this point, we're done with the transaction
        mTransaction = nsnull;
        mTransactionPump = 0;

        // handle auth retry...
        if (authRetry) {
            mAuthRetryPending = PR_FALSE;
            status = DoAuthRetry(conn);
            if (NS_SUCCEEDED(status))
                return NS_OK;
        }

        // If DoAuthRetry failed, or if we have been cancelled since showing
        // the auth. dialog, then we need to send OnStartRequest now
        if (authRetry || (mAuthRetryPending && NS_FAILED(status))) {
            NS_ASSERTION(NS_FAILED(status), "should have a failure code here");
            // NOTE: since we have a failure status, we can ignore the return
            // value from onStartRequest.
            mListener->OnStartRequest(this, mListenerContext);
        }

        // if this transaction has been replaced, then bail.
        if (mTransactionReplaced)
            return NS_OK;
    }

    mIsPending = PR_FALSE;
    mStatus = status;

    // perform any final cache operations before we close the cache entry.
    if (mCacheEntry && (mCacheAccess & nsICache::ACCESS_WRITE) &&
        mRequestTimeInitialized){
        FinalizeCacheEntry();
    }
    
    if (mListener) {
        LOG(("  calling OnStopRequest\n"));
        mListener->OnStopRequest(this, mListenerContext, status);
        mListener = 0;
        mListenerContext = 0;
    }

    if (mCacheEntry)
        CloseCacheEntry(!contentComplete);

    if (mOfflineCacheEntry)
        CloseOfflineCacheEntry();

    if (mLoadGroup)
        mLoadGroup->RemoveRequest(this, nsnull, status);

    // We don't need this info anymore
    CleanRedirectCacheChainIfNecessary();

    mCallbacks = nsnull;
    mProgressSink = nsnull;
    
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
    LOG(("nsHttpChannel::OnDataAvailable [this=%p request=%p offset=%u count=%u]\n",
        this, request, offset, count));

    // don't send out OnDataAvailable notifications if we've been canceled.
    if (mCanceled)
        return mStatus;

    NS_ASSERTION(mResponseHead, "No response head in ODA!!");

    NS_ASSERTION(!(mCachedContentIsPartial && (request == mTransactionPump)),
            "transaction pump not suspended");

    if (mAuthRetryPending || (request == mTransactionPump && mTransactionReplaced)) {
        PRUint32 n;
        return input->ReadSegments(NS_DiscardSegment, nsnull, count, &n);
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

        PRUint64 progressMax(PRUint64(mResponseHead->ContentLength()));
        PRUint64 progress = mLogicalOffset + PRUint64(count);
        NS_ASSERTION(progress <= progressMax, "unexpected progress values");

        OnTransportStatus(nsnull, transportStatus, progress, progressMax);

        //
        // we have to manually keep the logical offset of the stream up-to-date.
        // we cannot depend solely on the offset provided, since we may have 
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
                                 PRUint64 progress, PRUint64 progressMax)
{
    // cache the progress sink so we don't have to query for it each time.
    if (!mProgressSink)
        GetCallback(mProgressSink);

    // block socket status event after Cancel or OnStopRequest has been called.
    if (mProgressSink && NS_SUCCEEDED(mStatus) && mIsPending && !(mLoadFlags & LOAD_BACKGROUND)) {
        LOG(("sending status notification [this=%p status=%x progress=%llu/%llu]\n",
            this, status, progress, progressMax));

        nsCAutoString host;
        mURI->GetHost(host);
        mProgressSink->OnStatus(this, nsnull, status,
                                NS_ConvertUTF8toUTF16(host).get());

        if (progress > 0) {
            NS_ASSERTION(progress <= progressMax, "unexpected progress values");
            mProgressSink->OnProgress(this, nsnull, progress, progressMax);
        }
    }
#ifdef DEBUG
    else
        LOG(("skipping status notification [this=%p sink=%p pending=%u background=%x]\n",
            this, mProgressSink.get(), mIsPending, (mLoadFlags & LOAD_BACKGROUND)));
#endif

    return NS_OK;
} 

//-----------------------------------------------------------------------------
// nsHttpChannel::nsICacheInfoChannel
//-----------------------------------------------------------------------------

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

NS_IMETHODIMP
nsHttpChannel::GetCacheTokenExpirationTime(PRUint32 *_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    if (!mCacheEntry)
        return NS_ERROR_NOT_AVAILABLE;

    return mCacheEntry->GetExpirationTime(_retval);
}

NS_IMETHODIMP
nsHttpChannel::GetCacheTokenCachedCharset(nsACString &_retval)
{
    nsresult rv;

    if (!mCacheEntry)
        return NS_ERROR_NOT_AVAILABLE;

    nsXPIDLCString cachedCharset;
    rv = mCacheEntry->GetMetaDataElement("charset",
                                         getter_Copies(cachedCharset));
    if (NS_SUCCEEDED(rv))
        _retval = cachedCharset;

    return rv;
}

NS_IMETHODIMP
nsHttpChannel::SetCacheTokenCachedCharset(const nsACString &aCharset)
{
    if (!mCacheEntry)
        return NS_ERROR_NOT_AVAILABLE;

    return mCacheEntry->SetMetaDataElement("charset",
                                           PromiseFlatCString(aCharset).get());
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
nsHttpChannel::GetOfflineCacheToken(nsISupports **token)
{
    NS_ENSURE_ARG_POINTER(token);
    if (!mOfflineCacheEntry)
        return NS_ERROR_NOT_AVAILABLE;
    return CallQueryInterface(mOfflineCacheEntry, token);
}

NS_IMETHODIMP
nsHttpChannel::SetOfflineCacheToken(nsISupports *token)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

class nsHttpChannelCacheKey : public nsISupportsPRUint32,
                              public nsISupportsCString
{
    NS_DECL_ISUPPORTS

    NS_DECL_NSISUPPORTSPRIMITIVE
    NS_FORWARD_NSISUPPORTSPRUINT32(mSupportsPRUint32->)
    
    // Both interfaces declares toString method with the same signature.
    // Thus we have to delegate only to nsISupportsPRUint32 implementation.
    NS_SCRIPTABLE NS_IMETHOD GetData(nsACString & aData) 
    { 
        return mSupportsCString->GetData(aData);
    }
    NS_SCRIPTABLE NS_IMETHOD SetData(const nsACString & aData)
    { 
        return mSupportsCString->SetData(aData);
    }
    
public:
    nsresult SetData(PRUint32 aPostID, const nsACString& aKey);

protected:
    nsCOMPtr<nsISupportsPRUint32> mSupportsPRUint32;
    nsCOMPtr<nsISupportsCString> mSupportsCString;
};

NS_IMPL_ADDREF(nsHttpChannelCacheKey)
NS_IMPL_RELEASE(nsHttpChannelCacheKey)
NS_INTERFACE_TABLE_HEAD(nsHttpChannelCacheKey)
NS_INTERFACE_TABLE_BEGIN
NS_INTERFACE_TABLE_ENTRY_AMBIGUOUS(nsHttpChannelCacheKey,
                                   nsISupports, nsISupportsPRUint32)
NS_INTERFACE_TABLE_ENTRY_AMBIGUOUS(nsHttpChannelCacheKey,
                                   nsISupportsPrimitive, nsISupportsPRUint32)
NS_INTERFACE_TABLE_ENTRY(nsHttpChannelCacheKey,
                         nsISupportsPRUint32)
NS_INTERFACE_TABLE_ENTRY(nsHttpChannelCacheKey,
                         nsISupportsCString)
NS_INTERFACE_TABLE_END
NS_INTERFACE_TABLE_TAIL

NS_IMETHODIMP nsHttpChannelCacheKey::GetType(PRUint16 *aType)
{
    NS_ENSURE_ARG_POINTER(aType);

    *aType = TYPE_PRUINT32;
    return NS_OK;
}

nsresult nsHttpChannelCacheKey::SetData(PRUint32 aPostID,
                                        const nsACString& aKey)
{
    nsresult rv;

    mSupportsCString = 
        do_CreateInstance(NS_SUPPORTS_CSTRING_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    mSupportsCString->SetData(aKey);
    if (NS_FAILED(rv)) return rv;

    mSupportsPRUint32 = 
        do_CreateInstance(NS_SUPPORTS_PRUINT32_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    mSupportsPRUint32->SetData(aPostID);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetCacheKey(nsISupports **key)
{
    nsresult rv;
    NS_ENSURE_ARG_POINTER(key);

    LOG(("nsHttpChannel::GetCacheKey [this=%p]\n", this));

    *key = nsnull;

    nsRefPtr<nsHttpChannelCacheKey> container =
        new nsHttpChannelCacheKey();

    if (!container)
        return NS_ERROR_OUT_OF_MEMORY;

    nsCAutoString cacheKey;
    rv = GenerateCacheKey(mPostID, cacheKey);
    if (NS_FAILED(rv)) return rv;

    rv = container->SetData(mPostID, cacheKey);
    if (NS_FAILED(rv)) return rv;

    return CallQueryInterface(container.get(), key);
}

NS_IMETHODIMP
nsHttpChannel::SetCacheKey(nsISupports *key)
{
    nsresult rv;

    LOG(("nsHttpChannel::SetCacheKey [this=%p key=%p]\n", this, key));

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
nsHttpChannel::GetCacheForOfflineUse(PRBool *value)
{
    *value = mCacheForOfflineUse;

    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::SetCacheForOfflineUse(PRBool value)
{
    mCacheForOfflineUse = value;

    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetOfflineCacheClientID(nsACString &value)
{
    value = mOfflineCacheClientID;

    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::SetOfflineCacheClientID(const nsACString &value)
{
    mOfflineCacheClientID = value;

    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetCacheFile(nsIFile **cacheFile)
{
    if (!mCacheEntry)
        return NS_ERROR_NOT_AVAILABLE;
    return mCacheEntry->GetFile(cacheFile);
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsIResumableChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::ResumeAt(PRUint64 aStartPos,
                        const nsACString& aEntityID)
{
    LOG(("nsHttpChannel::ResumeAt [this=%p startPos=%llu id='%s']\n",
         this, aStartPos, PromiseFlatCString(aEntityID).get()));
    mEntityID = aEntityID;
    mStartPos = aStartPos;
    mResuming = PR_TRUE;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsICacheListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::OnCacheEntryAvailable(nsICacheEntryDescriptor *entry,
                                     nsCacheAccessMode access,
                                     nsresult status)
{
    nsresult rv;

    LOG(("nsHttpChannel::OnCacheEntryAvailable [this=%p entry=%p "
         "access=%x status=%x]\n", this, entry, access, status));

    // if the channel's already fired onStopRequest, then we should ignore
    // this event.
    if (!mIsPending)
        return NS_OK;

    nsOnCacheEntryAvailableCallback callback = mOnCacheEntryAvailableCallback;
    mOnCacheEntryAvailableCallback = nsnull;

    NS_ASSERTION(callback,
        "nsHttpChannel::OnCacheEntryAvailable called without callback");
    rv = ((*this).*callback)(entry, access, status, PR_FALSE);

    if (NS_FAILED(rv)) {
        LOG(("AsyncOpenCacheEntry failed [rv=%x]\n", rv));
        if (mLoadFlags & LOAD_ONLY_FROM_CACHE) {
            // If we have a fallback URI (and we're not already
            // falling back), process the fallback asynchronously.
            if (!mFallbackChannel && !mFallbackKey.IsEmpty()) {
                rv = AsyncCall(&nsHttpChannel::HandleAsyncFallback);
                if (NS_SUCCEEDED(rv))
                    return rv;
            }
        }
        CloseCacheEntry(PR_TRUE);
        AsyncAbort(rv);
    }

    return NS_OK;
}

nsresult
nsHttpChannel::DoAuthRetry(nsAHttpConnection *conn)
{
    LOG(("nsHttpChannel::DoAuthRetry [this=%p]\n", this));

    NS_ASSERTION(!mTransaction, "should not have a transaction");
    nsresult rv;

    // toggle mIsPending to allow nsIObserver implementations to modify
    // the request headers (bug 95044).
    mIsPending = PR_FALSE;

    // fetch cookies, and add them to the request header.
    // the server response could have included cookies that must be sent with
    // this authentication attempt (bug 84794).
    // TODO: save cookies from auth response and send them here (bug 572151).
    AddCookiesToRequest();
    
    // notify "http-on-modify-request" observers
    gHttpHandler->OnModifyRequest(this);

    mIsPending = PR_TRUE;

    // get rid of the old response headers
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

    rv = gHttpHandler->InitiateTransaction(mTransaction, mPriority);
    if (NS_FAILED(rv)) return rv;

    rv = mTransactionPump->AsyncRead(this, nsnull);
    if (NS_FAILED(rv)) return rv;

    PRUint32 suspendCount = mSuspendCount;
    while (suspendCount--)
        mTransactionPump->Suspend();

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsIApplicationCacheChannel
//-----------------------------------------------------------------------------
NS_IMETHODIMP
nsHttpChannel::GetApplicationCache(nsIApplicationCache **out)
{
    NS_IF_ADDREF(*out = mApplicationCache);
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::SetApplicationCache(nsIApplicationCache *appCache)
{
    NS_ENSURE_TRUE(!mWasOpened, NS_ERROR_ALREADY_OPENED);

    mApplicationCache = appCache;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetLoadedFromApplicationCache(PRBool *aLoadedFromApplicationCache)
{
    *aLoadedFromApplicationCache = mLoadedFromApplicationCache;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetInheritApplicationCache(PRBool *aInherit)
{
    *aInherit = mInheritApplicationCache;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::SetInheritApplicationCache(PRBool aInherit)
{
    NS_ENSURE_TRUE(!mWasOpened, NS_ERROR_ALREADY_OPENED);

    mInheritApplicationCache = aInherit;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::GetChooseApplicationCache(PRBool *aChoose)
{
    *aChoose = mChooseApplicationCache;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::SetChooseApplicationCache(PRBool aChoose)
{
    NS_ENSURE_TRUE(!mWasOpened, NS_ERROR_ALREADY_OPENED);

    mChooseApplicationCache = aChoose;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::MarkOfflineCacheEntryAsForeign()
{
    if (!mApplicationCache)
        return NS_ERROR_NOT_AVAILABLE;

    nsresult rv;

    nsCAutoString cacheKey;
    rv = GenerateCacheKey(mPostID, cacheKey);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mApplicationCache->MarkEntry(cacheKey,
                                      nsIApplicationCache::ITEM_FOREIGN);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsIAsyncVerifyRedirectCallback
//-----------------------------------------------------------------------------

nsresult
nsHttpChannel::WaitForRedirectCallback()
{
    nsresult rv;
    LOG(("nsHttpChannel::WaitForRedirectCallback [this=%p]\n", this));

    if (mTransactionPump) {
        rv = mTransactionPump->Suspend();
        NS_ENSURE_SUCCESS(rv, rv);
    }
    if (mCachePump) {
        rv = mCachePump->Suspend();
        if (NS_FAILED(rv) && mTransactionPump) {
#ifdef DEBUG
            nsresult resume = 
#endif
            mTransactionPump->Resume();
            NS_ASSERTION(NS_SUCCEEDED(resume),
                "Failed to resume transaction pump");
        }
        NS_ENSURE_SUCCESS(rv, rv);
    }

    mWaitingForRedirectCallback = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpChannel::OnRedirectVerifyCallback(nsresult result)
{
    LOG(("nsHttpChannel::OnRedirectVerifyCallback [this=%p] "
         "result=%x stack=%d mWaitingForRedirectCallback=%u\n",
         this, result, mRedirectFuncStack.Length(), mWaitingForRedirectCallback));
    NS_ASSERTION(mWaitingForRedirectCallback,
                 "Someone forgot to call WaitForRedirectCallback() ?!");
    mWaitingForRedirectCallback = PR_FALSE;

    if (mCanceled && NS_SUCCEEDED(result))
        result = NS_BINDING_ABORTED;

    for (PRUint32 i = mRedirectFuncStack.Length(); i > 0;) {
        --i;
        // Pop the last function pushed to the stack
        nsContinueRedirectionFunc func = mRedirectFuncStack[i];
        mRedirectFuncStack.RemoveElementAt(mRedirectFuncStack.Length() - 1);

        // Call it with the result we got from the callback or the deeper
        // function call.
        result = (this->*func)(result);

        // If a new function has been pushed to the stack and placed us in the
        // waiting state, we need to break the chain and wait for the callback
        // again.
        if (mWaitingForRedirectCallback)
            break;
    }

    if (NS_FAILED(result) && !mCanceled) {
        // First, cancel this channel if we are in failure state to set mStatus
        // and let it be propagated to pumps.
        Cancel(result);
    }

    if (!mWaitingForRedirectCallback) {
        // We are not waiting for the callback. At this moment we must release
        // reference to the redirect target channel, otherwise we may leak.
        mRedirectChannel = nsnull;
    }

    // We always resume the pumps here. If all functions on stack have been
    // called we need OnStopRequest to be triggered, and if we broke out of the
    // loop above (and are thus waiting for a new callback) the suspension
    // count must be balanced in the pumps.
    if (mTransactionPump)
        mTransactionPump->Resume();
    if (mCachePump)
        mCachePump->Resume();

    return result;
}

void
nsHttpChannel::PushRedirectAsyncFunc(nsContinueRedirectionFunc func)
{
    mRedirectFuncStack.AppendElement(func);
}

void
nsHttpChannel::PopRedirectAsyncFunc(nsContinueRedirectionFunc func)
{
    NS_ASSERTION(func == mRedirectFuncStack[mRedirectFuncStack.Length() - 1],
        "Trying to pop wrong method from redirect async stack!");

    mRedirectFuncStack.TruncateLength(mRedirectFuncStack.Length() - 1);
}

//-----------------------------------------------------------------------------
// nsStreamListenerWrapper <private>
//-----------------------------------------------------------------------------

// Wrapper class to make replacement of nsHttpChannel's listener
// from JavaScript possible. It is workaround for bug 433711.
class nsStreamListenerWrapper : public nsIStreamListener
{
public:
    nsStreamListenerWrapper(nsIStreamListener *listener);

    NS_DECL_ISUPPORTS
    NS_FORWARD_NSIREQUESTOBSERVER(mListener->)
    NS_FORWARD_NSISTREAMLISTENER(mListener->)

private:
    ~nsStreamListenerWrapper() {}
    nsCOMPtr<nsIStreamListener> mListener;
};

nsStreamListenerWrapper::nsStreamListenerWrapper(nsIStreamListener *listener)
    : mListener(listener) 
{
    NS_ASSERTION(mListener, "no stream listener specified");
}

NS_IMPL_ISUPPORTS2(nsStreamListenerWrapper,
                   nsIStreamListener,
                   nsIRequestObserver)

//-----------------------------------------------------------------------------
// nsHttpChannel::nsITraceableChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpChannel::SetNewListener(nsIStreamListener *aListener, nsIStreamListener **_retval)
{
    if (!mTracingEnabled)
        return NS_ERROR_FAILURE;

    NS_ENSURE_ARG_POINTER(aListener);

    nsCOMPtr<nsIStreamListener> wrapper = 
        new nsStreamListenerWrapper(mListener);

    if (!wrapper)
        return NS_ERROR_OUT_OF_MEMORY;

    wrapper.forget(_retval);
    mListener = aListener;
    return NS_OK;
}

void
nsHttpChannel::MaybeInvalidateCacheEntryForSubsequentGet()
{
    // See RFC 2616 section 5.1.1. These are considered valid
    // methods which DO NOT invalidate cache-entries for the
    // referred resource. POST, PUT and DELETE as well as any
    // other method not listed here will potentially invalidate
    // any cached copy of the resource
    if (mRequestHead.Method() == nsHttp::Options ||
       mRequestHead.Method() == nsHttp::Get ||
       mRequestHead.Method() == nsHttp::Head ||
       mRequestHead.Method() == nsHttp::Trace ||
       mRequestHead.Method() == nsHttp::Connect)
        return;
        
    // NOTE:
    // Following comments 24,32 and 33 in bug #327765, we only care about
    // the cache in the protocol-handler.
    // The logic below deviates from the original logic in OpenCacheEntry on
    // one point by using only READ_ONLY access-policy. I think this is safe.
    LOG(("MaybeInvalidateCacheEntryForSubsequentGet [this=%p]\n", this));

    nsCAutoString tmpCacheKey;
    // passing 0 in first param gives the cache-key for a GET to my resource
    GenerateCacheKey(0, tmpCacheKey);

    // Now, find the session holding the cache-entry
    nsCOMPtr<nsICacheSession> session;
    nsCacheStoragePolicy storagePolicy = DetermineStoragePolicy();

    nsresult rv;
    rv = gHttpHandler->GetCacheSession(storagePolicy,
                                       getter_AddRefs(session));

    if (NS_FAILED(rv)) return;

    // Finally, find the actual cache-entry
    nsCOMPtr<nsICacheEntryDescriptor> tmpCacheEntry;
    rv = session->OpenCacheEntry(tmpCacheKey, nsICache::ACCESS_READ,
                                 PR_FALSE,
                                 getter_AddRefs(tmpCacheEntry));
    
    // If entry was found, set its expiration-time = 0
    if(NS_SUCCEEDED(rv)) {
       tmpCacheEntry->SetExpirationTime(0);
    }
}

nsCacheStoragePolicy
nsHttpChannel::DetermineStoragePolicy()
{
    nsCacheStoragePolicy policy = nsICache::STORE_ANYWHERE;
    if (mLoadFlags & INHIBIT_PERSISTENT_CACHING)
        policy = nsICache::STORE_IN_MEMORY;

    return policy;
}

nsresult
nsHttpChannel::DetermineCacheAccess(nsCacheAccessMode *_retval)
{
    PRBool offline = gIOService->IsOffline();

    if (offline || (mLoadFlags & INHIBIT_CACHING)) {
        // If we have been asked to bypass the cache and not write to the
        // cache, then don't use the cache at all.  Unless we're actually
        // offline, which takes precedence over BYPASS_LOCAL_CACHE.
        if (BYPASS_LOCAL_CACHE(mLoadFlags) && !offline)
            return NS_ERROR_NOT_AVAILABLE;
        *_retval = nsICache::ACCESS_READ;
    }
    else if (BYPASS_LOCAL_CACHE(mLoadFlags))
        *_retval = nsICache::ACCESS_WRITE; // replace cache entry
    else
        *_retval = nsICache::ACCESS_READ_WRITE; // normal browsing

    return NS_OK;
}

void
nsHttpChannel::AsyncOnExamineCachedResponse()
{
    gHttpHandler->OnExamineCachedResponse(this);
}
