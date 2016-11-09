/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "mozilla/ipc/FileDescriptorSetParent.h"
#include "mozilla/net/HttpChannelParent.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/net/NeckoParent.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "HttpChannelParentListener.h"
#include "nsHttpHandler.h"
#include "nsNetUtil.h"
#include "nsISupportsPriority.h"
#include "nsIAuthPromptProvider.h"
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
#include "nsICachingChannel.h"
#include "mozilla/LoadInfo.h"
#include "nsQueryObject.h"
#include "mozilla/BasePrincipal.h"
#include "nsCORSListenerProxy.h"
#include "nsIIPCSerializableInputStream.h"
#include "nsIPrompt.h"
#include "nsIWindowWatcher.h"
#include "nsIDocument.h"
#include "nsStringStream.h"
#include "nsQueryObject.h"
#include "nsIURIClassifier.h"

using mozilla::BasePrincipal;
using namespace mozilla::dom;
using namespace mozilla::ipc;

namespace mozilla {
namespace net {

HttpChannelParent::HttpChannelParent(const PBrowserOrId& iframeEmbedding,
                                     nsILoadContext* aLoadContext,
                                     PBOverrideStatus aOverrideStatus)
  : mIPCClosed(false)
  , mIgnoreProgress(false)
  , mSentRedirect1Begin(false)
  , mSentRedirect1BeginFailed(false)
  , mReceivedRedirect2Verify(false)
  , mPBOverride(aOverrideStatus)
  , mLoadContext(aLoadContext)
  , mStatus(NS_OK)
  , mPendingDiversion(false)
  , mDivertingFromChild(false)
  , mDivertedOnStartRequest(false)
  , mSuspendedForDiversion(false)
  , mSuspendAfterSynthesizeResponse(false)
  , mWillSynthesizeResponse(false)
  , mNestedFrameId(0)
{
  LOG(("Creating HttpChannelParent [this=%p]\n", this));

  // Ensure gHttpHandler is initialized: we need the atom table up and running.
  nsCOMPtr<nsIHttpProtocolHandler> dummyInitializer =
    do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http");

  MOZ_ASSERT(gHttpHandler);
  mHttpHandler = gHttpHandler;

  if (iframeEmbedding.type() == PBrowserOrId::TPBrowserParent) {
    mTabParent = static_cast<dom::TabParent*>(iframeEmbedding.get_PBrowserParent());
  } else {
    mNestedFrameId = iframeEmbedding.get_TabId();
  }

  mEventQ = new ChannelEventQueue(static_cast<nsIParentRedirectingChannel*>(this));
}

HttpChannelParent::~HttpChannelParent()
{
  LOG(("Destroying HttpChannelParent [this=%p]\n", this));
}

void
HttpChannelParent::ActorDestroy(ActorDestroyReason why)
{
  // We may still have refcount>0 if nsHttpChannel hasn't called OnStopRequest
  // yet, but child process has crashed.  We must not try to send any more msgs
  // to child, or IPDL will kill chrome process, too.
  mIPCClosed = true;

  // If this is an intercepted channel, we need to make sure that any resources are
  // cleaned up to avoid leaks.
  if (mParentListener) {
    mParentListener->ClearInterceptedChannel();
  }
}

bool
HttpChannelParent::Init(const HttpChannelCreationArgs& aArgs)
{
  LOG(("HttpChannelParent::Init [this=%p]\n", this));
  switch (aArgs.type()) {
  case HttpChannelCreationArgs::THttpChannelOpenArgs:
  {
    const HttpChannelOpenArgs& a = aArgs.get_HttpChannelOpenArgs();
    return DoAsyncOpen(a.uri(), a.original(), a.doc(), a.referrer(),
                       a.referrerPolicy(), a.apiRedirectTo(), a.topWindowURI(),
                       a.loadFlags(), a.requestHeaders(),
                       a.requestMethod(), a.uploadStream(),
                       a.uploadStreamHasHeaders(), a.priority(), a.classOfService(),
                       a.redirectionLimit(), a.allowSTS(),
                       a.thirdPartyFlags(), a.resumeAt(), a.startPos(),
                       a.entityID(), a.chooseApplicationCache(),
                       a.appCacheClientID(), a.allowSpdy(), a.allowAltSvc(), a.beConservative(),
                       a.loadInfo(), a.synthesizedResponseHead(),
                       a.synthesizedSecurityInfoSerialization(),
                       a.cacheKey(), a.requestContextID(), a.preflightArgs(),
                       a.initialRwin(), a.blockAuthPrompt(),
                       a.suspendAfterSynthesizeResponse(),
                       a.allowStaleCacheContent(), a.contentTypeHint(),
                       a.channelId(), a.contentWindowId(), a.preferredAlternativeType());
  }
  case HttpChannelCreationArgs::THttpChannelConnectArgs:
  {
    const HttpChannelConnectArgs& cArgs = aArgs.get_HttpChannelConnectArgs();
    return ConnectChannel(cArgs.registrarId(), cArgs.shouldIntercept());
  }
  default:
    NS_NOTREACHED("unknown open type");
    return false;
  }
}

//-----------------------------------------------------------------------------
// HttpChannelParent::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ADDREF(HttpChannelParent)
NS_IMPL_RELEASE(HttpChannelParent)
NS_INTERFACE_MAP_BEGIN(HttpChannelParent)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsIProgressEventSink)
  NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsIParentChannel)
  NS_INTERFACE_MAP_ENTRY(nsIAuthPromptProvider)
  NS_INTERFACE_MAP_ENTRY(nsIParentRedirectingChannel)
  NS_INTERFACE_MAP_ENTRY(nsIDeprecationWarner)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIParentRedirectingChannel)
  if (aIID.Equals(NS_GET_IID(HttpChannelParent))) {
    foundInterface = static_cast<nsIInterfaceRequestor*>(this);
  } else
NS_INTERFACE_MAP_END

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
  if (XRE_IsParentProcess() &&
      aIID.Equals(NS_GET_IID(nsIAuthPromptProvider))) {
    *result = nullptr;
    return NS_OK;
  }

  // Only support nsILoadContext if child channel's callbacks did too
  if (aIID.Equals(NS_GET_IID(nsILoadContext)) && mLoadContext) {
    nsCOMPtr<nsILoadContext> copy = mLoadContext;
    copy.forget(result);
    return NS_OK;
  }

  if (mTabParent && aIID.Equals(NS_GET_IID(nsIPrompt))) {
    nsCOMPtr<Element> frameElement = mTabParent->GetOwnerElement();
    if (frameElement) {
      nsCOMPtr<nsPIDOMWindowOuter> win =frameElement->OwnerDoc()->GetWindow();
      NS_ENSURE_TRUE(win, NS_ERROR_UNEXPECTED);

      nsresult rv;
      nsCOMPtr<nsIWindowWatcher> wwatch =
        do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);

      if (NS_WARN_IF(!NS_SUCCEEDED(rv))) {
        return rv;
      }

      nsCOMPtr<nsIPrompt> prompt;
      rv = wwatch->GetNewPrompter(win, getter_AddRefs(prompt));
      if (NS_WARN_IF(!NS_SUCCEEDED(rv))) {
        return rv;
      }

      prompt.forget(result);
      return NS_OK;
    }
  }

  return QueryInterface(aIID, result);
}

//-----------------------------------------------------------------------------
// HttpChannelParent::PHttpChannelParent
//-----------------------------------------------------------------------------

void
HttpChannelParent::InvokeAsyncOpen(nsresult rv)
{
  if (NS_FAILED(rv)) {
    Unused << SendFailedAsyncOpen(rv);
    return;
  }

  nsCOMPtr<nsILoadInfo> loadInfo;
  rv = mChannel->GetLoadInfo(getter_AddRefs(loadInfo));
  if (NS_FAILED(rv)) {
    Unused << SendFailedAsyncOpen(rv);
    return;
  }
  if (loadInfo && loadInfo->GetEnforceSecurity()) {
    rv = mChannel->AsyncOpen2(mParentListener);
  }
  else {
    rv = mChannel->AsyncOpen(mParentListener, nullptr);
  }
  if (NS_FAILED(rv)) {
    Unused << SendFailedAsyncOpen(rv);
  }
}

namespace {
class InvokeAsyncOpen : public Runnable
{
  nsMainThreadPtrHandle<nsIInterfaceRequestor> mChannel;
  nsresult mStatus;
public:
  InvokeAsyncOpen(const nsMainThreadPtrHandle<nsIInterfaceRequestor>& aChannel,
                  nsresult aStatus)
  : mChannel(aChannel)
  , mStatus(aStatus)
  {
  }

  NS_IMETHOD Run()
  {
    RefPtr<HttpChannelParent> channel = do_QueryObject(mChannel.get());
    channel->InvokeAsyncOpen(mStatus);
    return NS_OK;
  }
};

struct UploadStreamClosure {
  nsMainThreadPtrHandle<nsIInterfaceRequestor> mChannel;

  explicit UploadStreamClosure(const nsMainThreadPtrHandle<nsIInterfaceRequestor>& aChannel)
  : mChannel(aChannel)
  {
  }
};

void
UploadCopyComplete(void* aClosure, nsresult aStatus) {
  // Called on the Stream Transport Service thread by NS_AsyncCopy
  MOZ_ASSERT(!NS_IsMainThread());
  UniquePtr<UploadStreamClosure> closure(static_cast<UploadStreamClosure*>(aClosure));
  nsCOMPtr<nsIRunnable> event = new InvokeAsyncOpen(closure->mChannel, aStatus);
  NS_DispatchToMainThread(event);
}
} // anonymous namespace

bool
HttpChannelParent::DoAsyncOpen(  const URIParams&           aURI,
                                 const OptionalURIParams&   aOriginalURI,
                                 const OptionalURIParams&   aDocURI,
                                 const OptionalURIParams&   aReferrerURI,
                                 const uint32_t&            aReferrerPolicy,
                                 const OptionalURIParams&   aAPIRedirectToURI,
                                 const OptionalURIParams&   aTopWindowURI,
                                 const uint32_t&            aLoadFlags,
                                 const RequestHeaderTuples& requestHeaders,
                                 const nsCString&           requestMethod,
                                 const OptionalIPCStream&   uploadStream,
                                 const bool&                uploadStreamHasHeaders,
                                 const int16_t&             priority,
                                 const uint32_t&            classOfService,
                                 const uint8_t&             redirectionLimit,
                                 const bool&                allowSTS,
                                 const uint32_t&            thirdPartyFlags,
                                 const bool&                doResumeAt,
                                 const uint64_t&            startPos,
                                 const nsCString&           entityID,
                                 const bool&                chooseApplicationCache,
                                 const nsCString&           appCacheClientID,
                                 const bool&                allowSpdy,
                                 const bool&                allowAltSvc,
                                 const bool&                beConservative,
                                 const OptionalLoadInfoArgs& aLoadInfoArgs,
                                 const OptionalHttpResponseHead& aSynthesizedResponseHead,
                                 const nsCString&           aSecurityInfoSerialization,
                                 const uint32_t&            aCacheKey,
                                 const nsCString&           aRequestContextID,
                                 const OptionalCorsPreflightArgs& aCorsPreflightArgs,
                                 const uint32_t&            aInitialRwin,
                                 const bool&                aBlockAuthPrompt,
                                 const bool&                aSuspendAfterSynthesizeResponse,
                                 const bool&                aAllowStaleCacheContent,
                                 const nsCString&           aContentTypeHint,
                                 const nsCString&           aChannelId,
                                 const uint64_t&            aContentWindowId,
                                 const nsCString&           aPreferredAlternativeType)
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
  nsCOMPtr<nsIURI> topWindowUri = DeserializeURI(aTopWindowURI);

  LOG(("HttpChannelParent RecvAsyncOpen [this=%p uri=%s]\n",
       this, uri->GetSpecOrDefault().get()));

  nsresult rv;

  nsCOMPtr<nsIIOService> ios(do_GetIOService(&rv));
  if (NS_FAILED(rv))
    return SendFailedAsyncOpen(rv);

  nsCOMPtr<nsILoadInfo> loadInfo;
  rv = mozilla::ipc::LoadInfoArgsToLoadInfo(aLoadInfoArgs,
                                            getter_AddRefs(loadInfo));
  if (NS_FAILED(rv)) {
    return SendFailedAsyncOpen(rv);
  }

  OriginAttributes attrs;
  rv = loadInfo->GetOriginAttributes(&attrs);
  if (NS_FAILED(rv)) {
    return SendFailedAsyncOpen(rv);
  }

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannelInternal(getter_AddRefs(channel), uri, loadInfo,
                             nullptr, nullptr, aLoadFlags, ios);
  if (NS_FAILED(rv)) {
    return SendFailedAsyncOpen(rv);
  }

  mChannel = do_QueryObject(channel, &rv);
  if (NS_FAILED(rv)) {
    return SendFailedAsyncOpen(rv);
  }

  // Set the channelId allocated in child to the parent instance
  mChannel->SetChannelId(aChannelId);
  mChannel->SetTopLevelContentWindowId(aContentWindowId);

  mChannel->SetWarningReporter(this);
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
    mChannel->SetReferrerWithPolicyInternal(referrerUri, aReferrerPolicy);
  if (apiRedirectToUri)
    mChannel->RedirectTo(apiRedirectToUri);
  if (topWindowUri)
    mChannel->SetTopWindowURI(topWindowUri);
  if (aLoadFlags != nsIRequest::LOAD_NORMAL)
    mChannel->SetLoadFlags(aLoadFlags);

  for (uint32_t i = 0; i < requestHeaders.Length(); i++) {
    if (requestHeaders[i].mEmpty) {
      mChannel->SetEmptyRequestHeader(requestHeaders[i].mHeader);
    } else {
      mChannel->SetRequestHeader(requestHeaders[i].mHeader,
                                 requestHeaders[i].mValue,
                                 requestHeaders[i].mMerge);
    }
  }

  mParentListener = new HttpChannelParentListener(this);

  mChannel->SetNotificationCallbacks(mParentListener);

  mChannel->SetRequestMethod(nsDependentCString(requestMethod.get()));

  if (aCorsPreflightArgs.type() == OptionalCorsPreflightArgs::TCorsPreflightArgs) {
    const CorsPreflightArgs& args = aCorsPreflightArgs.get_CorsPreflightArgs();
    mChannel->SetCorsPreflightParameters(args.unsafeHeaders());
  }

  bool delayAsyncOpen = false;
  nsCOMPtr<nsIInputStream> stream = DeserializeIPCStream(uploadStream);
  if (stream) {
    // FIXME: The fast path of using the existing stream currently only applies to streams
    //   that have had their entire contents serialized from the child at this point.
    //   Once bug 1294446 and bug 1294450 are fixed it is worth revisiting this heuristic.
    nsCOMPtr<nsIIPCSerializableInputStream> completeStream = do_QueryInterface(stream);
    if (!completeStream) {
      delayAsyncOpen = true;

      // buffer size matches PSendStream transfer size.
      const uint32_t kBufferSize = 32768;

      nsCOMPtr<nsIStorageStream> storageStream;
      nsresult rv = NS_NewStorageStream(kBufferSize, UINT32_MAX,
                                        getter_AddRefs(storageStream));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return SendFailedAsyncOpen(rv);
      }

      nsCOMPtr<nsIInputStream> newUploadStream;
      rv = storageStream->NewInputStream(0, getter_AddRefs(newUploadStream));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return SendFailedAsyncOpen(rv);
      }

      nsCOMPtr<nsIOutputStream> sink;
      rv = storageStream->GetOutputStream(0, getter_AddRefs(sink));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return SendFailedAsyncOpen(rv);
      }

      nsCOMPtr<nsIEventTarget> target =
          do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID, &rv);
      if (NS_FAILED(rv) || !target) {
        return SendFailedAsyncOpen(rv);
      }

      nsCOMPtr<nsIInterfaceRequestor> iir = static_cast<nsIInterfaceRequestor*>(this);
      nsMainThreadPtrHandle<nsIInterfaceRequestor> handle =
          nsMainThreadPtrHandle<nsIInterfaceRequestor>(
              new nsMainThreadPtrHolder<nsIInterfaceRequestor>(iir));
      UniquePtr<UploadStreamClosure> closure(new UploadStreamClosure(handle));

      // Accumulate the stream contents as the child sends it. We will continue with
      // the AsyncOpen process once the full stream has been received.
      rv = NS_AsyncCopy(stream, sink, target, NS_ASYNCCOPY_VIA_READSEGMENTS,
                        kBufferSize, // copy segment size
                        UploadCopyComplete, closure.release());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return SendFailedAsyncOpen(rv);
      }

      mChannel->InternalSetUploadStream(newUploadStream);
    } else {
      mChannel->InternalSetUploadStream(stream);
    }
    mChannel->SetUploadStreamHasHeaders(uploadStreamHasHeaders);
  }

  if (aSynthesizedResponseHead.type() == OptionalHttpResponseHead::TnsHttpResponseHead) {
    mParentListener->SetupInterception(aSynthesizedResponseHead.get_nsHttpResponseHead());
    mWillSynthesizeResponse = true;
    mChannel->SetCouldBeSynthesized();

    if (!aSecurityInfoSerialization.IsEmpty()) {
      nsCOMPtr<nsISupports> secInfo;
      NS_DeserializeObject(aSecurityInfoSerialization, getter_AddRefs(secInfo));
      mChannel->OverrideSecurityInfo(secInfo);
    }
  } else {
    nsLoadFlags newLoadFlags;
    mChannel->GetLoadFlags(&newLoadFlags);
    newLoadFlags |= nsIChannel::LOAD_BYPASS_SERVICE_WORKER;
    mChannel->SetLoadFlags(newLoadFlags);
  }

  nsCOMPtr<nsISupportsPRUint32> cacheKey =
    do_CreateInstance(NS_SUPPORTS_PRUINT32_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    return SendFailedAsyncOpen(rv);
  }

  rv = cacheKey->SetData(aCacheKey);
  if (NS_FAILED(rv)) {
    return SendFailedAsyncOpen(rv);
  }

  mChannel->SetCacheKey(cacheKey);
  mChannel->PreferAlternativeDataType(aPreferredAlternativeType);

  mChannel->SetAllowStaleCacheContent(aAllowStaleCacheContent);

  mChannel->SetContentType(aContentTypeHint);

  if (priority != nsISupportsPriority::PRIORITY_NORMAL) {
    mChannel->SetPriority(priority);
  }
  if (classOfService) {
    mChannel->SetClassFlags(classOfService);
  }
  mChannel->SetRedirectionLimit(redirectionLimit);
  mChannel->SetAllowSTS(allowSTS);
  mChannel->SetThirdPartyFlags(thirdPartyFlags);
  mChannel->SetAllowSpdy(allowSpdy);
  mChannel->SetAllowAltSvc(allowAltSvc);
  mChannel->SetBeConservative(beConservative);
  mChannel->SetInitialRwin(aInitialRwin);
  mChannel->SetBlockAuthPrompt(aBlockAuthPrompt);

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
      OriginAttributes attrs;
      NS_GetOriginAttributes(mChannel, attrs);

      nsCOMPtr<nsIPrincipal> principal =
        BasePrincipal::CreateCodebasePrincipal(uri, attrs);

      bool chooseAppCache = false;
      // This works because we've already called SetNotificationCallbacks and
      // done mPBOverride logic by this point.
      chooseAppCache = NS_ShouldCheckAppCache(principal, NS_UsePrivateBrowsing(mChannel));

      appCacheChan->SetChooseApplicationCache(chooseAppCache);
    }
  }

  nsID requestContextID;
  requestContextID.Parse(aRequestContextID.BeginReading());
  mChannel->SetRequestContextID(requestContextID);

  mSuspendAfterSynthesizeResponse = aSuspendAfterSynthesizeResponse;

  if (!delayAsyncOpen) {
    InvokeAsyncOpen(NS_OK);
  }

  return true;
}

bool
HttpChannelParent::ConnectChannel(const uint32_t& registrarId, const bool& shouldIntercept)
{
  nsresult rv;

  LOG(("HttpChannelParent::ConnectChannel: Looking for a registered channel "
       "[this=%p, id=%" PRIu32 "]\n", this, registrarId));
  nsCOMPtr<nsIChannel> channel;
  rv = NS_LinkRedirectChannels(registrarId, this, getter_AddRefs(channel));
  if (NS_FAILED(rv)) {
    NS_ERROR("Could not find the http channel to connect its IPC parent");
    // This makes the channel delete itself safely.  It's the only thing
    // we can do now, since this parent channel cannot be used and there is
    // no other way to tell the child side there were something wrong.
    Delete();
    return true;
  }

  LOG(("  found channel %p, rv=%08" PRIx32, channel.get(), static_cast<uint32_t>(rv)));
  mChannel = do_QueryObject(channel);
  if (!mChannel) {
    LOG(("  but it's not nsHttpChannel"));
    Delete();
    return true;
  }

  nsCOMPtr<nsINetworkInterceptController> controller;
  NS_QueryNotificationCallbacks(channel, controller);
  RefPtr<HttpChannelParentListener> parentListener = do_QueryObject(controller);
  MOZ_ASSERT(parentListener);
  parentListener->SetupInterceptionAfterRedirect(shouldIntercept);

  if (mPBOverride != kPBOverride_Unset) {
    // redirected-to channel may not support PB
    nsCOMPtr<nsIPrivateBrowsingChannel> pbChannel = do_QueryObject(mChannel);
    if (pbChannel) {
      pbChannel->SetPrivate(mPBOverride == kPBOverride_Private ? true : false);
    }
  }

  return true;
}

mozilla::ipc::IPCResult
HttpChannelParent::RecvSetPriority(const int16_t& priority)
{
  LOG(("HttpChannelParent::RecvSetPriority [this=%p, priority=%d]\n",
       this, priority));

  if (mChannel) {
    mChannel->SetPriority(priority);
  }

  nsCOMPtr<nsISupportsPriority> priorityRedirectChannel =
      do_QueryInterface(mRedirectChannel);
  if (priorityRedirectChannel)
    priorityRedirectChannel->SetPriority(priority);

  return IPC_OK();
}

mozilla::ipc::IPCResult
HttpChannelParent::RecvSetClassOfService(const uint32_t& cos)
{
  if (mChannel) {
    mChannel->SetClassFlags(cos);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
HttpChannelParent::RecvSuspend()
{
  LOG(("HttpChannelParent::RecvSuspend [this=%p]\n", this));

  if (mChannel) {
    mChannel->Suspend();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
HttpChannelParent::RecvResume()
{
  LOG(("HttpChannelParent::RecvResume [this=%p]\n", this));

  if (mChannel) {
    mChannel->Resume();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
HttpChannelParent::RecvCancel(const nsresult& status)
{
  LOG(("HttpChannelParent::RecvCancel [this=%p]\n", this));

  // May receive cancel before channel has been constructed!
  if (mChannel) {
    mChannel->Cancel(status);
  }
  return IPC_OK();
}


mozilla::ipc::IPCResult
HttpChannelParent::RecvSetCacheTokenCachedCharset(const nsCString& charset)
{
  if (mCacheEntry)
    mCacheEntry->SetMetaDataElement("charset", charset.get());
  return IPC_OK();
}

mozilla::ipc::IPCResult
HttpChannelParent::RecvUpdateAssociatedContentSecurity(const int32_t& broken,
                                                       const int32_t& no)
{
  if (mAssociatedContentSecurity) {
    mAssociatedContentSecurity->SetCountSubRequestsBrokenSecurity(broken);
    mAssociatedContentSecurity->SetCountSubRequestsNoSecurity(no);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
HttpChannelParent::RecvRedirect2Verify(const nsresult& result,
                                       const RequestHeaderTuples& changedHeaders,
                                       const uint32_t& loadFlags,
                                       const uint32_t& referrerPolicy,
                                       const OptionalURIParams& aReferrerURI,
                                       const OptionalURIParams& aAPIRedirectURI,
                                       const OptionalCorsPreflightArgs& aCorsPreflightArgs,
                                       const bool& aForceHSTSPriming,
                                       const bool& aMixedContentWouldBlock,
                                       const bool& aChooseAppcache)
{
  LOG(("HttpChannelParent::RecvRedirect2Verify [this=%p result=%" PRIx32 "]\n",
       this, static_cast<uint32_t>(result)));
  nsresult rv;
  if (NS_SUCCEEDED(result)) {
    nsCOMPtr<nsIHttpChannel> newHttpChannel =
        do_QueryInterface(mRedirectChannel);

    if (newHttpChannel) {
      nsCOMPtr<nsIURI> apiRedirectUri = DeserializeURI(aAPIRedirectURI);

      if (apiRedirectUri)
        newHttpChannel->RedirectTo(apiRedirectUri);

      for (uint32_t i = 0; i < changedHeaders.Length(); i++) {
        if (changedHeaders[i].mEmpty) {
          newHttpChannel->SetEmptyRequestHeader(changedHeaders[i].mHeader);
        } else {
          newHttpChannel->SetRequestHeader(changedHeaders[i].mHeader,
                                           changedHeaders[i].mValue,
                                           changedHeaders[i].mMerge);
        }
      }

      // A successfully redirected channel must have the LOAD_REPLACE flag.
      MOZ_ASSERT(loadFlags & nsIChannel::LOAD_REPLACE);
      if (loadFlags & nsIChannel::LOAD_REPLACE) {
        newHttpChannel->SetLoadFlags(loadFlags);
      }

      if (aCorsPreflightArgs.type() == OptionalCorsPreflightArgs::TCorsPreflightArgs) {
        nsCOMPtr<nsIHttpChannelInternal> newInternalChannel =
          do_QueryInterface(newHttpChannel);
        MOZ_RELEASE_ASSERT(newInternalChannel);
        const CorsPreflightArgs& args = aCorsPreflightArgs.get_CorsPreflightArgs();
        newInternalChannel->SetCorsPreflightParameters(args.unsafeHeaders());
      }

      if (aForceHSTSPriming) {
        nsCOMPtr<nsILoadInfo> newLoadInfo;
        rv = newHttpChannel->GetLoadInfo(getter_AddRefs(newLoadInfo));
        if (NS_SUCCEEDED(rv) && newLoadInfo) {
          newLoadInfo->SetHSTSPriming(aMixedContentWouldBlock);
        }
      }

      nsCOMPtr<nsIURI> referrerUri = DeserializeURI(aReferrerURI);
      newHttpChannel->SetReferrerWithPolicy(referrerUri, referrerPolicy);

      nsCOMPtr<nsIApplicationCacheChannel> appCacheChannel =
        do_QueryInterface(newHttpChannel);
      if (appCacheChannel) {
        appCacheChannel->SetChooseApplicationCache(aChooseAppcache);
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
    LOG(("HttpChannelParent::RecvRedirect2Verify call OnRedirectVerifyCallback"
         " [this=%p result=%" PRIx32 ", mRedirectCallback=%p]\n",
         this, static_cast<uint32_t>(result), mRedirectCallback.get()));
    mRedirectCallback->OnRedirectVerifyCallback(result);
    mRedirectCallback = nullptr;
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
HttpChannelParent::RecvDocumentChannelCleanup()
{
  // From now on only using mAssociatedContentSecurity.  Free everything else.
  mChannel = nullptr;          // Reclaim some memory sooner.
  mCacheEntry = nullptr;  // Else we'll block other channels reading same URI
  return IPC_OK();
}

mozilla::ipc::IPCResult
HttpChannelParent::RecvMarkOfflineCacheEntryAsForeign()
{
  if (mOfflineForeignMarker) {
    mOfflineForeignMarker->MarkAsForeign();
    mOfflineForeignMarker = 0;
  }

  return IPC_OK();
}

class DivertDataAvailableEvent : public ChannelEvent
{
public:
  DivertDataAvailableEvent(HttpChannelParent* aParent,
                           const nsCString& data,
                           const uint64_t& offset,
                           const uint32_t& count)
  : mParent(aParent)
  , mData(data)
  , mOffset(offset)
  , mCount(count)
  {
  }

  void Run()
  {
    mParent->DivertOnDataAvailable(mData, mOffset, mCount);
  }

private:
  HttpChannelParent* mParent;
  nsCString mData;
  uint64_t mOffset;
  uint32_t mCount;
};

mozilla::ipc::IPCResult
HttpChannelParent::RecvDivertOnDataAvailable(const nsCString& data,
                                             const uint64_t& offset,
                                             const uint32_t& count)
{
  LOG(("HttpChannelParent::RecvDivertOnDataAvailable [this=%p]\n", this));

  MOZ_ASSERT(mParentListener);
  if (NS_WARN_IF(!mDivertingFromChild)) {
    MOZ_ASSERT(mDivertingFromChild,
               "Cannot RecvDivertOnDataAvailable if diverting is not set!");
    FailDiversion(NS_ERROR_UNEXPECTED);
    return IPC_FAIL_NO_REASON(this);
  }

  // Drop OnDataAvailables if the parent was canceled already.
  if (NS_FAILED(mStatus)) {
    return IPC_OK();
  }

  mEventQ->RunOrEnqueue(new DivertDataAvailableEvent(this, data, offset,
                                                     count));
  return IPC_OK();
}

void
HttpChannelParent::DivertOnDataAvailable(const nsCString& data,
                                         const uint64_t& offset,
                                         const uint32_t& count)
{
  LOG(("HttpChannelParent::DivertOnDataAvailable [this=%p]\n", this));

  MOZ_ASSERT(mParentListener);
  if (NS_WARN_IF(!mDivertingFromChild)) {
    MOZ_ASSERT(mDivertingFromChild,
               "Cannot DivertOnDataAvailable if diverting is not set!");
    FailDiversion(NS_ERROR_UNEXPECTED);
    return;
  }

  // Drop OnDataAvailables if the parent was canceled already.
  if (NS_FAILED(mStatus)) {
    return;
  }

  nsCOMPtr<nsIInputStream> stringStream;
  nsresult rv = NS_NewByteInputStream(getter_AddRefs(stringStream), data.get(),
                                      count, NS_ASSIGNMENT_DEPEND);
  if (NS_FAILED(rv)) {
    if (mChannel) {
      mChannel->Cancel(rv);
    }
    mStatus = rv;
    return;
  }

  AutoEventEnqueuer ensureSerialDispatch(mEventQ);

  rv = mParentListener->OnDataAvailable(mChannel, nullptr, stringStream,
                                        offset, count);
  stringStream->Close();
  if (NS_FAILED(rv)) {
    if (mChannel) {
      mChannel->Cancel(rv);
    }
    mStatus = rv;
  }
}

class DivertStopRequestEvent : public ChannelEvent
{
public:
  DivertStopRequestEvent(HttpChannelParent* aParent,
                         const nsresult& statusCode)
  : mParent(aParent)
  , mStatusCode(statusCode)
  {
  }

  void Run() {
    mParent->DivertOnStopRequest(mStatusCode);
  }

private:
  HttpChannelParent* mParent;
  nsresult mStatusCode;
};

mozilla::ipc::IPCResult
HttpChannelParent::RecvDivertOnStopRequest(const nsresult& statusCode)
{
  LOG(("HttpChannelParent::RecvDivertOnStopRequest [this=%p]\n", this));

  MOZ_ASSERT(mParentListener);
  if (NS_WARN_IF(!mDivertingFromChild)) {
    MOZ_ASSERT(mDivertingFromChild,
               "Cannot RecvDivertOnStopRequest if diverting is not set!");
    FailDiversion(NS_ERROR_UNEXPECTED);
    return IPC_FAIL_NO_REASON(this);
  }

  mEventQ->RunOrEnqueue(new DivertStopRequestEvent(this, statusCode));
  return IPC_OK();
}

void
HttpChannelParent::DivertOnStopRequest(const nsresult& statusCode)
{
  LOG(("HttpChannelParent::DivertOnStopRequest [this=%p]\n", this));

  MOZ_ASSERT(mParentListener);
  if (NS_WARN_IF(!mDivertingFromChild)) {
    MOZ_ASSERT(mDivertingFromChild,
               "Cannot DivertOnStopRequest if diverting is not set!");
    FailDiversion(NS_ERROR_UNEXPECTED);
    return;
  }

  // Honor the channel's status even if the underlying transaction completed.
  nsresult status = NS_FAILED(mStatus) ? mStatus : statusCode;

  // Reset fake pending status in case OnStopRequest has already been called.
  if (mChannel) {
    mChannel->ForcePending(false);
  }

  AutoEventEnqueuer ensureSerialDispatch(mEventQ);
  mParentListener->OnStopRequest(mChannel, nullptr, status);
}

class DivertCompleteEvent : public ChannelEvent
{
public:
  explicit DivertCompleteEvent(HttpChannelParent* aParent)
  : mParent(aParent)
  {
  }

  void Run() {
    mParent->DivertComplete();
  }

private:
  HttpChannelParent* mParent;
};

mozilla::ipc::IPCResult
HttpChannelParent::RecvDivertComplete()
{
  LOG(("HttpChannelParent::RecvDivertComplete [this=%p]\n", this));

  MOZ_ASSERT(mParentListener);
  if (NS_WARN_IF(!mDivertingFromChild)) {
    MOZ_ASSERT(mDivertingFromChild,
               "Cannot RecvDivertComplete if diverting is not set!");
    FailDiversion(NS_ERROR_UNEXPECTED);
    return IPC_FAIL_NO_REASON(this);
  }

  mEventQ->RunOrEnqueue(new DivertCompleteEvent(this));
  return IPC_OK();
}

void
HttpChannelParent::DivertComplete()
{
  LOG(("HttpChannelParent::DivertComplete [this=%p]\n", this));

  MOZ_ASSERT(mParentListener);
  if (NS_WARN_IF(!mDivertingFromChild)) {
    MOZ_ASSERT(mDivertingFromChild,
               "Cannot DivertComplete if diverting is not set!");
    FailDiversion(NS_ERROR_UNEXPECTED);
    return;
  }

  nsresult rv = ResumeForDiversion();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    FailDiversion(NS_ERROR_UNEXPECTED);
    return;
  }

  mParentListener = nullptr;
}

void
HttpChannelParent::MaybeFlushPendingDiversion()
{
  if (!mPendingDiversion) {
    return;
  }

  mPendingDiversion = false;

  nsresult rv = SuspendForDiversion();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  if (mDivertListener) {
    DivertTo(mDivertListener);
  }

  return;
}

void
HttpChannelParent::ResponseSynthesized()
{
  // Suspend now even though the FinishSynthesizeResponse runnable has
  // not executed.  We want to suspend after we get far enough to trigger
  // the synthesis, but not actually allow the nsHttpChannel to trigger
  // any OnStartRequests().
  if (mSuspendAfterSynthesizeResponse) {
    mChannel->Suspend();
  }

  mWillSynthesizeResponse = false;

  MaybeFlushPendingDiversion();
}

mozilla::ipc::IPCResult
HttpChannelParent::RecvRemoveCorsPreflightCacheEntry(const URIParams& uri,
  const mozilla::ipc::PrincipalInfo& requestingPrincipal)
{
  nsCOMPtr<nsIURI> deserializedURI = DeserializeURI(uri);
  if (!deserializedURI) {
    return IPC_FAIL_NO_REASON(this);
  }
  nsCOMPtr<nsIPrincipal> principal =
    PrincipalInfoToPrincipal(requestingPrincipal);
  if (!principal) {
    return IPC_FAIL_NO_REASON(this);
  }
  nsCORSListenerProxy::RemoveFromCorsPreflightCache(deserializedURI,
                                                    principal);
  return IPC_OK();
}

//-----------------------------------------------------------------------------
// HttpChannelParent::nsIRequestObserver
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParent::OnStartRequest(nsIRequest *aRequest, nsISupports *aContext)
{
  LOG(("HttpChannelParent::OnStartRequest [this=%p, aRequest=%p]\n",
       this, aRequest));

  MOZ_RELEASE_ASSERT(!mDivertingFromChild,
    "Cannot call OnStartRequest if diverting is set!");

  RefPtr<nsHttpChannel> chan = do_QueryObject(aRequest);
  if (!chan) {
    LOG(("  aRequest is not nsHttpChannel"));
    NS_ERROR("Expecting only nsHttpChannel as aRequest in HttpChannelParent::OnStartRequest");
    return NS_ERROR_UNEXPECTED;
  }

  MOZ_ASSERT(mChannel == chan,
             "HttpChannelParent getting OnStartRequest from a different nsHttpChannel instance");

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
  UpdateAndSerializeSecurityInfo(secInfoSerialization);

  uint16_t redirectCount = 0;
  chan->GetRedirectCount(&redirectCount);

  nsCOMPtr<nsISupports> cacheKey;
  chan->GetCacheKey(getter_AddRefs(cacheKey));
  uint32_t cacheKeyValue = 0;
  if (cacheKey) {
    nsCOMPtr<nsISupportsPRUint32> container = do_QueryInterface(cacheKey);
    if (!container) {
      return NS_ERROR_ILLEGAL_VALUE;
    }

    nsresult rv = container->GetData(&cacheKeyValue);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  nsAutoCString altDataType;
  chan->GetAlternativeDataType(altDataType);

  // !!! We need to lock headers and please don't forget to unlock them !!!
  requestHead->Enter();
  nsresult rv = NS_OK;
  if (mIPCClosed ||
      !SendOnStartRequest(channelStatus,
                          responseHead ? *responseHead : nsHttpResponseHead(),
                          !!responseHead,
                          requestHead->Headers(),
                          isFromCache,
                          mCacheEntry ? true : false,
                          expirationTime, cachedCharset, secInfoSerialization,
                          chan->GetSelfAddr(), chan->GetPeerAddr(),
                          redirectCount,
                          cacheKeyValue,
                          altDataType))
  {
    rv = NS_ERROR_UNEXPECTED;
  }
  requestHead->Exit();
  return rv;
}

NS_IMETHODIMP
HttpChannelParent::OnStopRequest(nsIRequest *aRequest,
                                 nsISupports *aContext,
                                 nsresult aStatusCode)
{
  LOG(("HttpChannelParent::OnStopRequest: [this=%p aRequest=%p status=%" PRIx32 "]\n",
       this, aRequest, static_cast<uint32_t>(aStatusCode)));

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
  mChannel->GetTransferSize(&timing.transferSize);
  mChannel->GetEncodedBodySize(&timing.encodedBodySize);
  // decodedBodySize can be computed in the child process so it doesn't need
  // to be passed down.
  mChannel->GetProtocolVersion(timing.protocolVersion);

  mChannel->GetCacheReadStart(&timing.cacheReadStart);
  mChannel->GetCacheReadEnd(&timing.cacheReadEnd);

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
  LOG(("HttpChannelParent::OnDataAvailable [this=%p aRequest=%p]\n",
       this, aRequest));

  MOZ_RELEASE_ASSERT(!mDivertingFromChild,
    "Cannot call OnDataAvailable if diverting is set!");

  nsresult channelStatus = NS_OK;
  mChannel->GetStatus(&channelStatus);

  nsresult transportStatus =
    (mChannel->IsReadingFromCache()) ? NS_NET_STATUS_READING
                                     : NS_NET_STATUS_RECEIVING_FROM;

  static uint32_t const kCopyChunkSize = 128 * 1024;
  uint32_t toRead = std::min<uint32_t>(aCount, kCopyChunkSize);

  nsCString data;
  if (!data.SetCapacity(toRead, fallible)) {
    LOG(("  out of memory!"));
    return NS_ERROR_OUT_OF_MEMORY;
  }

  while (aCount) {
    nsresult rv = NS_ReadInputStreamToString(aInputStream, data, toRead);
    if (NS_FAILED(rv)) {
      return rv;
    }

    if (mIPCClosed || !SendOnTransportAndData(channelStatus, transportStatus,
                                              aOffset, toRead, data)) {
      return NS_ERROR_UNEXPECTED;
    }

    aOffset += toRead;
    aCount -= toRead;
    toRead = std::min<uint32_t>(aCount, kCopyChunkSize);
  }

  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelParent::nsIProgressEventSink
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParent::OnProgress(nsIRequest *aRequest,
                              nsISupports *aContext,
                              int64_t aProgress,
                              int64_t aProgressMax)
{
  // If it indicates this precedes OnDataAvailable, child can derive the value in ODA.
  if (mIgnoreProgress) {
    mIgnoreProgress = false;
    return NS_OK;
  }

  // Send OnProgress events to the child for data upload progress notifications
  // (i.e. status == NS_NET_STATUS_SENDING_TO) or if the channel has
  // LOAD_BACKGROUND set.
  if (mIPCClosed || !SendOnProgress(aProgress, aProgressMax)) {
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
  // If this precedes OnDataAvailable, transportStatus will be derived in ODA.
  if (aStatus == NS_NET_STATUS_RECEIVING_FROM ||
      aStatus == NS_NET_STATUS_READING) {
    // The transport status and progress generated by ODA will be coalesced
    // into one IPC message. Therefore, we can ignore the next OnProgress event
    // since it is generated by ODA as well.
    mIgnoreProgress = true;
    return NS_OK;
  }

  // Otherwise, send to child now
  if (mIPCClosed || !SendOnStatus(aStatus)) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelParent::nsIParentChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParent::SetParentListener(HttpChannelParentListener* aListener)
{
  LOG(("HttpChannelParent::SetParentListener [this=%p aListener=%p]\n",
       this, aListener));
  MOZ_ASSERT(aListener);
  MOZ_ASSERT(!mParentListener, "SetParentListener should only be called for "
                               "new HttpChannelParents after a redirect, when "
                               "mParentListener is null.");
  mParentListener = aListener;
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelParent::NotifyTrackingProtectionDisabled()
{
  if (!mIPCClosed)
    Unused << SendNotifyTrackingProtectionDisabled();
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelParent::SetClassifierMatchedInfo(const nsACString& aList,
                                            const nsACString& aProvider,
                                            const nsACString& aPrefix)
{
  if (!mIPCClosed) {
    ClassifierInfo info;
    info.list() = aList;
    info.prefix() = aPrefix;
    info.provider() = aProvider;

    Unused << SendSetClassifierMatchedInfo(info);
  }
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelParent::NotifyTrackingResource()
{
  if (!mIPCClosed)
    Unused << SendNotifyTrackingResource();
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelParent::Delete()
{
  if (!mIPCClosed)
    Unused << DoSendDeleteSelf();

  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelParent::nsIParentRedirectingChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParent::StartRedirect(uint32_t registrarId,
                                 nsIChannel* newChannel,
                                 uint32_t redirectFlags,
                                 nsIAsyncVerifyRedirectCallback* callback)
{
  LOG(("HttpChannelParent::StartRedirect [this=%p, registrarId=%" PRIu32 " "
       "newChannel=%p callback=%p]\n", this, registrarId, newChannel,
       callback));

  if (mIPCClosed)
    return NS_BINDING_ABORTED;

  nsCOMPtr<nsIURI> newURI;
  newChannel->GetURI(getter_AddRefs(newURI));

  URIParams uriParams;
  SerializeURI(newURI, uriParams);

  nsCString secInfoSerialization;
  UpdateAndSerializeSecurityInfo(secInfoSerialization);

  // If the channel is a HTTP channel, we also want to inform the child
  // about the parent's channelId attribute, so that both parent and child
  // share the same ID. Useful for monitoring channel activity in devtools.
  nsAutoCString channelId;
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(newChannel);
  if (httpChannel) {
    nsresult rv = httpChannel->GetChannelId(channelId);
    NS_ENSURE_SUCCESS(rv, NS_BINDING_ABORTED);
  }

  nsHttpResponseHead *responseHead = mChannel->GetResponseHead();
  bool result = false;
  if (!mIPCClosed) {
    result = SendRedirect1Begin(registrarId, uriParams, redirectFlags,
                                responseHead ? *responseHead
                                             : nsHttpResponseHead(),
                                secInfoSerialization,
                                channelId);
  }
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
  LOG(("HttpChannelParent::CompleteRedirect [this=%p succeeded=%d]\n",
       this, succeeded));

  if (succeeded && !mIPCClosed) {
    // TODO: check return value: assume child dead if failed
    Unused << SendRedirect3Complete();
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
  LOG(("HttpChannelParent::SuspendForDiversion [this=%p]\n", this));
  MOZ_ASSERT(mChannel);
  MOZ_ASSERT(mParentListener);

  // If we're in the process of opening a synthesized response, we must wait
  // to perform the diversion.  Some of our diversion listeners clear callbacks
  // which breaks the synthesis process.
  if (mWillSynthesizeResponse) {
    mPendingDiversion = true;
    return NS_OK;
  }

  if (NS_WARN_IF(mDivertingFromChild)) {
    MOZ_ASSERT(!mDivertingFromChild, "Already suspended for diversion!");
    return NS_ERROR_UNEXPECTED;
  }

  // MessageDiversionStarted call will suspend mEventQ as many times as the
  // channel has been suspended, so that channel and this queue are in sync.
  mChannel->MessageDiversionStarted(this);

  nsresult rv = NS_OK;

  // Try suspending the channel. Allow it to fail, since OnStopRequest may have
  // been called and thus the channel may not be pending.  If we've already
  // automatically suspended after synthesizing the response, then we don't
  // need to suspend again here.
  if (!mSuspendAfterSynthesizeResponse) {
    // We need to suspend only nsHttpChannel (i.e. we should not suspend
    // mEventQ). Therefore we call mChannel->SuspendInternal() and not
    // mChannel->Suspend().
    // We are suspending only nsHttpChannel here because we want to stop
    // OnDataAvailable until diversion is over. At the same time we should
    // send the diverted OnDataAvailable-s to the listeners and not queue them
    // in mEventQ.
    rv = mChannel->SuspendInternal();
    MOZ_ASSERT(NS_SUCCEEDED(rv) || rv == NS_ERROR_NOT_AVAILABLE);
    mSuspendedForDiversion = NS_SUCCEEDED(rv);
  } else {
    // Take ownership of the automatic suspend that occurred after synthesizing
    // the response.
    mSuspendedForDiversion = true;

    // If mSuspendAfterSynthesizeResponse is true channel has been already
    // suspended. From comment above mSuspendedForDiversion takes the ownership
    // of this suspend, therefore mEventQ should not be suspened so we need to
    // resume it once.
    mEventQ->Resume();
  }

  rv = mParentListener->SuspendForDiversion();
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  // Once this is set, no more OnStart/OnData/OnStop callbacks should be sent
  // to the child.
  mDivertingFromChild = true;

  return NS_OK;
}

nsresult
HttpChannelParent::SuspendMessageDiversion()
{
  LOG(("HttpChannelParent::SuspendMessageDiversion [this=%p]", this));
  // This only needs to suspend message queue.
  mEventQ->Suspend();
  return NS_OK;
}

nsresult
HttpChannelParent::ResumeMessageDiversion()
{
  LOG(("HttpChannelParent::SuspendMessageDiversion [this=%p]", this));
  // This only needs to resumes message queue.
  mEventQ->Resume();
  return NS_OK;
}

/* private, supporting function for ADivertableParentChannel */
nsresult
HttpChannelParent::ResumeForDiversion()
{
  LOG(("HttpChannelParent::ResumeForDiversion [this=%p]\n", this));
  MOZ_ASSERT(mChannel);
  if (NS_WARN_IF(!mDivertingFromChild)) {
    MOZ_ASSERT(mDivertingFromChild,
               "Cannot ResumeForDiversion if not diverting!");
    return NS_ERROR_UNEXPECTED;
  }

  mChannel->MessageDiversionStop();

  if (mSuspendedForDiversion) {
    // The nsHttpChannel will deliver remaining OnData/OnStop for the transfer.
    nsresult rv = mChannel->ResumeInternal();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      FailDiversion(NS_ERROR_UNEXPECTED, true);
      return rv;
    }
    mSuspendedForDiversion = false;
  }

  if (NS_WARN_IF(mIPCClosed || !DoSendDeleteSelf())) {
    FailDiversion(NS_ERROR_UNEXPECTED);
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

void
HttpChannelParent::DivertTo(nsIStreamListener *aListener)
{
  LOG(("HttpChannelParent::DivertTo [this=%p aListener=%p]\n",
       this, aListener));
  MOZ_ASSERT(mParentListener);

  // If we're in the process of opening a synthesized response, we must wait
  // to perform the diversion.  Some of our diversion listeners clear callbacks
  // which breaks the synthesis process.
  if (mWillSynthesizeResponse) {
    // We should already have started pending the diversion when
    // SuspendForDiversion() was called.
    MOZ_ASSERT(mPendingDiversion);
    mDivertListener = aListener;
    return;
  }

  if (NS_WARN_IF(!mDivertingFromChild)) {
    MOZ_ASSERT(mDivertingFromChild,
               "Cannot DivertTo new listener if diverting is not set!");
    return;
  }

  mDivertListener = aListener;

  // Call OnStartRequest and SendDivertMessages asynchronously to avoid
  // reentering client context.
  NS_DispatchToCurrentThread(
    NewRunnableMethod(this, &HttpChannelParent::StartDiversion));
  return;
}

void
HttpChannelParent::StartDiversion()
{
  LOG(("HttpChannelParent::StartDiversion [this=%p]\n", this));
  if (NS_WARN_IF(!mDivertingFromChild)) {
    MOZ_ASSERT(mDivertingFromChild,
               "Cannot StartDiversion if diverting is not set!");
    return;
  }

  // Fake pending status in case OnStopRequest has already been called.
  if (mChannel) {
    mChannel->ForcePending(true);
  }

  {
    AutoEventEnqueuer ensureSerialDispatch(mEventQ);

    // Call OnStartRequest for the "DivertTo" listener.
    nsresult rv = mDivertListener->OnStartRequest(mChannel, nullptr);
    if (NS_FAILED(rv)) {
      if (mChannel) {
        mChannel->Cancel(rv);
      }
      mStatus = rv;
    }
  }
  mDivertedOnStartRequest = true;

  // After OnStartRequest has been called, setup content decoders if needed.
  //
  // Create a content conversion chain based on mDivertListener and update
  // mDivertListener.
  nsCOMPtr<nsIStreamListener> converterListener;
  mChannel->DoApplyContentConversions(mDivertListener,
                                      getter_AddRefs(converterListener));
  if (converterListener) {
    mDivertListener = converterListener.forget();
  }

  // Now mParentListener can be diverted to mDivertListener.
  DebugOnly<nsresult> rvdbg = mParentListener->DivertTo(mDivertListener);
  MOZ_ASSERT(NS_SUCCEEDED(rvdbg));
  mDivertListener = nullptr;

  if (NS_WARN_IF(mIPCClosed || !SendFlushedForDiversion())) {
    FailDiversion(NS_ERROR_UNEXPECTED);
    return;
  }

  // The listener chain should now be setup; tell HttpChannelChild to divert
  // the OnDataAvailables and OnStopRequest to this HttpChannelParent.
  if (NS_WARN_IF(mIPCClosed || !SendDivertMessages())) {
    FailDiversion(NS_ERROR_UNEXPECTED);
    return;
  }
}

class HTTPFailDiversionEvent : public Runnable
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
  NS_IMETHOD Run() override
  {
    mChannelParent->NotifyDiversionFailed(mErrorCode, mSkipResume);
    return NS_OK;
  }
private:
  RefPtr<HttpChannelParent> mChannelParent;
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
  LOG(("HttpChannelParent::NotifyDiversionFailed [this=%p aErrorCode=%" PRIx32 "]\n",
       this, static_cast<uint32_t>(aErrorCode)));
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
    mChannel->ResumeInternal();
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
  mChannel = nullptr;

  if (!mIPCClosed) {
    Unused << DoSendDeleteSelf();
  }
}

nsresult
HttpChannelParent::OpenAlternativeOutputStream(const nsACString & type, nsIOutputStream * *_retval)
{
  // We need to make sure the child does not call SendDocumentChannelCleanup()
  // before opening the altOutputStream, because that clears mCacheEntry.
  if (!mCacheEntry) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return mCacheEntry->OpenAlternativeOutputStream(type, _retval);
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

void
HttpChannelParent::UpdateAndSerializeSecurityInfo(nsACString& aSerializedSecurityInfoOut)
{
  nsCOMPtr<nsISupports> secInfoSupp;
  mChannel->GetSecurityInfo(getter_AddRefs(secInfoSupp));
  if (secInfoSupp) {
    mAssociatedContentSecurity = do_QueryInterface(secInfoSupp);
    nsCOMPtr<nsISerializable> secInfoSer = do_QueryInterface(secInfoSupp);
    if (secInfoSer) {
      NS_SerializeToString(secInfoSer, aSerializedSecurityInfoOut);
    }
  }
}

bool
HttpChannelParent::DoSendDeleteSelf()
{
  bool rv = SendDeleteSelf();
  mIPCClosed = true;
  return rv;
}

mozilla::ipc::IPCResult
HttpChannelParent::RecvDeletingChannel()
{
  // We need to ensure that the parent channel will not be sending any more IPC
  // messages after this, as the child is going away. DoSendDeleteSelf will
  // set mIPCClosed = true;
  if (!DoSendDeleteSelf()) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
HttpChannelParent::RecvFinishInterceptedRedirect()
{
  // We make sure not to send any more messages until the IPC channel is torn
  // down by the child.
  mIPCClosed = true;
  if (!SendFinishInterceptedRedirect()) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

//-----------------------------------------------------------------------------
// HttpChannelSecurityWarningReporter
//-----------------------------------------------------------------------------

nsresult
HttpChannelParent::ReportSecurityMessage(const nsAString& aMessageTag,
                                         const nsAString& aMessageCategory)
{
  if (mIPCClosed ||
      NS_WARN_IF(!SendReportSecurityMessage(nsString(aMessageTag),
                                            nsString(aMessageCategory)))) {
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelParent::IssueWarning(uint32_t aWarning, bool aAsError)
{
  Unused << SendIssueDeprecationWarning(aWarning, aAsError);
  return NS_OK;
}

void
HttpChannelParent::DoSendSetPriority(int16_t aValue)
{
  if (!mIPCClosed) {
    Unused << SendSetPriority(aValue);
  }
}

} // namespace net
} // namespace mozilla
