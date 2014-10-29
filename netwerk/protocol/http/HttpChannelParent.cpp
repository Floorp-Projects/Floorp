/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "mozilla/ipc/FileDescriptorSetParent.h"
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
#include "nsIAuthInformation.h"
#include "nsIAuthPromptCallback.h"
#include "nsIContentPolicy.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "nsIOService.h"
#include "nsICachingChannel.h"


using namespace mozilla::dom;
using namespace mozilla::ipc;

namespace mozilla {
namespace net {

HttpChannelParent::HttpChannelParent(const PBrowserOrId& iframeEmbedding,
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
  , mStatus(NS_OK)
  , mDivertingFromChild(false)
  , mDivertedOnStartRequest(false)
  , mSuspendedForDiversion(false)
  , mNestedFrameId(0)
{
  // Ensure gHttpHandler is initialized: we need the atom table up and running.
  nsCOMPtr<nsIHttpProtocolHandler> dummyInitializer =
    do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http");

  MOZ_ASSERT(gHttpHandler);
  mHttpHandler = gHttpHandler;

  if (iframeEmbedding.type() == PBrowserOrId::TPBrowserParent) {
    mTabParent = static_cast<dom::TabParent*>(iframeEmbedding.get_PBrowserParent());
  } else {
    mNestedFrameId = iframeEmbedding.get_uint64_t();
  }

  mObserver = new OfflineObserver(this);
}

HttpChannelParent::~HttpChannelParent()
{
  if (mObserver) {
    mObserver->RemoveObserver();
  }
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
                       a.redirectionLimit(), a.allowPipelining(), a.allowSTS(),
                       a.thirdPartyFlags(), a.resumeAt(), a.startPos(),
                       a.entityID(), a.chooseApplicationCache(),
                       a.appCacheClientID(), a.allowSpdy(), a.fds(),
                       a.requestingPrincipalInfo(), a.securityFlags(),
                       a.contentPolicyType());
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

NS_IMPL_ISUPPORTS(HttpChannelParent,
                  nsIInterfaceRequestor,
                  nsIProgressEventSink,
                  nsIRequestObserver,
                  nsIStreamListener,
                  nsIParentChannel,
                  nsIAuthPromptProvider,
                  nsIParentRedirectingChannel)

//-----------------------------------------------------------------------------
// HttpChannelParent::nsIInterfaceRequestor
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParent::GetInterface(const nsIID& aIID, void **result)
{
  if (aIID.Equals(NS_GET_IID(nsIAuthPromptProvider)) ||
      aIID.Equals(NS_GET_IID(nsISecureBrowserUI))) {
    if (mTabParent) {
      return mTabParent->QueryInterface(aIID, result);
    }
  }

  // Only support nsIAuthPromptProvider in Content process
  if (XRE_GetProcessType() == GeckoProcessType_Default &&
      aIID.Equals(NS_GET_IID(nsIAuthPromptProvider))) {
    *result = nullptr;
    return NS_OK;
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
                                 const uint32_t&            aLoadFlags,
                                 const RequestHeaderTuples& requestHeaders,
                                 const nsCString&           requestMethod,
                                 const OptionalInputStreamParams& uploadStream,
                                 const bool&                uploadStreamHasHeaders,
                                 const uint16_t&            priority,
                                 const uint8_t&             redirectionLimit,
                                 const bool&                allowPipelining,
                                 const bool&                allowSTS,
                                 const uint32_t&            thirdPartyFlags,
                                 const bool&                doResumeAt,
                                 const uint64_t&            startPos,
                                 const nsCString&           entityID,
                                 const bool&                chooseApplicationCache,
                                 const nsCString&           appCacheClientID,
                                 const bool&                allowSpdy,
                                 const OptionalFileDescriptorSet& aFds,
                                 const ipc::PrincipalInfo&  aRequestingPrincipalInfo,
                                 const uint32_t&            aSecurityFlags,
                                 const uint32_t&            aContentPolicyType)
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

  nsCOMPtr<nsIPrincipal> requestingPrincipal =
    mozilla::ipc::PrincipalInfoToPrincipal(aRequestingPrincipalInfo, &rv);
  if (NS_FAILED(rv)) {
    return SendFailedAsyncOpen(rv);
  }

  bool appOffline = false;
  uint32_t appId = GetAppId();
  if (appId != NECKO_UNKNOWN_APP_ID &&
      appId != NECKO_NO_APP_ID) {
    gIOService->IsAppOffline(appId, &appOffline);
  }

  uint32_t loadFlags = aLoadFlags;
  if (appOffline) {
    loadFlags |= nsICachingChannel::LOAD_ONLY_FROM_CACHE;
    loadFlags |= nsIRequest::LOAD_FROM_CACHE;
    loadFlags |= nsICachingChannel::LOAD_NO_NETWORK_IO;
  }

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel),
                     uri,
                     requestingPrincipal,
                     aSecurityFlags,
                     aContentPolicyType,
                     nullptr,   // loadGroup
                     nullptr,   // aCallbacks
                     loadFlags,
                     ios);

  if (NS_FAILED(rv))
    return SendFailedAsyncOpen(rv);

  mChannel = static_cast<nsHttpChannel *>(channel.get());
  mChannel->SetTimingEnabled(true);
  if (mPBOverride != kPBOverride_Unset) {
    mChannel->SetPrivate(mPBOverride == kPBOverride_Private ? true : false);
  }

  if (doResumeAt)
    mChannel->ResumeAt(startPos, entityID);

  if (originalUri)
    mChannel->SetOriginalURI(originalUri);
  if (docUri)
    mChannel->SetDocumentURI(docUri);
  if (referrerUri)
    mChannel->SetReferrerInternal(referrerUri);
  if (apiRedirectToUri)
    mChannel->RedirectTo(apiRedirectToUri);
  if (loadFlags != nsIRequest::LOAD_NORMAL)
    mChannel->SetLoadFlags(loadFlags);

  for (uint32_t i = 0; i < requestHeaders.Length(); i++) {
    mChannel->SetRequestHeader(requestHeaders[i].mHeader,
                               requestHeaders[i].mValue,
                               requestHeaders[i].mMerge);
  }

  mParentListener = new HttpChannelParentListener(this);

  mChannel->SetNotificationCallbacks(mParentListener);

  mChannel->SetRequestMethod(nsDependentCString(requestMethod.get()));

  nsTArray<mozilla::ipc::FileDescriptor> fds;
  if (aFds.type() == OptionalFileDescriptorSet::TPFileDescriptorSetParent) {
    FileDescriptorSetParent* fdSetActor =
      static_cast<FileDescriptorSetParent*>(aFds.get_PFileDescriptorSetParent());
    MOZ_ASSERT(fdSetActor);

    fdSetActor->ForgetFileDescriptors(fds);
    MOZ_ASSERT(!fds.IsEmpty());

    unused << fdSetActor->Send__delete__(fdSetActor);
  }

  nsCOMPtr<nsIInputStream> stream = DeserializeInputStream(uploadStream, fds);
  if (stream) {
    mChannel->InternalSetUploadStream(stream);
    mChannel->SetUploadStreamHasHeaders(uploadStreamHasHeaders);
  }

  if (priority != nsISupportsPriority::PRIORITY_NORMAL)
    mChannel->SetPriority(priority);
  mChannel->SetRedirectionLimit(redirectionLimit);
  mChannel->SetAllowPipelining(allowPipelining);
  mChannel->SetAllowSTS(allowSTS);
  mChannel->SetThirdPartyFlags(thirdPartyFlags);
  mChannel->SetAllowSpdy(allowSpdy);

  nsCOMPtr<nsIApplicationCacheChannel> appCacheChan =
    do_QueryObject(mChannel);
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
      if (mLoadContext) {
        mLoadContext->GetIsInBrowserElement(&inBrowser);
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

  rv = mChannel->AsyncOpen(mParentListener, nullptr);
  if (NS_FAILED(rv))
    return SendFailedAsyncOpen(rv);

  return true;
}

bool
HttpChannelParent::ConnectChannel(const uint32_t& channelId)
{
  nsresult rv;

  LOG(("Looking for a registered channel [this=%p, id=%d]", this, channelId));
  nsCOMPtr<nsIChannel> channel;
  rv = NS_LinkRedirectChannels(channelId, this, getter_AddRefs(channel));
  mChannel = static_cast<nsHttpChannel*>(channel.get());
  LOG(("  found channel %p, rv=%08x", mChannel.get(), rv));

  if (mPBOverride != kPBOverride_Unset) {
    // redirected-to channel may not support PB
    nsCOMPtr<nsIPrivateBrowsingChannel> pbChannel = do_QueryObject(mChannel);
    if (pbChannel) {
      pbChannel->SetPrivate(mPBOverride == kPBOverride_Private ? true : false);
    }
  }

  bool appOffline = false;
  uint32_t appId = GetAppId();
  if (appId != NECKO_UNKNOWN_APP_ID &&
      appId != NECKO_NO_APP_ID) {
    gIOService->IsAppOffline(appId, &appOffline);
  }

  if (appOffline) {
    uint32_t loadFlags;
    mChannel->GetLoadFlags(&loadFlags);
    loadFlags |= nsICachingChannel::LOAD_ONLY_FROM_CACHE;
    loadFlags |= nsIRequest::LOAD_FROM_CACHE;
    loadFlags |= nsICachingChannel::LOAD_NO_NETWORK_IO;
    mChannel->SetLoadFlags(loadFlags);
  }

  return true;
}

bool
HttpChannelParent::RecvSetPriority(const uint16_t& priority)
{
  if (mChannel) {
    mChannel->SetPriority(priority);
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
    mChannel->Cancel(status);
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

bool
HttpChannelParent::RecvDivertOnDataAvailable(const nsCString& data,
                                             const uint64_t& offset,
                                             const uint32_t& count)
{
  MOZ_ASSERT(mParentListener);
  if (NS_WARN_IF(!mDivertingFromChild)) {
    MOZ_ASSERT(mDivertingFromChild,
               "Cannot RecvDivertOnDataAvailable if diverting is not set!");
    FailDiversion(NS_ERROR_UNEXPECTED);
    return false;
  }

  // Drop OnDataAvailables if the parent was canceled already.
  if (NS_FAILED(mStatus)) {
    return true;
  }

  nsCOMPtr<nsIInputStream> stringStream;
  nsresult rv = NS_NewByteInputStream(getter_AddRefs(stringStream), data.get(),
                                      count, NS_ASSIGNMENT_DEPEND);
  if (NS_FAILED(rv)) {
    if (mChannel) {
      mChannel->Cancel(rv);
    }
    mStatus = rv;
    return true;
  }

  nsCOMPtr<nsIStreamListener> listener;
  if (mConverterListener) {
    listener = mConverterListener;
  } else {
    listener = mParentListener;
    MOZ_ASSERT(listener);
  }
  rv = listener->OnDataAvailable(mChannel, nullptr, stringStream,
                                        offset, count);
  stringStream->Close();
  if (NS_FAILED(rv)) {
    if (mChannel) {
      mChannel->Cancel(rv);
    }
    mStatus = rv;
    return true;
  }
  return true;
}

bool
HttpChannelParent::RecvDivertOnStopRequest(const nsresult& statusCode)
{
  MOZ_ASSERT(mParentListener);
  if (NS_WARN_IF(!mDivertingFromChild)) {
    MOZ_ASSERT(mDivertingFromChild,
               "Cannot RecvDivertOnStopRequest if diverting is not set!");
    FailDiversion(NS_ERROR_UNEXPECTED);
    return false;
  }

  // Honor the channel's status even if the underlying transaction completed.
  nsresult status = NS_FAILED(mStatus) ? mStatus : statusCode;

  // Reset fake pending status in case OnStopRequest has already been called.
  if (mChannel) {
    mChannel->ForcePending(false);
  }

  nsCOMPtr<nsIStreamListener> listener;
  if (mConverterListener) {
    listener = mConverterListener;
  } else {
    listener = mParentListener;
    MOZ_ASSERT(listener);
  }
  listener->OnStopRequest(mChannel, nullptr, status);
  return true;
}

bool
HttpChannelParent::RecvDivertComplete()
{
  MOZ_ASSERT(mParentListener);
  if (NS_WARN_IF(!mDivertingFromChild)) {
    MOZ_ASSERT(mDivertingFromChild,
               "Cannot RecvDivertComplete if diverting is not set!");
    FailDiversion(NS_ERROR_UNEXPECTED);
    return false;
  }

  nsresult rv = ResumeForDiversion();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    FailDiversion(NS_ERROR_UNEXPECTED);
    return false;
  }

  mConverterListener = nullptr; 
  mParentListener = nullptr;
  return true;
}

//-----------------------------------------------------------------------------
// HttpChannelParent::nsIRequestObserver
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParent::OnStartRequest(nsIRequest *aRequest, nsISupports *aContext)
{
  LOG(("HttpChannelParent::OnStartRequest [this=%p]\n", this));

  MOZ_RELEASE_ASSERT(!mDivertingFromChild,
    "Cannot call OnStartRequest if diverting is set!");

  nsHttpChannel *chan = static_cast<nsHttpChannel *>(aRequest);
  nsHttpResponseHead *responseHead = chan->GetResponseHead();
  nsHttpRequestHead  *requestHead = chan->GetRequestHead();
  bool isFromCache = false;
  chan->IsFromCache(&isFromCache);
  uint32_t expirationTime = nsICacheEntry::NO_EXPIRATION_TIME;
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

  nsresult channelStatus = NS_OK;
  chan->GetStatus(&channelStatus);

  nsCString secInfoSerialization;
  nsCOMPtr<nsISupports> secInfoSupp;
  chan->GetSecurityInfo(getter_AddRefs(secInfoSupp));
  if (secInfoSupp) {
    mAssociatedContentSecurity = do_QueryInterface(secInfoSupp);
    nsCOMPtr<nsISerializable> secInfoSer = do_QueryInterface(secInfoSupp);
    if (secInfoSer)
      NS_SerializeToString(secInfoSer, secInfoSerialization);
  }

  uint16_t redirectCount = 0;
  mChannel->GetRedirectCount(&redirectCount);
  if (mIPCClosed ||
      !SendOnStartRequest(channelStatus,
                          responseHead ? *responseHead : nsHttpResponseHead(),
                          !!responseHead,
                          requestHead->Headers(),
                          isFromCache,
                          mCacheEntry ? true : false,
                          expirationTime, cachedCharset, secInfoSerialization,
                          mChannel->GetSelfAddr(), mChannel->GetPeerAddr(),
                          redirectCount))
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

  MOZ_RELEASE_ASSERT(!mDivertingFromChild,
    "Cannot call OnStopRequest if diverting is set!");
  ResourceTimingStruct timing;
  mChannel->GetDomainLookupStart(&timing.domainLookupStart);
  mChannel->GetDomainLookupEnd(&timing.domainLookupEnd);
  mChannel->GetConnectStart(&timing.connectStart);
  mChannel->GetConnectEnd(&timing.connectEnd);
  mChannel->GetRequestStart(&timing.requestStart);
  mChannel->GetResponseStart(&timing.responseStart);
  mChannel->GetResponseEnd(&timing.responseEnd);
  mChannel->GetAsyncOpen(&timing.fetchStart);
  mChannel->GetRedirectStart(&timing.redirectStart);
  mChannel->GetRedirectEnd(&timing.redirectEnd);

  if (mIPCClosed || !SendOnStopRequest(aStatusCode, timing))
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

  MOZ_RELEASE_ASSERT(!mDivertingFromChild,
    "Cannot call OnDataAvailable if diverting is set!");

  nsCString data;
  nsresult rv = NS_ReadInputStreamToString(aInputStream, data, aCount);
  if (NS_FAILED(rv))
    return rv;

  nsresult channelStatus = NS_OK;
  mChannel->GetStatus(&channelStatus);

  // OnDataAvailable is always preceded by OnStatus/OnProgress calls that set
  // mStoredStatus/mStoredProgress(Max) to appropriate values, unless
  // LOAD_BACKGROUND set.  In that case, they'll have garbage values, but
  // child doesn't use them.
  if (mIPCClosed || !SendOnTransportAndData(channelStatus, mStoredStatus,
                                            mStoredProgress, mStoredProgressMax,
                                            data, aOffset, aCount)) {
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
    // Send OnProgress events to the child for data upload progress notifications
    // (i.e. status == NS_NET_STATUS_SENDING_TO) or if the channel has
    // LOAD_BACKGROUND set.
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
HttpChannelParent::SetParentListener(HttpChannelParentListener* aListener)
{
  MOZ_ASSERT(aListener);
  MOZ_ASSERT(!mParentListener, "SetParentListener should only be called for "
                               "new HttpChannelParents after a redirect, when "
                               "mParentListener is null.");
  mParentListener = aListener;
  return NS_OK;
}

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

  nsHttpResponseHead *responseHead = mChannel->GetResponseHead();
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

//-----------------------------------------------------------------------------
// HttpChannelParent::ADivertableParentChannel
//-----------------------------------------------------------------------------
nsresult
HttpChannelParent::SuspendForDiversion()
{
  MOZ_ASSERT(mChannel);
  MOZ_ASSERT(mParentListener);
  if (NS_WARN_IF(mDivertingFromChild)) {
    MOZ_ASSERT(!mDivertingFromChild, "Already suspended for diversion!");
    return NS_ERROR_UNEXPECTED;
  }

  // Try suspending the channel. Allow it to fail, since OnStopRequest may have
  // been called and thus the channel may not be pending.
  nsresult rv = mChannel->Suspend();
  MOZ_ASSERT(NS_SUCCEEDED(rv) || rv == NS_ERROR_NOT_AVAILABLE);
  mSuspendedForDiversion = NS_SUCCEEDED(rv);

  rv = mParentListener->SuspendForDiversion();
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  // Once this is set, no more OnStart/OnData/OnStop callbacks should be sent
  // to the child.
  mDivertingFromChild = true;

  return NS_OK;
}

/* private, supporting function for ADivertableParentChannel */
nsresult
HttpChannelParent::ResumeForDiversion()
{
  MOZ_ASSERT(mChannel);
  if (NS_WARN_IF(!mDivertingFromChild)) {
    MOZ_ASSERT(mDivertingFromChild,
               "Cannot ResumeForDiversion if not diverting!");
    return NS_ERROR_UNEXPECTED;
  }

  if (mSuspendedForDiversion) {
    // The nsHttpChannel will deliver remaining OnData/OnStop for the transfer.
    nsresult rv = mChannel->Resume();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      FailDiversion(NS_ERROR_UNEXPECTED, true);
      return rv;
    }
    mSuspendedForDiversion = false;
  }

  if (NS_WARN_IF(mIPCClosed || !SendDeleteSelf())) {
    FailDiversion(NS_ERROR_UNEXPECTED);
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

void
HttpChannelParent::DivertTo(nsIStreamListener *aListener)
{
  MOZ_ASSERT(mParentListener);
  if (NS_WARN_IF(!mDivertingFromChild)) {
    MOZ_ASSERT(mDivertingFromChild,
               "Cannot DivertTo new listener if diverting is not set!");
    return;
  }

  DebugOnly<nsresult> rv = mParentListener->DivertTo(aListener);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  if (NS_WARN_IF(mIPCClosed || !SendFlushedForDiversion())) {
    FailDiversion(NS_ERROR_UNEXPECTED);
    return;
  }

  // Call OnStartRequest and SendDivertMessages asynchronously to avoid
  // reentering client context.
  NS_DispatchToCurrentThread(
    NS_NewRunnableMethod(this, &HttpChannelParent::StartDiversion));
  return;
}

void
HttpChannelParent::StartDiversion()
{
  if (NS_WARN_IF(!mDivertingFromChild)) {
    MOZ_ASSERT(mDivertingFromChild,
               "Cannot StartDiversion if diverting is not set!");
    return;
  }

  // Fake pending status in case OnStopRequest has already been called.
  if (mChannel) {
    mChannel->ForcePending(true);
  }

  // Call OnStartRequest for the "DivertTo" listener.
  nsresult rv = mParentListener->OnStartRequest(mChannel, nullptr);
  if (NS_FAILED(rv)) {
    if (mChannel) {
      mChannel->Cancel(rv);
    }
    mStatus = rv;
  }
  mDivertedOnStartRequest = true;

  // After OnStartRequest has been called, setup content decoders if needed.
  //
  // We want to use the same decoders that the nsHttpChannel might use. There
  // are two possible scenarios depending on whether OnStopRequest has been
  // called or not.
  //
  // 1. nsHttpChannel has not called OnStopRequest yet.
  // Create content conversion chain based on nsHttpChannel::mListener
  // Get ptr to final listener and set that in mContentConverter, as well as
  // nsHttpChannel::mListener.
  //
  // 2. nsHttpChannel has called OnStopRequest.
  // Create a content conversion chain based on mParentListener.
  // Get ptr to final listener and set that in mContentConverter.

  nsCOMPtr<nsIStreamListener> converterListener;
  mChannel->DoApplyContentConversions(mParentListener,
                                      getter_AddRefs(converterListener));
  if (converterListener) {
    mConverterListener = converterListener.forget();
  }

  // The listener chain should now be setup; tell HttpChannelChild to divert
  // the OnDataAvailables and OnStopRequest to this HttpChannelParent.
  if (NS_WARN_IF(mIPCClosed || !SendDivertMessages())) {
    FailDiversion(NS_ERROR_UNEXPECTED);
    return;
  }
}

class HTTPFailDiversionEvent : public nsRunnable
{
public:
  HTTPFailDiversionEvent(HttpChannelParent *aChannelParent,
                         nsresult aErrorCode,
                         bool aSkipResume)
    : mChannelParent(aChannelParent)
    , mErrorCode(aErrorCode)
    , mSkipResume(aSkipResume)
  {
    MOZ_RELEASE_ASSERT(aChannelParent);
    MOZ_RELEASE_ASSERT(NS_FAILED(aErrorCode));
  }
  NS_IMETHOD Run()
  {
    mChannelParent->NotifyDiversionFailed(mErrorCode, mSkipResume);
    return NS_OK;
  }
private:
  nsRefPtr<HttpChannelParent> mChannelParent;
  nsresult mErrorCode;
  bool mSkipResume;
};

void
HttpChannelParent::FailDiversion(nsresult aErrorCode,
                                 bool aSkipResume)
{
  MOZ_RELEASE_ASSERT(NS_FAILED(aErrorCode));
  MOZ_RELEASE_ASSERT(mDivertingFromChild);
  MOZ_RELEASE_ASSERT(mParentListener);
  MOZ_RELEASE_ASSERT(mChannel);

  NS_DispatchToCurrentThread(
    new HTTPFailDiversionEvent(this, aErrorCode, aSkipResume));
}

void
HttpChannelParent::NotifyDiversionFailed(nsresult aErrorCode,
                                         bool aSkipResume)
{
  MOZ_RELEASE_ASSERT(NS_FAILED(aErrorCode));
  MOZ_RELEASE_ASSERT(mDivertingFromChild);
  MOZ_RELEASE_ASSERT(mParentListener);
  MOZ_RELEASE_ASSERT(mChannel);

  mChannel->Cancel(aErrorCode);

  mChannel->ForcePending(false);

  bool isPending = false;
  nsresult rv = mChannel->IsPending(&isPending);
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));

  // Resume only if we suspended earlier.
  if (mSuspendedForDiversion) {
    mChannel->Resume();
  }
  // Channel has already sent OnStartRequest to the child, so ensure that we
  // call it here if it hasn't already been called.
  if (!mDivertedOnStartRequest) {
    mChannel->ForcePending(true);
    mParentListener->OnStartRequest(mChannel, nullptr);
    mChannel->ForcePending(false);
  }
  // If the channel is pending, it will call OnStopRequest itself; otherwise, do
  // it here.
  if (!isPending) {
    mParentListener->OnStopRequest(mChannel, nullptr, aErrorCode);
  }
  mParentListener = nullptr;
  mConverterListener = nullptr;
  mChannel = nullptr;

  if (!mIPCClosed) {
    unused << SendDeleteSelf();
  }
}

void
HttpChannelParent::OfflineDisconnect()
{
  if (mChannel) {
    mChannel->Cancel(NS_ERROR_OFFLINE);
  }
  mStatus = NS_ERROR_OFFLINE;
}

uint32_t
HttpChannelParent::GetAppId()
{
  uint32_t appId = NECKO_UNKNOWN_APP_ID;
  if (mLoadContext) {
    mLoadContext->GetAppId(&appId);
  }
  return appId;
}

NS_IMETHODIMP
HttpChannelParent::GetAuthPrompt(uint32_t aPromptReason, const nsIID& iid,
                                 void** aResult)
{
  nsCOMPtr<nsIAuthPrompt2> prompt =
    new NeckoParent::NestedFrameAuthPrompt(Manager(), mNestedFrameId);
  prompt.forget(aResult);
  return NS_OK;
}

}} // mozilla::net
