/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "mozilla/net/HttpChannelParent.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/net/NeckoParent.h"
#include "mozilla/unused.h"
#include "HttpChannelParentListener.h"
#include "nsHttpHandler.h"
#include "nsNetUtil.h"
#include "nsISupportsPriority.h"
#include "nsIAuthPromptProvider.h"
#include "nsIScriptSecurityManager.h"
#include "nsSerializationHelper.h"
#include "nsISerializable.h"
#include "nsIAssociatedContentSecurity.h"
#include "nsIApplicationCacheService.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/ipc/URIUtils.h"
#include "SerializedLoadContext.h"

using namespace mozilla::dom;
using namespace mozilla::ipc;

namespace mozilla {
namespace net {

HttpChannelParent::HttpChannelParent(PBrowserParent* iframeEmbedding,
                                     nsILoadContext* aLoadContext,
                                     PBOverrideStatus aOverrideStatus)
  : mIPCClosed(false)
  , mStoredStatus(NS_OK)
  , mStoredProgress(0)
  , mStoredProgressMax(0)
  , mSentRedirect1Begin(false)
  , mSentRedirect1BeginFailed(false)
  , mReceivedRedirect2Verify(false)
  , mPBOverride(aOverrideStatus)
  , mLoadContext(aLoadContext)
{
  // Ensure gHttpHandler is initialized: we need the atom table up and running.
  nsCOMPtr<nsIHttpProtocolHandler> dummyInitializer =
    do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http");

  MOZ_ASSERT(gHttpHandler);
  mHttpHandler = gHttpHandler;

  mTabParent = static_cast<mozilla::dom::TabParent*>(iframeEmbedding);
}

HttpChannelParent::~HttpChannelParent()
{
}

void
HttpChannelParent::ActorDestroy(ActorDestroyReason why)
{
  // We may still have refcount>0 if nsHttpChannel hasn't called OnStopRequest
  // yet, but child process has crashed.  We must not try to send any more msgs
  // to child, or IPDL will kill chrome process, too.
  mIPCClosed = true;
}

bool
HttpChannelParent::Init(const HttpChannelCreationArgs& aArgs)
{
  switch (aArgs.type()) {
  case HttpChannelCreationArgs::THttpChannelOpenArgs:
  {
    const HttpChannelOpenArgs& a = aArgs.get_HttpChannelOpenArgs();
    return DoAsyncOpen(a.uri(), a.original(), a.doc(), a.referrer(),
                       a.apiRedirectTo(), a.loadFlags(), a.requestHeaders(),
                       a.requestMethod(), a.uploadStream(),
                       a.uploadStreamHasHeaders(), a.priority(),
                       a.redirectionLimit(), a.allowPipelining(),
                       a.forceAllowThirdPartyCookie(), a.resumeAt(),
                       a.startPos(), a.entityID(), a.chooseApplicationCache(),
                       a.appCacheClientID(), a.allowSpdy());
  }
  case HttpChannelCreationArgs::THttpChannelConnectArgs:
  {
    const HttpChannelConnectArgs& cArgs = aArgs.get_HttpChannelConnectArgs();
    return ConnectChannel(cArgs.channelId());
  }
  default:
    NS_NOTREACHED("unknown open type");
    return false;
  }
}

//-----------------------------------------------------------------------------
// HttpChannelParent::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS6(HttpChannelParent,
                   nsIInterfaceRequestor,
                   nsIProgressEventSink,
                   nsIRequestObserver,
                   nsIStreamListener,
                   nsIParentChannel,
                   nsIParentRedirectingChannel)

//-----------------------------------------------------------------------------
// HttpChannelParent::nsIInterfaceRequestor
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParent::GetInterface(const nsIID& aIID, void **result)
{
  if (aIID.Equals(NS_GET_IID(nsIAuthPromptProvider)) ||
      aIID.Equals(NS_GET_IID(nsISecureBrowserUI))) {
    if (!mTabParent)
      return NS_NOINTERFACE;

    return mTabParent->QueryInterface(aIID, result);
  }

  // Only support nsILoadContext if child channel's callbacks did too
  if (aIID.Equals(NS_GET_IID(nsILoadContext)) && mLoadContext) {
    NS_ADDREF(mLoadContext);
    *result = static_cast<nsILoadContext*>(mLoadContext);
    return NS_OK;
  }

  return QueryInterface(aIID, result);
}

//-----------------------------------------------------------------------------
// HttpChannelParent::PHttpChannelParent
//-----------------------------------------------------------------------------

bool
HttpChannelParent::DoAsyncOpen(  const URIParams&           aURI,
                                 const OptionalURIParams&   aOriginalURI,
                                 const OptionalURIParams&   aDocURI,
                                 const OptionalURIParams&   aReferrerURI,
                                 const OptionalURIParams&   aAPIRedirectToURI,
                                 const uint32_t&            loadFlags,
                                 const RequestHeaderTuples& requestHeaders,
                                 const nsHttpAtom&          requestMethod,
                                 const OptionalInputStreamParams& uploadStream,
                                 const bool&              uploadStreamHasHeaders,
                                 const uint16_t&            priority,
                                 const uint8_t&             redirectionLimit,
                                 const bool&              allowPipelining,
                                 const bool&              forceAllowThirdPartyCookie,
                                 const bool&                doResumeAt,
                                 const uint64_t&            startPos,
                                 const nsCString&           entityID,
                                 const bool&                chooseApplicationCache,
                                 const nsCString&           appCacheClientID,
                                 const bool&                allowSpdy)
{
  nsCOMPtr<nsIURI> uri = DeserializeURI(aURI);
  if (!uri) {
    // URIParams does MOZ_ASSERT if null, but we need to protect opt builds from
    // null deref here.
    return false;
  }
  nsCOMPtr<nsIURI> originalUri = DeserializeURI(aOriginalURI);
  nsCOMPtr<nsIURI> docUri = DeserializeURI(aDocURI);
  nsCOMPtr<nsIURI> referrerUri = DeserializeURI(aReferrerURI);
  nsCOMPtr<nsIURI> apiRedirectToUri = DeserializeURI(aAPIRedirectToURI);

  nsCString uriSpec;
  uri->GetSpec(uriSpec);
  LOG(("HttpChannelParent RecvAsyncOpen [this=%p uri=%s]\n",
       this, uriSpec.get()));

  nsresult rv;

  nsCOMPtr<nsIIOService> ios(do_GetIOService(&rv));
  if (NS_FAILED(rv))
    return SendFailedAsyncOpen(rv);

  rv = NS_NewChannel(getter_AddRefs(mChannel), uri, ios, nullptr, nullptr, loadFlags);
  if (NS_FAILED(rv))
    return SendFailedAsyncOpen(rv);

  nsHttpChannel *httpChan = static_cast<nsHttpChannel *>(mChannel.get());
  if (mPBOverride != kPBOverride_Unset) {
    httpChan->SetPrivate(mPBOverride == kPBOverride_Private ? true : false);
  }

  if (doResumeAt)
    httpChan->ResumeAt(startPos, entityID);

  if (originalUri)
    httpChan->SetOriginalURI(originalUri);
  if (docUri)
    httpChan->SetDocumentURI(docUri);
  if (referrerUri)
    httpChan->SetReferrerInternal(referrerUri);
  if (apiRedirectToUri)
    httpChan->RedirectTo(apiRedirectToUri);
  if (loadFlags != nsIRequest::LOAD_NORMAL)
    httpChan->SetLoadFlags(loadFlags);

  for (uint32_t i = 0; i < requestHeaders.Length(); i++) {
    httpChan->SetRequestHeader(requestHeaders[i].mHeader,
                               requestHeaders[i].mValue,
                               requestHeaders[i].mMerge);
  }

  nsRefPtr<HttpChannelParentListener> channelListener =
      new HttpChannelParentListener(this);

  httpChan->SetNotificationCallbacks(channelListener);

  httpChan->SetRequestMethod(nsDependentCString(requestMethod.get()));

  nsCOMPtr<nsIInputStream> stream = DeserializeInputStream(uploadStream);
  if (stream) {
    httpChan->InternalSetUploadStream(stream);
    httpChan->SetUploadStreamHasHeaders(uploadStreamHasHeaders);
  }

  if (priority != nsISupportsPriority::PRIORITY_NORMAL)
    httpChan->SetPriority(priority);
  httpChan->SetRedirectionLimit(redirectionLimit);
  httpChan->SetAllowPipelining(allowPipelining);
  httpChan->SetForceAllowThirdPartyCookie(forceAllowThirdPartyCookie);
  httpChan->SetAllowSpdy(allowSpdy);

  nsCOMPtr<nsIApplicationCacheChannel> appCacheChan =
    do_QueryInterface(mChannel);
  nsCOMPtr<nsIApplicationCacheService> appCacheService =
    do_GetService(NS_APPLICATIONCACHESERVICE_CONTRACTID);

  bool setChooseApplicationCache = chooseApplicationCache;
  if (appCacheChan && appCacheService) {
    // We might potentially want to drop this flag (that is TRUE by default)
    // after we successfully associate the channel with an application cache
    // reported by the channel child.  Dropping it here may be too early.
    appCacheChan->SetInheritApplicationCache(false);
    if (!appCacheClientID.IsEmpty()) {
      nsCOMPtr<nsIApplicationCache> appCache;
      rv = appCacheService->GetApplicationCache(appCacheClientID,
                                                getter_AddRefs(appCache));
      if (NS_SUCCEEDED(rv)) {
        appCacheChan->SetApplicationCache(appCache);
        setChooseApplicationCache = false;
      }
    }

    if (setChooseApplicationCache) {
      bool inBrowser = false;
      uint32_t appId = NECKO_NO_APP_ID;
      if (mLoadContext) {
        mLoadContext->GetIsInBrowserElement(&inBrowser);
        mLoadContext->GetAppId(&appId);
      }

      bool chooseAppCache = false;
      nsCOMPtr<nsIScriptSecurityManager> secMan =
        do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID);
      if (secMan) {
        nsCOMPtr<nsIPrincipal> principal;
        secMan->GetAppCodebasePrincipal(uri, appId, inBrowser, getter_AddRefs(principal));

        // This works because we've already called SetNotificationCallbacks and
        // done mPBOverride logic by this point.
        chooseAppCache = NS_ShouldCheckAppCache(principal, NS_UsePrivateBrowsing(mChannel));
      }

      appCacheChan->SetChooseApplicationCache(chooseAppCache);
    }
  }

  rv = httpChan->AsyncOpen(channelListener, nullptr);
  if (NS_FAILED(rv))
    return SendFailedAsyncOpen(rv);

  return true;
}

bool
HttpChannelParent::ConnectChannel(const uint32_t& channelId)
{
  nsresult rv;

  LOG(("Looking for a registered channel [this=%p, id=%d]", this, channelId));
  rv = NS_LinkRedirectChannels(channelId, this, getter_AddRefs(mChannel));
  LOG(("  found channel %p, rv=%08x", mChannel.get(), rv));

  if (mPBOverride != kPBOverride_Unset) {
    // redirected-to channel may not support PB
    nsCOMPtr<nsIPrivateBrowsingChannel> pbChannel = do_QueryInterface(mChannel);
    if (pbChannel) {
      pbChannel->SetPrivate(mPBOverride == kPBOverride_Private ? true : false);
    }
  }

  return true;
}

bool
HttpChannelParent::RecvSetPriority(const uint16_t& priority)
{
  if (mChannel) {
    nsHttpChannel *httpChan = static_cast<nsHttpChannel *>(mChannel.get());
    httpChan->SetPriority(priority);
  }

  nsCOMPtr<nsISupportsPriority> priorityRedirectChannel =
      do_QueryInterface(mRedirectChannel);
  if (priorityRedirectChannel)
    priorityRedirectChannel->SetPriority(priority);

  return true;
}

bool
HttpChannelParent::RecvSuspend()
{
  if (mChannel) {
    mChannel->Suspend();
  }
  return true;
}

bool
HttpChannelParent::RecvResume()
{
  if (mChannel) {
    mChannel->Resume();
  }
  return true;
}

bool
HttpChannelParent::RecvCancel(const nsresult& status)
{
  // May receive cancel before channel has been constructed!
  if (mChannel) {
    nsHttpChannel *httpChan = static_cast<nsHttpChannel *>(mChannel.get());
    httpChan->Cancel(status);
  }
  return true;
}


bool
HttpChannelParent::RecvSetCacheTokenCachedCharset(const nsCString& charset)
{
  if (mCacheEntry)
    mCacheEntry->SetMetaDataElement("charset", charset.get());
  return true;
}

bool
HttpChannelParent::RecvUpdateAssociatedContentSecurity(const int32_t& broken,
                                                       const int32_t& no)
{
  if (mAssociatedContentSecurity) {
    mAssociatedContentSecurity->SetCountSubRequestsBrokenSecurity(broken);
    mAssociatedContentSecurity->SetCountSubRequestsNoSecurity(no);
  }
  return true;
}

bool
HttpChannelParent::RecvRedirect2Verify(const nsresult& result,
                                       const RequestHeaderTuples& changedHeaders,
                                       const OptionalURIParams&   aAPIRedirectURI)
{
  if (NS_SUCCEEDED(result)) {
    nsCOMPtr<nsIHttpChannel> newHttpChannel =
        do_QueryInterface(mRedirectChannel);

    if (newHttpChannel) {
      nsCOMPtr<nsIURI> apiRedirectUri = DeserializeURI(aAPIRedirectURI);

      if (apiRedirectUri)
        newHttpChannel->RedirectTo(apiRedirectUri);

      for (uint32_t i = 0; i < changedHeaders.Length(); i++) {
        newHttpChannel->SetRequestHeader(changedHeaders[i].mHeader,
                                         changedHeaders[i].mValue,
                                         changedHeaders[i].mMerge);
      }
    }
  }

  if (!mRedirectCallback) {
    // This should according the logic never happen, log the situation.
    if (mReceivedRedirect2Verify)
      LOG(("RecvRedirect2Verify[%p]: Duplicate fire", this));
    if (mSentRedirect1BeginFailed)
      LOG(("RecvRedirect2Verify[%p]: Send to child failed", this));
    if (mSentRedirect1Begin && NS_FAILED(result))
      LOG(("RecvRedirect2Verify[%p]: Redirect failed", this));
    if (mSentRedirect1Begin && NS_SUCCEEDED(result))
      LOG(("RecvRedirect2Verify[%p]: Redirect succeeded", this));
    if (!mRedirectChannel)
      LOG(("RecvRedirect2Verify[%p]: Missing redirect channel", this));

    NS_ERROR("Unexpcted call to HttpChannelParent::RecvRedirect2Verify, "
             "mRedirectCallback null");
  }

  mReceivedRedirect2Verify = true;

  if (mRedirectCallback) {
    mRedirectCallback->OnRedirectVerifyCallback(result);
    mRedirectCallback = nullptr;
  }

  return true;
}

bool
HttpChannelParent::RecvDocumentChannelCleanup()
{
  // From now on only using mAssociatedContentSecurity.  Free everything else.
  mChannel = 0;          // Reclaim some memory sooner.
  mCacheEntry = 0;  // Else we'll block other channels reading same URI
  return true;
}

bool
HttpChannelParent::RecvMarkOfflineCacheEntryAsForeign()
{
  if (mOfflineForeignMarker) {
    mOfflineForeignMarker->MarkAsForeign();
    mOfflineForeignMarker = 0;
  }

  return true;
}

//-----------------------------------------------------------------------------
// HttpChannelParent::nsIRequestObserver
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParent::OnStartRequest(nsIRequest *aRequest, nsISupports *aContext)
{
  LOG(("HttpChannelParent::OnStartRequest [this=%p]\n", this));

  nsHttpChannel *chan = static_cast<nsHttpChannel *>(aRequest);
  nsHttpResponseHead *responseHead = chan->GetResponseHead();
  nsHttpRequestHead  *requestHead = chan->GetRequestHead();
  bool isFromCache = false;
  chan->IsFromCache(&isFromCache);
  uint32_t expirationTime = nsICache::NO_EXPIRATION_TIME;
  chan->GetCacheTokenExpirationTime(&expirationTime);
  nsCString cachedCharset;
  chan->GetCacheTokenCachedCharset(cachedCharset);

  bool loadedFromApplicationCache;
  chan->GetLoadedFromApplicationCache(&loadedFromApplicationCache);
  if (loadedFromApplicationCache) {
    mOfflineForeignMarker = chan->GetOfflineCacheEntryAsForeignMarker();
    nsCOMPtr<nsIApplicationCache> appCache;
    chan->GetApplicationCache(getter_AddRefs(appCache));
    nsCString appCacheGroupId;
    nsCString appCacheClientId;
    appCache->GetGroupID(appCacheGroupId);
    appCache->GetClientID(appCacheClientId);
    if (mIPCClosed ||
        !SendAssociateApplicationCache(appCacheGroupId, appCacheClientId))
    {
      return NS_ERROR_UNEXPECTED;
    }
  }

  nsCOMPtr<nsIEncodedChannel> encodedChannel = do_QueryInterface(aRequest);
  if (encodedChannel)
    encodedChannel->SetApplyConversion(false);

  // Keep the cache entry for future use in RecvSetCacheTokenCachedCharset().
  // It could be already released by nsHttpChannel at that time.
  nsCOMPtr<nsISupports> cacheEntry;
  chan->GetCacheToken(getter_AddRefs(cacheEntry));
  mCacheEntry = do_QueryInterface(cacheEntry);


  nsCString secInfoSerialization;
  nsCOMPtr<nsISupports> secInfoSupp;
  chan->GetSecurityInfo(getter_AddRefs(secInfoSupp));
  if (secInfoSupp) {
    mAssociatedContentSecurity = do_QueryInterface(secInfoSupp);
    nsCOMPtr<nsISerializable> secInfoSer = do_QueryInterface(secInfoSupp);
    if (secInfoSer)
      NS_SerializeToString(secInfoSer, secInfoSerialization);
  }

  nsHttpChannel *httpChan = static_cast<nsHttpChannel *>(mChannel.get());
  if (mIPCClosed ||
      !SendOnStartRequest(responseHead ? *responseHead : nsHttpResponseHead(),
                          !!responseHead,
                          requestHead->Headers(),
                          isFromCache,
                          mCacheEntry ? true : false,
                          expirationTime, cachedCharset, secInfoSerialization,
                          httpChan->GetSelfAddr(), httpChan->GetPeerAddr()))
  {
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelParent::OnStopRequest(nsIRequest *aRequest,
                                 nsISupports *aContext,
                                 nsresult aStatusCode)
{
  LOG(("HttpChannelParent::OnStopRequest: [this=%p status=%x]\n",
       this, aStatusCode));

  if (mIPCClosed || !SendOnStopRequest(aStatusCode))
    return NS_ERROR_UNEXPECTED;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelParent::nsIStreamListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParent::OnDataAvailable(nsIRequest *aRequest,
                                   nsISupports *aContext,
                                   nsIInputStream *aInputStream,
                                   uint64_t aOffset,
                                   uint32_t aCount)
{
  LOG(("HttpChannelParent::OnDataAvailable [this=%p]\n", this));

  nsCString data;
  nsresult rv = NS_ReadInputStreamToString(aInputStream, data, aCount);
  if (NS_FAILED(rv))
    return rv;

  // OnDataAvailable is always preceded by OnStatus/OnProgress calls that set
  // mStoredStatus/mStoredProgress(Max) to appropriate values, unless
  // LOAD_BACKGROUND set.  In that case, they'll have garbage values, but
  // child doesn't use them.
  if (mIPCClosed || !SendOnTransportAndData(mStoredStatus, mStoredProgress,
                                            mStoredProgressMax, data, aOffset,
                                            aCount)) {
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelParent::nsIProgressEventSink
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParent::OnProgress(nsIRequest *aRequest,
                              nsISupports *aContext,
                              uint64_t aProgress,
                              uint64_t aProgressMax)
{
  // OnStatus has always just set mStoredStatus. If it indicates this precedes
  // OnDataAvailable, store and ODA will send to child.
  if (mStoredStatus == NS_NET_STATUS_RECEIVING_FROM ||
      mStoredStatus == NS_NET_STATUS_READING)
  {
    mStoredProgress = aProgress;
    mStoredProgressMax = aProgressMax;
  } else {
    // Send to child now.  The only case I've observed that this handles (i.e.
    // non-ODA status with progress > 0) is data upload progress notification
    // (status == NS_NET_STATUS_SENDING_TO)
    if (mIPCClosed || !SendOnProgress(aProgress, aProgressMax))
      return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

NS_IMETHODIMP
HttpChannelParent::OnStatus(nsIRequest *aRequest,
                            nsISupports *aContext,
                            nsresult aStatus,
                            const char16_t *aStatusArg)
{
  // If this precedes OnDataAvailable, store and ODA will send to child.
  if (aStatus == NS_NET_STATUS_RECEIVING_FROM ||
      aStatus == NS_NET_STATUS_READING)
  {
    mStoredStatus = aStatus;
    return NS_OK;
  }
  // Otherwise, send to child now
  if (mIPCClosed || !SendOnStatus(aStatus))
    return NS_ERROR_UNEXPECTED;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelParent::nsIParentChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParent::Delete()
{
  if (!mIPCClosed)
    unused << SendDeleteSelf();

  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelParent::nsIParentRedirectingChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParent::StartRedirect(uint32_t newChannelId,
                                 nsIChannel* newChannel,
                                 uint32_t redirectFlags,
                                 nsIAsyncVerifyRedirectCallback* callback)
{
  if (mIPCClosed)
    return NS_BINDING_ABORTED;

  nsCOMPtr<nsIURI> newURI;
  newChannel->GetURI(getter_AddRefs(newURI));

  URIParams uriParams;
  SerializeURI(newURI, uriParams);

  nsHttpChannel *httpChan = static_cast<nsHttpChannel *>(mChannel.get());
  nsHttpResponseHead *responseHead = httpChan->GetResponseHead();
  bool result = SendRedirect1Begin(newChannelId, uriParams, redirectFlags,
                                   responseHead ? *responseHead
                                                : nsHttpResponseHead());
  if (!result) {
    // Bug 621446 investigation
    mSentRedirect1BeginFailed = true;
    return NS_BINDING_ABORTED;
  }

  // Bug 621446 investigation
  mSentRedirect1Begin = true;

  // Result is handled in RecvRedirect2Verify above

  mRedirectChannel = newChannel;
  mRedirectCallback = callback;
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelParent::CompleteRedirect(bool succeeded)
{
  if (succeeded && !mIPCClosed) {
    // TODO: check return value: assume child dead if failed
    unused << SendRedirect3Complete();
  }

  mRedirectChannel = nullptr;
  return NS_OK;
}

}} // mozilla::net
