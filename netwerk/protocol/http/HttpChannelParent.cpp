/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "mozilla/ipc/FileDescriptorSetParent.h"
#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/net/HttpChannelParent.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ServiceWorkerUtils.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/net/NeckoParent.h"
#include "mozilla/InputStreamLengthHelper.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "HttpBackgroundChannelParent.h"
#include "HttpChannelParentListener.h"
#include "nsHttpHandler.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsISupportsPriority.h"
#include "nsIAuthPromptProvider.h"
#include "mozilla/net/BackgroundChannelRegistrar.h"
#include "nsSerializationHelper.h"
#include "nsISerializable.h"
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
#include "mozilla/net/RedirectChannelRegistrar.h"
#include "nsIWindowWatcher.h"
#include "mozilla/dom/Document.h"
#include "nsISecureBrowserUI.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"
#include "nsIStorageStream.h"
#include "nsThreadUtils.h"
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
    : mLoadContext(aLoadContext),
      mNestedFrameId(0),
      mIPCClosed(false),
      mPBOverride(aOverrideStatus),
      mStatus(NS_OK),
      mIgnoreProgress(false),
      mSentRedirect1BeginFailed(false),
      mReceivedRedirect2Verify(false),
      mHasSuspendedByBackPressure(false),
      mPendingDiversion(false),
      mDivertingFromChild(false),
      mDivertedOnStartRequest(false),
      mSuspendedForDiversion(false),
      mSuspendAfterSynthesizeResponse(false),
      mWillSynthesizeResponse(false),
      mCacheNeedFlowControlInitialized(false),
      mNeedFlowControl(true),
      mSuspendedForFlowControl(false),
      mDoingCrossProcessRedirect(false) {
  LOG(("Creating HttpChannelParent [this=%p]\n", this));

  // Ensure gHttpHandler is initialized: we need the atom table up and running.
  nsCOMPtr<nsIHttpProtocolHandler> dummyInitializer =
      do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http");

  MOZ_ASSERT(gHttpHandler);
  mHttpHandler = gHttpHandler;

  if (iframeEmbedding.type() == PBrowserOrId::TPBrowserParent) {
    mBrowserParent =
        static_cast<dom::BrowserParent*>(iframeEmbedding.get_PBrowserParent());
  } else {
    mNestedFrameId = iframeEmbedding.get_TabId();
  }

  mSendWindowSize = gHttpHandler->SendWindowSize();

  mEventQ =
      new ChannelEventQueue(static_cast<nsIParentRedirectingChannel*>(this));
}

HttpChannelParent::~HttpChannelParent() {
  LOG(("Destroying HttpChannelParent [this=%p]\n", this));
  CleanupBackgroundChannel();
}

void HttpChannelParent::ActorDestroy(ActorDestroyReason why) {
  // We may still have refcount>0 if nsHttpChannel hasn't called OnStopRequest
  // yet, but child process has crashed.  We must not try to send any more msgs
  // to child, or IPDL will kill chrome process, too.
  mIPCClosed = true;

  // If this is an intercepted channel, we need to make sure that any resources
  // are cleaned up to avoid leaks.
  if (mParentListener) {
    mParentListener->ClearInterceptedChannel(this);
  }

  CleanupBackgroundChannel();
}

bool HttpChannelParent::Init(const HttpChannelCreationArgs& aArgs) {
  LOG(("HttpChannelParent::Init [this=%p]\n", this));
  AUTO_PROFILER_LABEL("HttpChannelParent::Init", NETWORK);
  switch (aArgs.type()) {
    case HttpChannelCreationArgs::THttpChannelOpenArgs: {
      const HttpChannelOpenArgs& a = aArgs.get_HttpChannelOpenArgs();
      return DoAsyncOpen(
          a.uri(), a.original(), a.doc(), a.referrerInfo(), a.apiRedirectTo(),
          a.topWindowURI(), a.loadFlags(), a.requestHeaders(),
          a.requestMethod(), a.uploadStream(), a.uploadStreamHasHeaders(),
          a.priority(), a.classOfService(), a.redirectionLimit(), a.allowSTS(),
          a.thirdPartyFlags(), a.resumeAt(), a.startPos(), a.entityID(),
          a.chooseApplicationCache(), a.appCacheClientID(), a.allowSpdy(),
          a.allowAltSvc(), a.beConservative(), a.tlsFlags(), a.loadInfo(),
          a.synthesizedResponseHead(), a.synthesizedSecurityInfoSerialization(),
          a.cacheKey(), a.requestContextID(), a.preflightArgs(),
          a.initialRwin(), a.blockAuthPrompt(),
          a.suspendAfterSynthesizeResponse(), a.allowStaleCacheContent(),
          a.contentTypeHint(), a.corsMode(), a.redirectMode(), a.channelId(),
          a.integrityMetadata(), a.contentWindowId(),
          a.preferredAlternativeTypes(), a.topLevelOuterContentWindowId(),
          a.launchServiceWorkerStart(), a.launchServiceWorkerEnd(),
          a.dispatchFetchEventStart(), a.dispatchFetchEventEnd(),
          a.handleFetchEventStart(), a.handleFetchEventEnd(),
          a.forceMainDocumentChannel(), a.navigationStartTimeStamp());
    }
    case HttpChannelCreationArgs::THttpChannelConnectArgs: {
      const HttpChannelConnectArgs& cArgs = aArgs.get_HttpChannelConnectArgs();
      return ConnectChannel(cArgs.registrarId(), cArgs.shouldIntercept());
    }
    default:
      MOZ_ASSERT_UNREACHABLE("unknown open type");
      return false;
  }
}

void HttpChannelParent::TryInvokeAsyncOpen(nsresult aRv) {
  LOG(("HttpChannelParent::TryInvokeAsyncOpen [this=%p barrier=%u rv=%" PRIx32
       "]\n",
       this, mAsyncOpenBarrier, static_cast<uint32_t>(aRv)));
  MOZ_ASSERT(NS_IsMainThread());
  AUTO_PROFILER_LABEL("HttpChannelParent::TryInvokeAsyncOpen", NETWORK);

  // TryInvokeAsyncOpen is called more than we expected.
  // Assert in nightly build but ignore it in release channel.
  MOZ_DIAGNOSTIC_ASSERT(mAsyncOpenBarrier > 0);
  if (NS_WARN_IF(!mAsyncOpenBarrier)) {
    return;
  }

  if (--mAsyncOpenBarrier > 0 && NS_SUCCEEDED(aRv)) {
    // Need to wait for more events.
    return;
  }

  InvokeAsyncOpen(aRv);
}

void HttpChannelParent::OnBackgroundParentReady(
    HttpBackgroundChannelParent* aBgParent) {
  LOG(("HttpChannelParent::OnBackgroundParentReady [this=%p bgParent=%p]\n",
       this, aBgParent));
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mBgParent);

  mBgParent = aBgParent;

  mPromise.ResolveIfExists(true, __func__);
}

void HttpChannelParent::OnBackgroundParentDestroyed() {
  LOG(("HttpChannelParent::OnBackgroundParentDestroyed [this=%p]\n", this));
  MOZ_ASSERT(NS_IsMainThread());

  if (!mPromise.IsEmpty()) {
    MOZ_ASSERT(!mBgParent);
    mPromise.Reject(NS_ERROR_FAILURE, __func__);
    return;
  }

  if (!mBgParent) {
    return;
  }

  // Background channel is closed unexpectly, abort PHttpChannel operation.
  mBgParent = nullptr;
  Delete();
}

void HttpChannelParent::CleanupBackgroundChannel() {
  LOG(("HttpChannelParent::CleanupBackgroundChannel [this=%p bgParent=%p]\n",
       this, mBgParent.get()));
  MOZ_ASSERT(NS_IsMainThread());

  if (mBgParent) {
    RefPtr<HttpBackgroundChannelParent> bgParent = mBgParent.forget();
    bgParent->OnChannelClosed();
    return;
  }

  // The nsHttpChannel may have a reference to this parent, release it
  // to avoid circular references.
  RefPtr<nsHttpChannel> httpChannelImpl = do_QueryObject(mChannel);
  if (httpChannelImpl) {
    httpChannelImpl->SetWarningReporter(nullptr);
  }

  if (!mPromise.IsEmpty()) {
    mRequest.DisconnectIfExists();
    mPromise.Reject(NS_ERROR_FAILURE, __func__);

    if (!mChannel) {
      return;
    }

    // This HttpChannelParent might still have a reference from
    // BackgroundChannelRegistrar.
    nsCOMPtr<nsIBackgroundChannelRegistrar> registrar =
        BackgroundChannelRegistrar::GetOrCreate();
    MOZ_ASSERT(registrar);

    registrar->DeleteChannel(mChannel->ChannelId());

    // If mAsyncOpenBarrier is greater than zero, it means AsyncOpen procedure
    // is still on going. we need to abort AsyncOpen with failure to destroy
    // PHttpChannel actor.
    if (mAsyncOpenBarrier) {
      TryInvokeAsyncOpen(NS_ERROR_FAILURE);
    }
  }
}

base::ProcessId HttpChannelParent::OtherPid() const {
  if (mIPCClosed) {
    return 0;
  }
  return Manager()->OtherPid();
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
  NS_INTERFACE_MAP_ENTRY(nsIAsyncVerifyRedirectReadyCallback)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIParentRedirectingChannel)
  NS_INTERFACE_MAP_ENTRY_CONCRETE(HttpChannelParent)
NS_INTERFACE_MAP_END

//-----------------------------------------------------------------------------
// HttpChannelParent::nsIInterfaceRequestor
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParent::GetInterface(const nsIID& aIID, void** result) {
  if (aIID.Equals(NS_GET_IID(nsIAuthPromptProvider)) ||
      aIID.Equals(NS_GET_IID(nsISecureBrowserUI)) ||
      aIID.Equals(NS_GET_IID(nsIRemoteTab))) {
    if (mBrowserParent) {
      return mBrowserParent->QueryInterface(aIID, result);
    }
  }

  // Only support nsIAuthPromptProvider in Content process
  if (XRE_IsParentProcess() && aIID.Equals(NS_GET_IID(nsIAuthPromptProvider))) {
    *result = nullptr;
    return NS_OK;
  }

  // Only support nsILoadContext if child channel's callbacks did too
  if (aIID.Equals(NS_GET_IID(nsILoadContext)) && mLoadContext) {
    nsCOMPtr<nsILoadContext> copy = mLoadContext;
    copy.forget(result);
    return NS_OK;
  }

  if (mBrowserParent && aIID.Equals(NS_GET_IID(nsIPrompt))) {
    nsCOMPtr<Element> frameElement = mBrowserParent->GetOwnerElement();
    if (frameElement) {
      nsCOMPtr<nsPIDOMWindowOuter> win = frameElement->OwnerDoc()->GetWindow();
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

void HttpChannelParent::AsyncOpenFailed(nsresult aRv) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(NS_FAILED(aRv));

  // Break the reference cycle among HttpChannelParent,
  // HttpChannelParentListener, and nsHttpChannel to avoid memory leakage.
  mChannel = nullptr;
  mParentListener = nullptr;

  if (!mIPCClosed) {
    Unused << SendFailedAsyncOpen(aRv);
  }
}

void HttpChannelParent::InvokeAsyncOpen(nsresult rv) {
  LOG(("HttpChannelParent::InvokeAsyncOpen [this=%p rv=%" PRIx32 "]\n", this,
       static_cast<uint32_t>(rv)));
  MOZ_ASSERT(NS_IsMainThread());

  if (NS_FAILED(rv)) {
    AsyncOpenFailed(rv);
    return;
  }

  rv = mChannel->AsyncOpen(mParentListener);
  if (NS_FAILED(rv)) {
    AsyncOpenFailed(rv);
  }
}

bool HttpChannelParent::DoAsyncOpen(
    const URIParams& aURI, const Maybe<URIParams>& aOriginalURI,
    const Maybe<URIParams>& aDocURI, nsIReferrerInfo* aReferrerInfo,
    const Maybe<URIParams>& aAPIRedirectToURI,
    const Maybe<URIParams>& aTopWindowURI, const uint32_t& aLoadFlags,
    const RequestHeaderTuples& requestHeaders, const nsCString& requestMethod,
    const Maybe<IPCStream>& uploadStream, const bool& uploadStreamHasHeaders,
    const int16_t& priority, const uint32_t& classOfService,
    const uint8_t& redirectionLimit, const bool& allowSTS,
    const uint32_t& thirdPartyFlags, const bool& doResumeAt,
    const uint64_t& startPos, const nsCString& entityID,
    const bool& chooseApplicationCache, const nsCString& appCacheClientID,
    const bool& allowSpdy, const bool& allowAltSvc, const bool& beConservative,
    const uint32_t& tlsFlags, const Maybe<LoadInfoArgs>& aLoadInfoArgs,
    const Maybe<nsHttpResponseHead>& aSynthesizedResponseHead,
    const nsCString& aSecurityInfoSerialization, const uint32_t& aCacheKey,
    const uint64_t& aRequestContextID,
    const Maybe<CorsPreflightArgs>& aCorsPreflightArgs,
    const uint32_t& aInitialRwin, const bool& aBlockAuthPrompt,
    const bool& aSuspendAfterSynthesizeResponse,
    const bool& aAllowStaleCacheContent, const nsCString& aContentTypeHint,
    const uint32_t& aCorsMode, const uint32_t& aRedirectMode,
    const uint64_t& aChannelId, const nsString& aIntegrityMetadata,
    const uint64_t& aContentWindowId,
    const nsTArray<PreferredAlternativeDataTypeParams>&
        aPreferredAlternativeTypes,
    const uint64_t& aTopLevelOuterContentWindowId,
    const TimeStamp& aLaunchServiceWorkerStart,
    const TimeStamp& aLaunchServiceWorkerEnd,
    const TimeStamp& aDispatchFetchEventStart,
    const TimeStamp& aDispatchFetchEventEnd,
    const TimeStamp& aHandleFetchEventStart,
    const TimeStamp& aHandleFetchEventEnd,
    const bool& aForceMainDocumentChannel,
    const TimeStamp& aNavigationStartTimeStamp) {
  nsCOMPtr<nsIURI> uri = DeserializeURI(aURI);
  if (!uri) {
    // URIParams does MOZ_ASSERT if null, but we need to protect opt builds from
    // null deref here.
    return false;
  }
  nsCOMPtr<nsIURI> originalUri = DeserializeURI(aOriginalURI);
  nsCOMPtr<nsIURI> docUri = DeserializeURI(aDocURI);
  nsCOMPtr<nsIURI> apiRedirectToUri = DeserializeURI(aAPIRedirectToURI);
  nsCOMPtr<nsIURI> topWindowUri = DeserializeURI(aTopWindowURI);

  LOG(("HttpChannelParent RecvAsyncOpen [this=%p uri=%s, gid=%" PRIu64
       " topwinid=%" PRIx64 "]\n",
       this, uri->GetSpecOrDefault().get(), aChannelId,
       aTopLevelOuterContentWindowId));

  nsresult rv;

  nsCOMPtr<nsIIOService> ios(do_GetIOService(&rv));
  if (NS_FAILED(rv)) return SendFailedAsyncOpen(rv);

  nsCOMPtr<nsILoadInfo> loadInfo;
  rv = mozilla::ipc::LoadInfoArgsToLoadInfo(aLoadInfoArgs,
                                            getter_AddRefs(loadInfo));
  if (NS_FAILED(rv)) {
    return SendFailedAsyncOpen(rv);
  }

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannelInternal(getter_AddRefs(channel), uri, loadInfo, nullptr,
                             nullptr, nullptr, aLoadFlags, ios);
  if (NS_FAILED(rv)) {
    return SendFailedAsyncOpen(rv);
  }

  RefPtr<HttpBaseChannel> httpChannel = do_QueryObject(channel, &rv);
  if (NS_FAILED(rv)) {
    return SendFailedAsyncOpen(rv);
  }

  // Set attributes needed to create a FetchEvent from this channel.
  httpChannel->SetCorsMode(aCorsMode);
  httpChannel->SetRedirectMode(aRedirectMode);

  // Set the channelId allocated in child to the parent instance
  httpChannel->SetChannelId(aChannelId);
  httpChannel->SetTopLevelContentWindowId(aContentWindowId);
  httpChannel->SetTopLevelOuterContentWindowId(aTopLevelOuterContentWindowId);

  httpChannel->SetIntegrityMetadata(aIntegrityMetadata);

  RefPtr<nsHttpChannel> httpChannelImpl = do_QueryObject(httpChannel);
  if (httpChannelImpl) {
    httpChannelImpl->SetWarningReporter(this);
  }
  httpChannel->SetTimingEnabled(true);
  if (mPBOverride != kPBOverride_Unset) {
    httpChannel->SetPrivate(mPBOverride == kPBOverride_Private ? true : false);
  }

  if (doResumeAt) httpChannel->ResumeAt(startPos, entityID);

  if (originalUri) httpChannel->SetOriginalURI(originalUri);
  if (docUri) httpChannel->SetDocumentURI(docUri);
  if (aReferrerInfo) {
    // Referrer header is computed in child no need to recompute here
    rv = httpChannel->SetReferrerInfo(aReferrerInfo, false, false);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  if (apiRedirectToUri) httpChannel->RedirectTo(apiRedirectToUri);
  if (topWindowUri) {
    rv = httpChannel->SetTopWindowURI(topWindowUri);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  if (aLoadFlags != nsIRequest::LOAD_NORMAL)
    httpChannel->SetLoadFlags(aLoadFlags);

  if (aForceMainDocumentChannel) {
    httpChannel->SetIsMainDocumentChannel(true);
  }

  for (uint32_t i = 0; i < requestHeaders.Length(); i++) {
    if (requestHeaders[i].mEmpty) {
      httpChannel->SetEmptyRequestHeader(requestHeaders[i].mHeader);
    } else {
      httpChannel->SetRequestHeader(requestHeaders[i].mHeader,
                                    requestHeaders[i].mValue,
                                    requestHeaders[i].mMerge);
    }
  }

  RefPtr<HttpChannelParentListener> parentListener =
      new HttpChannelParentListener(this);

  httpChannel->SetRequestMethod(nsDependentCString(requestMethod.get()));

  if (aCorsPreflightArgs.isSome()) {
    const CorsPreflightArgs& args = aCorsPreflightArgs.ref();
    httpChannel->SetCorsPreflightParameters(args.unsafeHeaders());
  }

  nsCOMPtr<nsIInputStream> stream = DeserializeIPCStream(uploadStream);
  if (stream) {
    int64_t length;
    if (InputStreamLengthHelper::GetSyncLength(stream, &length)) {
      httpChannel->InternalSetUploadStreamLength(length >= 0 ? length : 0);
    } else {
      // Wait for the nputStreamLengthHelper::GetAsyncLength callback.
      ++mAsyncOpenBarrier;

      // Let's resolve the size of the stream. The following operation is always
      // async.
      RefPtr<HttpChannelParent> self = this;
      InputStreamLengthHelper::GetAsyncLength(stream, [self, httpChannel](
                                                          int64_t aLength) {
        httpChannel->InternalSetUploadStreamLength(aLength >= 0 ? aLength : 0);
        self->TryInvokeAsyncOpen(NS_OK);
      });
    }

    httpChannel->InternalSetUploadStream(stream);
    httpChannel->SetUploadStreamHasHeaders(uploadStreamHasHeaders);
  }

  if (aSynthesizedResponseHead.isSome()) {
    parentListener->SetupInterception(aSynthesizedResponseHead.ref());
    mWillSynthesizeResponse = true;
    httpChannelImpl->SetCouldBeSynthesized();

    if (!aSecurityInfoSerialization.IsEmpty()) {
      nsCOMPtr<nsISupports> secInfo;
      rv = NS_DeserializeObject(aSecurityInfoSerialization,
                                getter_AddRefs(secInfo));
      MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv),
                            "Deserializing security info should not fail");
      rv = httpChannel->OverrideSecurityInfo(secInfo);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  }

  nsCOMPtr<nsICacheInfoChannel> cacheChannel =
      do_QueryInterface(static_cast<nsIChannel*>(httpChannel.get()));
  if (cacheChannel) {
    cacheChannel->SetCacheKey(aCacheKey);
    for (auto& data : aPreferredAlternativeTypes) {
      cacheChannel->PreferAlternativeDataType(data.type(), data.contentType(),
                                              data.deliverAltData());
    }

    cacheChannel->SetAllowStaleCacheContent(aAllowStaleCacheContent);

    // This is to mark that the results are going to the content process.
    if (httpChannelImpl) {
      httpChannelImpl->SetAltDataForChild(true);
    }
  }

  httpChannel->SetContentType(aContentTypeHint);

  if (priority != nsISupportsPriority::PRIORITY_NORMAL) {
    httpChannel->SetPriority(priority);
  }
  if (classOfService) {
    httpChannel->SetClassFlags(classOfService);
  }
  httpChannel->SetRedirectionLimit(redirectionLimit);
  httpChannel->SetAllowSTS(allowSTS);
  httpChannel->SetThirdPartyFlags(thirdPartyFlags);
  httpChannel->SetAllowSpdy(allowSpdy);
  httpChannel->SetAllowAltSvc(allowAltSvc);
  httpChannel->SetBeConservative(beConservative);
  httpChannel->SetTlsFlags(tlsFlags);
  httpChannel->SetInitialRwin(aInitialRwin);
  httpChannel->SetBlockAuthPrompt(aBlockAuthPrompt);

  httpChannel->SetLaunchServiceWorkerStart(aLaunchServiceWorkerStart);
  httpChannel->SetLaunchServiceWorkerEnd(aLaunchServiceWorkerEnd);
  httpChannel->SetDispatchFetchEventStart(aDispatchFetchEventStart);
  httpChannel->SetDispatchFetchEventEnd(aDispatchFetchEventEnd);
  httpChannel->SetHandleFetchEventStart(aHandleFetchEventStart);
  httpChannel->SetHandleFetchEventEnd(aHandleFetchEventEnd);

  httpChannel->SetNavigationStartTimeStamp(aNavigationStartTimeStamp);

  nsCOMPtr<nsIApplicationCacheChannel> appCacheChan =
      do_QueryObject(httpChannel);
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
      NS_GetOriginAttributes(httpChannel, attrs);

      nsCOMPtr<nsIPrincipal> principal =
          BasePrincipal::CreateCodebasePrincipal(uri, attrs);

      bool chooseAppCache = false;
      // This works because we've already called SetNotificationCallbacks and
      // done mPBOverride logic by this point.
      chooseAppCache = NS_ShouldCheckAppCache(principal);

      appCacheChan->SetChooseApplicationCache(chooseAppCache);
    }
  }

  httpChannel->SetRequestContextID(aRequestContextID);

  // Store the strong reference of channel and parent listener object until
  // all the initialization procedure is complete without failure, to remove
  // cycle reference in fail case and to avoid memory leakage.
  mChannel = httpChannel.forget();
  mParentListener = parentListener.forget();
  mChannel->SetNotificationCallbacks(mParentListener);

  mSuspendAfterSynthesizeResponse = aSuspendAfterSynthesizeResponse;

  MOZ_ASSERT(!mBgParent);
  MOZ_ASSERT(mPromise.IsEmpty());
  // Wait for HttpBackgrounChannel to continue the async open procedure.
  ++mAsyncOpenBarrier;
  RefPtr<HttpChannelParent> self = this;
  WaitForBgParent()
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [self]() {
            self->mRequest.Complete();
            self->TryInvokeAsyncOpen(NS_OK);
          },
          [self](nsresult aStatus) {
            self->mRequest.Complete();
            self->TryInvokeAsyncOpen(aStatus);
          })
      ->Track(mRequest);

  // The stream, received from the child process, must be cloneable and seekable
  // in order to allow devtools to inspect its content.
  nsCOMPtr<nsIRunnable> r =
      NS_NewRunnableFunction("HttpChannelParent::EnsureUploadStreamIsCloneable",
                             [self]() { self->TryInvokeAsyncOpen(NS_OK); });
  ++mAsyncOpenBarrier;
  mChannel->EnsureUploadStreamIsCloneable(r);
  return true;
}

RefPtr<GenericNonExclusivePromise> HttpChannelParent::WaitForBgParent() {
  LOG(("HttpChannelParent::WaitForBgParent [this=%p]\n", this));
  MOZ_ASSERT(!mBgParent);
  MOZ_ASSERT(mChannel);

  nsCOMPtr<nsIBackgroundChannelRegistrar> registrar =
      BackgroundChannelRegistrar::GetOrCreate();
  MOZ_ASSERT(registrar);
  registrar->LinkHttpChannel(mChannel->ChannelId(), this);

  if (mBgParent) {
    return GenericNonExclusivePromise::CreateAndResolve(true, __func__);
  }

  return mPromise.Ensure(__func__);
}

bool HttpChannelParent::ConnectChannel(const uint32_t& registrarId,
                                       const bool& shouldIntercept) {
  nsresult rv;

  LOG(
      ("HttpChannelParent::ConnectChannel: Looking for a registered channel "
       "[this=%p, id=%" PRIu32 "]\n",
       this, registrarId));
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

  LOG(("  found channel %p, rv=%08" PRIx32, channel.get(),
       static_cast<uint32_t>(rv)));
  mChannel = do_QueryObject(channel);
  if (!mChannel) {
    LOG(("  but it's not HttpBaseChannel"));
    Delete();
    return true;
  }

  LOG(("  and it is HttpBaseChannel %p", mChannel.get()));

  RefPtr<nsHttpChannel> httpChannelImpl = do_QueryObject(mChannel);
  if (httpChannelImpl) {
    httpChannelImpl->SetWarningReporter(this);
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

  MOZ_ASSERT(!mBgParent);
  MOZ_ASSERT(mPromise.IsEmpty());
  // Waiting for background channel
  RefPtr<HttpChannelParent> self = this;
  WaitForBgParent()
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [self]() { self->mRequest.Complete(); },
          [self](const nsresult& aResult) {
            NS_ERROR("failed to establish the background channel");
            self->mRequest.Complete();
          })
      ->Track(mRequest);
  return true;
}

mozilla::ipc::IPCResult HttpChannelParent::RecvSetPriority(
    const int16_t& priority) {
  LOG(("HttpChannelParent::RecvSetPriority [this=%p, priority=%d]\n", this,
       priority));
  AUTO_PROFILER_LABEL("HttpChannelParent::RecvSetPriority", NETWORK);

  if (mChannel) {
    mChannel->SetPriority(priority);
  }

  nsCOMPtr<nsISupportsPriority> priorityRedirectChannel =
      do_QueryInterface(mRedirectChannel);
  if (priorityRedirectChannel) priorityRedirectChannel->SetPriority(priority);

  return IPC_OK();
}

mozilla::ipc::IPCResult HttpChannelParent::RecvSetClassOfService(
    const uint32_t& cos) {
  if (mChannel) {
    mChannel->SetClassFlags(cos);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult HttpChannelParent::RecvSuspend() {
  LOG(("HttpChannelParent::RecvSuspend [this=%p]\n", this));

  if (mChannel) {
    mChannel->Suspend();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult HttpChannelParent::RecvResume() {
  LOG(("HttpChannelParent::RecvResume [this=%p]\n", this));

  if (mChannel) {
    mChannel->Resume();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult HttpChannelParent::RecvCancel(const nsresult& status) {
  LOG(("HttpChannelParent::RecvCancel [this=%p]\n", this));

  // Don't cancel our channel if we're doing a CrossProcessRedirect.
  if (mDoingCrossProcessRedirect) {
    LOG(("Child was cancelled for cross-process redirect. Skip Cancel()."));
    return IPC_OK();
  }

  // May receive cancel before channel has been constructed!
  if (mChannel) {
    mChannel->Cancel(status);

    // Once we receive |Cancel|, child will stop sending RecvBytesRead. Force
    // the channel resumed if needed.
    if (mSuspendedForFlowControl) {
      LOG(("  resume the channel due to e10s backpressure relief by cancel"));
      Unused << mChannel->Resume();
      mSuspendedForFlowControl = false;
    }
  }

  // We won't need flow control anymore. Toggle the flag to avoid |Suspend|
  // since OnDataAvailable could be off-main-thread.
  mCacheNeedFlowControlInitialized = true;
  mNeedFlowControl = false;
  return IPC_OK();
}

mozilla::ipc::IPCResult HttpChannelParent::RecvSetCacheTokenCachedCharset(
    const nsCString& charset) {
  if (mCacheEntry) mCacheEntry->SetMetaDataElement("charset", charset.get());
  return IPC_OK();
}

mozilla::ipc::IPCResult HttpChannelParent::RecvRedirect2Verify(
    const nsresult& aResult, const RequestHeaderTuples& changedHeaders,
    const ChildLoadInfoForwarderArgs& aLoadInfoForwarder,
    const uint32_t& loadFlags, nsIReferrerInfo* aReferrerInfo,
    const Maybe<URIParams>& aAPIRedirectURI,
    const Maybe<CorsPreflightArgs>& aCorsPreflightArgs,
    const bool& aChooseAppcache) {
  LOG(("HttpChannelParent::RecvRedirect2Verify [this=%p result=%" PRIx32 "]\n",
       this, static_cast<uint32_t>(aResult)));

  // Result from the child.  If something fails here, we might overwrite a
  // success with a further failure.
  nsresult result = aResult;

  // Local results.
  nsresult rv;

  if (NS_SUCCEEDED(result)) {
    nsCOMPtr<nsIHttpChannel> newHttpChannel =
        do_QueryInterface(mRedirectChannel);

    if (newHttpChannel) {
      nsCOMPtr<nsIURI> apiRedirectUri = DeserializeURI(aAPIRedirectURI);

      if (apiRedirectUri) {
        rv = newHttpChannel->RedirectTo(apiRedirectUri);
        MOZ_ASSERT(NS_SUCCEEDED(rv));
      }

      for (uint32_t i = 0; i < changedHeaders.Length(); i++) {
        if (changedHeaders[i].mEmpty) {
          rv = newHttpChannel->SetEmptyRequestHeader(changedHeaders[i].mHeader);
        } else {
          rv = newHttpChannel->SetRequestHeader(changedHeaders[i].mHeader,
                                                changedHeaders[i].mValue,
                                                changedHeaders[i].mMerge);
        }
        MOZ_ASSERT(NS_SUCCEEDED(rv));
      }

      // A successfully redirected channel must have the LOAD_REPLACE flag.
      MOZ_ASSERT(loadFlags & nsIChannel::LOAD_REPLACE);
      if (loadFlags & nsIChannel::LOAD_REPLACE) {
        newHttpChannel->SetLoadFlags(loadFlags);
      }

      if (aCorsPreflightArgs.isSome()) {
        nsCOMPtr<nsIHttpChannelInternal> newInternalChannel =
            do_QueryInterface(newHttpChannel);
        MOZ_RELEASE_ASSERT(newInternalChannel);
        const CorsPreflightArgs& args = aCorsPreflightArgs.ref();
        newInternalChannel->SetCorsPreflightParameters(args.unsafeHeaders());
      }

      if (aReferrerInfo) {
        RefPtr<HttpBaseChannel> baseChannel = do_QueryObject(newHttpChannel);
        MOZ_ASSERT(baseChannel);
        if (baseChannel) {
          // Referrer header is computed in child no need to recompute here
          rv = baseChannel->SetReferrerInfo(aReferrerInfo, false, false);
          MOZ_ASSERT(NS_SUCCEEDED(rv));
        }
      }

      nsCOMPtr<nsIApplicationCacheChannel> appCacheChannel =
          do_QueryInterface(newHttpChannel);
      if (appCacheChannel) {
        appCacheChannel->SetChooseApplicationCache(aChooseAppcache);
      }

      nsCOMPtr<nsILoadInfo> newLoadInfo = newHttpChannel->LoadInfo();
      rv = MergeChildLoadInfoForwarder(aLoadInfoForwarder, newLoadInfo);
      if (NS_FAILED(rv) && NS_SUCCEEDED(result)) {
        result = rv;
      }
    }
  }

  // Continue the verification procedure if child has veto the redirection.
  if (NS_FAILED(result)) {
    ContinueRedirect2Verify(result);
    return IPC_OK();
  }

  // Wait for background channel ready on target channel
  nsCOMPtr<nsIRedirectChannelRegistrar> redirectReg =
      RedirectChannelRegistrar::GetOrCreate();
  MOZ_ASSERT(redirectReg);

  nsCOMPtr<nsIParentChannel> redirectParentChannel;
  rv = redirectReg->GetParentChannel(mRedirectRegistrarId,
                                     getter_AddRefs(redirectParentChannel));
  MOZ_ASSERT(redirectParentChannel);
  if (!redirectParentChannel) {
    ContinueRedirect2Verify(rv);
    return IPC_OK();
  }

  nsCOMPtr<nsIParentRedirectingChannel> redirectedParent =
      do_QueryInterface(redirectParentChannel);
  if (!redirectedParent) {
    // Continue verification procedure if redirecting to non-Http protocol
    ContinueRedirect2Verify(result);
    return IPC_OK();
  }

  // Ask redirected channel if verification can proceed.
  // ContinueRedirect2Verify will be invoked when redirected channel is ready.
  redirectedParent->ContinueVerification(this);

  return IPC_OK();
}

// from nsIParentRedirectingChannel
NS_IMETHODIMP
HttpChannelParent::ContinueVerification(
    nsIAsyncVerifyRedirectReadyCallback* aCallback) {
  LOG(("HttpChannelParent::ContinueVerification [this=%p callback=%p]\n", this,
       aCallback));

  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aCallback);

  // Continue the verification procedure if background channel is ready.
  if (mBgParent) {
    aCallback->ReadyToVerify(NS_OK);
    return NS_OK;
  }

  // ConnectChannel must be received before Redirect2Verify.
  MOZ_ASSERT(!mPromise.IsEmpty());

  // Otherwise, wait for the background channel.
  nsCOMPtr<nsIAsyncVerifyRedirectReadyCallback> callback = aCallback;
  WaitForBgParent()->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [callback]() { callback->ReadyToVerify(NS_OK); },
      [callback](const nsresult& aResult) {
        NS_ERROR("failed to establish the background channel");
        callback->ReadyToVerify(aResult);
      });
  return NS_OK;
}

void HttpChannelParent::ContinueRedirect2Verify(const nsresult& aResult) {
  LOG(("HttpChannelParent::ContinueRedirect2Verify [this=%p result=%" PRIx32
       "]\n",
       this, static_cast<uint32_t>(aResult)));

  if (!mRedirectCallback) {
    // This should according the logic never happen, log the situation.
    if (mReceivedRedirect2Verify)
      LOG(("RecvRedirect2Verify[%p]: Duplicate fire", this));
    if (mSentRedirect1BeginFailed)
      LOG(("RecvRedirect2Verify[%p]: Send to child failed", this));
    if ((mRedirectRegistrarId > 0) && NS_FAILED(aResult))
      LOG(("RecvRedirect2Verify[%p]: Redirect failed", this));
    if ((mRedirectRegistrarId > 0) && NS_SUCCEEDED(aResult))
      LOG(("RecvRedirect2Verify[%p]: Redirect succeeded", this));
    if (!mRedirectChannel)
      LOG(("RecvRedirect2Verify[%p]: Missing redirect channel", this));

    NS_ERROR(
        "Unexpcted call to HttpChannelParent::RecvRedirect2Verify, "
        "mRedirectCallback null");
  }

  mReceivedRedirect2Verify = true;

  if (mRedirectCallback) {
    LOG(
        ("HttpChannelParent::ContinueRedirect2Verify call "
         "OnRedirectVerifyCallback"
         " [this=%p result=%" PRIx32 ", mRedirectCallback=%p]\n",
         this, static_cast<uint32_t>(aResult), mRedirectCallback.get()));
    mRedirectCallback->OnRedirectVerifyCallback(aResult);
    mRedirectCallback = nullptr;
  }
}

mozilla::ipc::IPCResult HttpChannelParent::RecvDocumentChannelCleanup(
    const bool& clearCacheEntry) {
  CleanupBackgroundChannel();  // Background channel can be closed.
  mChannel = nullptr;          // Reclaim some memory sooner.
  if (clearCacheEntry) {
    mCacheEntry = nullptr;  // Else we'll block other channels reading same URI
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
HttpChannelParent::RecvMarkOfflineCacheEntryAsForeign() {
  if (mOfflineForeignMarker) {
    mOfflineForeignMarker->MarkAsForeign();
    mOfflineForeignMarker = nullptr;
  }

  return IPC_OK();
}

class DivertDataAvailableEvent : public MainThreadChannelEvent {
 public:
  DivertDataAvailableEvent(HttpChannelParent* aParent, const nsCString& data,
                           const uint64_t& offset, const uint32_t& count)
      : mParent(aParent), mData(data), mOffset(offset), mCount(count) {}

  void Run() override {
    mParent->DivertOnDataAvailable(mData, mOffset, mCount);
  }

 private:
  HttpChannelParent* mParent;
  nsCString mData;
  uint64_t mOffset;
  uint32_t mCount;
};

mozilla::ipc::IPCResult HttpChannelParent::RecvDivertOnDataAvailable(
    const nsCString& data, const uint64_t& offset, const uint32_t& count) {
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

  mEventQ->RunOrEnqueue(
      new DivertDataAvailableEvent(this, data, offset, count));
  return IPC_OK();
}

void HttpChannelParent::DivertOnDataAvailable(const nsCString& data,
                                              const uint64_t& offset,
                                              const uint32_t& count) {
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
  nsresult rv =
      NS_NewByteInputStream(getter_AddRefs(stringStream),
                            MakeSpan(data).To(count), NS_ASSIGNMENT_DEPEND);
  if (NS_FAILED(rv)) {
    if (mChannel) {
      mChannel->Cancel(rv);
    }
    mStatus = rv;
    return;
  }

  AutoEventEnqueuer ensureSerialDispatch(mEventQ);

  rv = mParentListener->OnDataAvailable(mChannel, stringStream, offset, count);
  stringStream->Close();
  if (NS_FAILED(rv)) {
    if (mChannel) {
      mChannel->Cancel(rv);
    }
    mStatus = rv;
  }
}

class DivertStopRequestEvent : public MainThreadChannelEvent {
 public:
  DivertStopRequestEvent(HttpChannelParent* aParent, const nsresult& statusCode)
      : mParent(aParent), mStatusCode(statusCode) {}

  void Run() override { mParent->DivertOnStopRequest(mStatusCode); }

 private:
  HttpChannelParent* mParent;
  nsresult mStatusCode;
};

mozilla::ipc::IPCResult HttpChannelParent::RecvDivertOnStopRequest(
    const nsresult& statusCode) {
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

void HttpChannelParent::DivertOnStopRequest(const nsresult& statusCode) {
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
  mParentListener->OnStopRequest(mChannel, status);
}

class DivertCompleteEvent : public MainThreadChannelEvent {
 public:
  explicit DivertCompleteEvent(HttpChannelParent* aParent) : mParent(aParent) {}

  void Run() override { mParent->DivertComplete(); }

 private:
  HttpChannelParent* mParent;
};

mozilla::ipc::IPCResult HttpChannelParent::RecvDivertComplete() {
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

void HttpChannelParent::DivertComplete() {
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

void HttpChannelParent::MaybeFlushPendingDiversion() {
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
}

static void FinishCrossProcessRedirect(nsHttpChannel* channel,
                                       nsresult status) {
  if (NS_SUCCEEDED(status)) {
    nsCOMPtr<nsINetworkInterceptController> controller;
    NS_QueryNotificationCallbacks(channel, controller);
    RefPtr<HttpChannelParentListener> parentListener =
        do_QueryObject(controller);
    MOZ_ASSERT(parentListener);

    // This updates HttpChannelParentListener to point to this parent and at
    // the same time cancels the old channel.
    parentListener->OnRedirectResult(status == NS_OK);
  }

  channel->OnRedirectVerifyCallback(status);
}

mozilla::ipc::IPCResult HttpChannelParent::RecvCrossProcessRedirectDone(
    const nsresult& aResult) {
  RefPtr<nsHttpChannel> chan = do_QueryObject(mChannel);
  if (!mBgParent) {
    RefPtr<HttpChannelParent> self = this;
    WaitForBgParent()->Then(
        GetMainThreadSerialEventTarget(), __func__,
        [self, chan, aResult]() { FinishCrossProcessRedirect(chan, aResult); },
        [self, chan](const nsresult& aRejectionRv) {
          MOZ_ASSERT(NS_FAILED(aRejectionRv), "This should be an error code");
          FinishCrossProcessRedirect(chan, aRejectionRv);
        });
  } else {
    FinishCrossProcessRedirect(chan, aResult);
  }

  return IPC_OK();
}

void HttpChannelParent::ResponseSynthesized() {
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

mozilla::ipc::IPCResult HttpChannelParent::RecvRemoveCorsPreflightCacheEntry(
    const URIParams& uri,
    const mozilla::ipc::PrincipalInfo& requestingPrincipal) {
  nsCOMPtr<nsIURI> deserializedURI = DeserializeURI(uri);
  if (!deserializedURI) {
    return IPC_FAIL_NO_REASON(this);
  }
  nsCOMPtr<nsIPrincipal> principal =
      PrincipalInfoToPrincipal(requestingPrincipal);
  if (!principal) {
    return IPC_FAIL_NO_REASON(this);
  }
  nsCORSListenerProxy::RemoveFromCorsPreflightCache(deserializedURI, principal);
  return IPC_OK();
}

//-----------------------------------------------------------------------------
// HttpChannelParent::nsIRequestObserver
//-----------------------------------------------------------------------------

static void GetTimingAttributes(HttpBaseChannel* aChannel,
                                ResourceTimingStruct& aTiming) {
  aChannel->GetDomainLookupStart(&aTiming.domainLookupStart);
  aChannel->GetDomainLookupEnd(&aTiming.domainLookupEnd);
  aChannel->GetConnectStart(&aTiming.connectStart);
  aChannel->GetTcpConnectEnd(&aTiming.tcpConnectEnd);
  aChannel->GetSecureConnectionStart(&aTiming.secureConnectionStart);
  aChannel->GetConnectEnd(&aTiming.connectEnd);
  aChannel->GetRequestStart(&aTiming.requestStart);
  aChannel->GetResponseStart(&aTiming.responseStart);
  aChannel->GetResponseEnd(&aTiming.responseEnd);
  aChannel->GetAsyncOpen(&aTiming.fetchStart);
  aChannel->GetRedirectStart(&aTiming.redirectStart);
  aChannel->GetRedirectEnd(&aTiming.redirectEnd);
  aChannel->GetTransferSize(&aTiming.transferSize);
  aChannel->GetEncodedBodySize(&aTiming.encodedBodySize);
  // decodedBodySize can be computed in the child process so it doesn't need
  // to be passed down.
  aChannel->GetProtocolVersion(aTiming.protocolVersion);

  aChannel->GetCacheReadStart(&aTiming.cacheReadStart);
  aChannel->GetCacheReadEnd(&aTiming.cacheReadEnd);
}

NS_IMETHODIMP
HttpChannelParent::OnStartRequest(nsIRequest* aRequest) {
  nsresult rv;

  LOG(("HttpChannelParent::OnStartRequest [this=%p, aRequest=%p]\n", this,
       aRequest));
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(!mDivertingFromChild,
                     "Cannot call OnStartRequest if diverting is set!");

  if (mDoingCrossProcessRedirect) {
    LOG(("Child was cancelled for cross-process redirect. Bail."));
    return NS_OK;
  }

  RefPtr<HttpBaseChannel> chan = do_QueryObject(aRequest);
  if (!chan) {
    LOG(("  aRequest is not HttpBaseChannel"));
    NS_ERROR(
        "Expecting only HttpBaseChannel as aRequest in "
        "HttpChannelParent::OnStartRequest");
    return NS_ERROR_UNEXPECTED;
  }

  MOZ_ASSERT(mChannel == chan,
             "HttpChannelParent getting OnStartRequest from a different "
             "HttpBaseChannel instance");

  // Send down any permissions which are relevant to this URL if we are
  // performing a document load. We can't do that if mIPCClosed is set.
  if (!mIPCClosed) {
    PContentParent* pcp = Manager()->Manager();
    MOZ_ASSERT(pcp, "We should have a manager if our IPC isn't closed");
    DebugOnly<nsresult> rv =
        static_cast<ContentParent*>(pcp)->AboutToLoadHttpFtpDocumentForChild(
            chan);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  bool isFromCache = false;
  bool isRacing = false;
  uint64_t cacheEntryId = 0;
  int32_t fetchCount = 0;
  uint32_t expirationTime = nsICacheEntry::NO_EXPIRATION_TIME;
  nsCString cachedCharset;

  RefPtr<nsHttpChannel> httpChannelImpl = do_QueryObject(chan);

  if (httpChannelImpl) {
    httpChannelImpl->IsFromCache(&isFromCache);
    httpChannelImpl->IsRacing(&isRacing);
    httpChannelImpl->GetCacheEntryId(&cacheEntryId);
    httpChannelImpl->GetCacheTokenFetchCount(&fetchCount);
    httpChannelImpl->GetCacheTokenExpirationTime(&expirationTime);
    httpChannelImpl->GetCacheTokenCachedCharset(cachedCharset);
  }

  bool loadedFromApplicationCache = false;

  if (httpChannelImpl) {
    httpChannelImpl->GetLoadedFromApplicationCache(&loadedFromApplicationCache);
    if (loadedFromApplicationCache) {
      mOfflineForeignMarker =
          httpChannelImpl->GetOfflineCacheEntryAsForeignMarker();
      nsCOMPtr<nsIApplicationCache> appCache;
      httpChannelImpl->GetApplicationCache(getter_AddRefs(appCache));
      nsCString appCacheGroupId;
      nsCString appCacheClientId;
      appCache->GetGroupID(appCacheGroupId);
      appCache->GetClientID(appCacheClientId);
      if (mIPCClosed ||
          !SendAssociateApplicationCache(appCacheGroupId, appCacheClientId)) {
        return NS_ERROR_UNEXPECTED;
      }
    }
  }

  // Propagate whether or not conversion should occur from the parent-side
  // channel to the child-side channel.  Then disable the parent-side
  // conversion so that it only occurs in the child.
  bool applyConversion = true;
  Unused << chan->GetApplyConversion(&applyConversion);
  chan->SetApplyConversion(false);

  nsresult channelStatus = NS_OK;
  chan->GetStatus(&channelStatus);

  // Keep the cache entry for future use in RecvSetCacheTokenCachedCharset().
  // It could be already released by nsHttpChannel at that time.
  nsCOMPtr<nsISupports> cacheEntry;
  uint32_t cacheKey = 0;
  nsAutoCString altDataType;

  if (httpChannelImpl) {
    httpChannelImpl->GetCacheToken(getter_AddRefs(cacheEntry));
    mCacheEntry = do_QueryInterface(cacheEntry);

    httpChannelImpl->GetCacheKey(&cacheKey);

    httpChannelImpl->GetAlternativeDataType(altDataType);
  }

  nsCString secInfoSerialization;
  UpdateAndSerializeSecurityInfo(secInfoSerialization);

  uint8_t redirectCount = 0;
  chan->GetRedirectCount(&redirectCount);

  int64_t altDataLen = chan->GetAltDataLength();
  bool deliveringAltData = chan->IsDeliveringAltData();

  nsCOMPtr<nsILoadInfo> loadInfo = chan->LoadInfo();

  ParentLoadInfoForwarderArgs loadInfoForwarderArg;
  mozilla::ipc::LoadInfoToParentLoadInfoForwarder(loadInfo,
                                                  &loadInfoForwarderArg);

  nsHttpResponseHead* responseHead = chan->GetResponseHead();
  bool useResponseHead = !!responseHead;
  nsHttpResponseHead cleanedUpResponseHead;
  if (responseHead && responseHead->HasHeader(nsHttp::Set_Cookie)) {
    cleanedUpResponseHead = *responseHead;
    cleanedUpResponseHead.ClearHeader(nsHttp::Set_Cookie);
    responseHead = &cleanedUpResponseHead;
  }

  if (!responseHead) {
    responseHead = &cleanedUpResponseHead;
  }

  nsHttpRequestHead* requestHead = chan->GetRequestHead();
  // !!! We need to lock headers and please don't forget to unlock them !!!
  requestHead->Enter();

  nsHttpHeaderArray cleanedUpRequestHeaders;
  bool cleanedUpRequest = false;
  if (requestHead->HasHeader(nsHttp::Cookie)) {
    cleanedUpRequestHeaders = requestHead->Headers();
    cleanedUpRequestHeaders.ClearHeader(nsHttp::Cookie);
    cleanedUpRequest = true;
  }

  ResourceTimingStruct timing;
  GetTimingAttributes(mChannel, timing);

  bool isResolvedByTRR = false;
  chan->GetIsResolvedByTRR(&isResolvedByTRR);

  rv = NS_OK;
  if (mIPCClosed ||
      !SendOnStartRequest(
          channelStatus, *responseHead, useResponseHead,
          cleanedUpRequest ? cleanedUpRequestHeaders : requestHead->Headers(),
          loadInfoForwarderArg, isFromCache, isRacing,
          mCacheEntry ? true : false, cacheEntryId, fetchCount, expirationTime,
          cachedCharset, secInfoSerialization, chan->GetSelfAddr(),
          chan->GetPeerAddr(), redirectCount, cacheKey, altDataType, altDataLen,
          deliveringAltData, applyConversion, isResolvedByTRR, timing)) {
    rv = NS_ERROR_UNEXPECTED;
  }
  requestHead->Exit();

  // OnStartRequest is sent to content process successfully.
  // Notify PHttpBackgroundChannelChild that all following IPC mesasges
  // should be run after OnStartRequest is handled.
  if (NS_SUCCEEDED(rv)) {
    MOZ_ASSERT(mBgParent);
    if (!mBgParent->OnStartRequestSent()) {
      rv = NS_ERROR_UNEXPECTED;
    }
  }

  return rv;
}

NS_IMETHODIMP
HttpChannelParent::OnStopRequest(nsIRequest* aRequest, nsresult aStatusCode) {
  LOG(("HttpChannelParent::OnStopRequest: [this=%p aRequest=%p status=%" PRIx32
       "]\n",
       this, aRequest, static_cast<uint32_t>(aStatusCode)));
  MOZ_ASSERT(NS_IsMainThread());

  if (mDoingCrossProcessRedirect) {
    LOG(("Child was cancelled for cross-process redirect. Bail."));
    return NS_OK;
  }

  MOZ_RELEASE_ASSERT(!mDivertingFromChild,
                     "Cannot call OnStopRequest if diverting is set!");
  ResourceTimingStruct timing;
  GetTimingAttributes(mChannel, timing);

  RefPtr<nsHttpChannel> httpChannelImpl = do_QueryObject(mChannel);
  if (httpChannelImpl) {
    httpChannelImpl->SetWarningReporter(nullptr);
  }

  nsHttpHeaderArray* responseTrailer = mChannel->GetResponseTrailers();

  // Either IPC channel is closed or background channel
  // is ready to send OnStopRequest.
  MOZ_ASSERT(mIPCClosed || mBgParent);

  if (mIPCClosed || !mBgParent ||
      !mBgParent->OnStopRequest(
          aStatusCode, timing,
          responseTrailer ? *responseTrailer : nsHttpHeaderArray())) {
    return NS_ERROR_UNEXPECTED;
  }

  if (NeedFlowControl()) {
    bool isLocal = false;
    NetAddr peerAddr = mChannel->GetPeerAddr();

#if defined(XP_UNIX)
    // Unix-domain sockets are always local.
    isLocal = (peerAddr.raw.family == PR_AF_LOCAL);
#endif

    isLocal = isLocal || IsLoopBackAddress(&peerAddr);

    if (!isLocal) {
      if (!mHasSuspendedByBackPressure) {
        AccumulateCategorical(
            Telemetry::LABELS_NETWORK_BACK_PRESSURE_SUSPENSION_RATE_V2::
                NotSuspended);
      } else {
        AccumulateCategorical(
            Telemetry::LABELS_NETWORK_BACK_PRESSURE_SUSPENSION_RATE_V2::
                Suspended);

        // Only analyze non-local suspended cases, which we are interested in.
        nsCOMPtr<nsILoadInfo> loadInfo = mChannel->LoadInfo();
        Telemetry::Accumulate(
            Telemetry::NETWORK_BACK_PRESSURE_SUSPENSION_CP_TYPE,
            loadInfo->InternalContentPolicyType());
      }
    } else {
      if (!mHasSuspendedByBackPressure) {
        AccumulateCategorical(
            Telemetry::LABELS_NETWORK_BACK_PRESSURE_SUSPENSION_RATE_V2::
                NotSuspendedLocal);
      } else {
        AccumulateCategorical(
            Telemetry::LABELS_NETWORK_BACK_PRESSURE_SUSPENSION_RATE_V2::
                SuspendedLocal);
      }
    }
  }
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelParent::nsIStreamListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParent::OnDataAvailable(nsIRequest* aRequest,
                                   nsIInputStream* aInputStream,
                                   uint64_t aOffset, uint32_t aCount) {
  LOG(("HttpChannelParent::OnDataAvailable [this=%p aRequest=%p offset=%" PRIu64
       " count=%" PRIu32 "]\n",
       this, aRequest, aOffset, aCount));
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_RELEASE_ASSERT(!mDivertingFromChild,
                     "Cannot call OnDataAvailable if diverting is set!");

  nsresult channelStatus = NS_OK;
  mChannel->GetStatus(&channelStatus);

  nsresult transportStatus = NS_NET_STATUS_RECEIVING_FROM;
  RefPtr<nsHttpChannel> httpChannelImpl = do_QueryObject(mChannel);
  if (httpChannelImpl && httpChannelImpl->IsReadingFromCache()) {
    transportStatus = NS_NET_STATUS_READING;
  }

  static uint32_t const kCopyChunkSize = 128 * 1024;
  uint32_t toRead = std::min<uint32_t>(aCount, kCopyChunkSize);

  nsCString data;

  int32_t count = static_cast<int32_t>(aCount);

  while (aCount) {
    nsresult rv = NS_ReadInputStreamToString(aInputStream, data, toRead);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Either IPC channel is closed or background channel
    // is ready to send OnTransportAndData.
    MOZ_ASSERT(mIPCClosed || mBgParent);

    if (mIPCClosed || !mBgParent || mDoingCrossProcessRedirect ||
        !mBgParent->OnTransportAndData(channelStatus, transportStatus, aOffset,
                                       toRead, data)) {
      return NS_ERROR_UNEXPECTED;
    }

    aOffset += toRead;
    aCount -= toRead;
    toRead = std::min<uint32_t>(aCount, kCopyChunkSize);
  }

  if (NeedFlowControl()) {
    // We're going to run out of sending window size
    if (mSendWindowSize > 0 && mSendWindowSize <= count) {
      MOZ_ASSERT(!mSuspendedForFlowControl);
      LOG(("  suspend the channel due to e10s backpressure"));
      Unused << mChannel->Suspend();
      mSuspendedForFlowControl = true;
      mHasSuspendedByBackPressure = true;
    } else if (!mResumedTimestamp.IsNull()) {
      // Calculate the delay when the first packet arrived after resume
      Telemetry::AccumulateTimeDelta(
          Telemetry::NETWORK_BACK_PRESSURE_SUSPENSION_DELAY_TIME_MS,
          mResumedTimestamp);
      mResumedTimestamp = TimeStamp();
    }
    mSendWindowSize -= count;
  }

  return NS_OK;
}

bool HttpChannelParent::NeedFlowControl() {
  if (mCacheNeedFlowControlInitialized) {
    return mNeedFlowControl;
  }

  int64_t contentLength = -1;

  RefPtr<nsHttpChannel> httpChannelImpl = do_QueryObject(mChannel);

  // By design, we won't trigger the flow control if
  // a. pref-out
  // b. the resource is from cache or partial cache
  // c. the resource is small
  // Note that we served the cached resource first for partical cache, which is
  // ignored here since we only take the first ODA into consideration.
  if (gHttpHandler->SendWindowSize() == 0 || !httpChannelImpl ||
      httpChannelImpl->IsReadingFromCache() ||
      NS_FAILED(httpChannelImpl->GetContentLength(&contentLength)) ||
      contentLength < gHttpHandler->SendWindowSize()) {
    mNeedFlowControl = false;
  }
  mCacheNeedFlowControlInitialized = true;
  return mNeedFlowControl;
}

mozilla::ipc::IPCResult HttpChannelParent::RecvBytesRead(
    const int32_t& aCount) {
  // no more flow control after diviersion starts
  if (!NeedFlowControl() || mDivertingFromChild) {
    return IPC_OK();
  }

  LOG(("HttpChannelParent::RecvBytesRead [this=%p count=%" PRId32 "]\n", this,
       aCount));

  if (mSendWindowSize <= 0 && mSendWindowSize + aCount > 0) {
    MOZ_ASSERT(mSuspendedForFlowControl);
    LOG(("  resume the channel due to e10s backpressure relief"));
    Unused << mChannel->Resume();
    mSuspendedForFlowControl = false;

    mResumedTimestamp = TimeStamp::Now();
  }
  mSendWindowSize += aCount;
  return IPC_OK();
}

mozilla::ipc::IPCResult HttpChannelParent::RecvOpenOriginalCacheInputStream() {
  if (mIPCClosed) {
    return IPC_OK();
  }
  AutoIPCStream autoStream;
  if (mCacheEntry) {
    nsCOMPtr<nsIInputStream> inputStream;
    nsresult rv = mCacheEntry->OpenInputStream(0, getter_AddRefs(inputStream));
    if (NS_SUCCEEDED(rv)) {
      PContentParent* pcp = Manager()->Manager();
      Unused << autoStream.Serialize(inputStream,
                                     static_cast<ContentParent*>(pcp));
    }
  }

  Unused << SendOriginalCacheInputStreamAvailable(
      autoStream.TakeOptionalValue());
  return IPC_OK();
}

mozilla::ipc::IPCResult HttpChannelParent::RecvOpenAltDataCacheInputStream(
    const nsCString& aType) {
  if (mIPCClosed) {
    return IPC_OK();
  }
  AutoIPCStream autoStream;
  if (mCacheEntry) {
    nsCOMPtr<nsIInputStream> inputStream;
    nsresult rv = mCacheEntry->OpenAlternativeInputStream(
        aType, getter_AddRefs(inputStream));
    if (NS_SUCCEEDED(rv)) {
      PContentParent* pcp = Manager()->Manager();
      Unused << autoStream.Serialize(inputStream,
                                     static_cast<ContentParent*>(pcp));
    }
  }

  Unused << SendAltDataCacheInputStreamAvailable(
      autoStream.TakeOptionalValue());
  return IPC_OK();
}

//-----------------------------------------------------------------------------
// HttpChannelParent::nsIProgressEventSink
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParent::OnProgress(nsIRequest* aRequest, nsISupports* aContext,
                              int64_t aProgress, int64_t aProgressMax) {
  LOG(("HttpChannelParent::OnStatus [this=%p progress=%" PRId64 "max=%" PRId64
       "]\n",
       this, aProgress, aProgressMax));
  MOZ_ASSERT(NS_IsMainThread());

  // If it indicates this precedes OnDataAvailable, child can derive the value
  // in ODA.
  if (mIgnoreProgress) {
    mIgnoreProgress = false;
    return NS_OK;
  }

  // Either IPC channel is closed or background channel
  // is ready to send OnProgress.
  MOZ_ASSERT(mIPCClosed || mBgParent);

  // Send OnProgress events to the child for data upload progress notifications
  // (i.e. status == NS_NET_STATUS_SENDING_TO) or if the channel has
  // LOAD_BACKGROUND set.
  if (mIPCClosed || !mBgParent ||
      !mBgParent->OnProgress(aProgress, aProgressMax)) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

NS_IMETHODIMP
HttpChannelParent::OnStatus(nsIRequest* aRequest, nsISupports* aContext,
                            nsresult aStatus, const char16_t* aStatusArg) {
  LOG(("HttpChannelParent::OnStatus [this=%p status=%" PRIx32 "]\n", this,
       static_cast<uint32_t>(aStatus)));
  MOZ_ASSERT(NS_IsMainThread());

  // If this precedes OnDataAvailable, transportStatus will be derived in ODA.
  if (aStatus == NS_NET_STATUS_RECEIVING_FROM ||
      aStatus == NS_NET_STATUS_READING) {
    // The transport status and progress generated by ODA will be coalesced
    // into one IPC message. Therefore, we can ignore the next OnProgress event
    // since it is generated by ODA as well.
    mIgnoreProgress = true;
    return NS_OK;
  }

  // Either IPC channel is closed or background channel
  // is ready to send OnStatus.
  MOZ_ASSERT(mIPCClosed || mBgParent);

  // Otherwise, send to child now
  if (mIPCClosed || !mBgParent || !mBgParent->OnStatus(aStatus)) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelParent::nsIParentChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParent::SetParentListener(HttpChannelParentListener* aListener) {
  LOG(("HttpChannelParent::SetParentListener [this=%p aListener=%p]\n", this,
       aListener));
  MOZ_ASSERT(aListener);
  MOZ_ASSERT(!mParentListener,
             "SetParentListener should only be called for "
             "new HttpChannelParents after a redirect, when "
             "mParentListener is null.");
  mParentListener = aListener;
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelParent::NotifyChannelClassifierProtectionDisabled(
    uint32_t aAcceptedReason) {
  LOG(
      ("HttpChannelParent::NotifyChannelClassifierProtectionDisabled [this=%p "
       "aAcceptedReason=%" PRIu32 "]\n",
       this, aAcceptedReason));
  if (!mIPCClosed) {
    MOZ_ASSERT(mBgParent);
    Unused << NS_WARN_IF(
        !mBgParent->OnNotifyChannelClassifierProtectionDisabled(
            aAcceptedReason));
  }
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelParent::NotifyCookieAllowed() {
  LOG(("HttpChannelParent::NotifyCookieAllowed [this=%p]\n", this));
  if (!mIPCClosed) {
    MOZ_ASSERT(mBgParent);
    Unused << NS_WARN_IF(!mBgParent->OnNotifyCookieAllowed());
  }
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelParent::NotifyCookieBlocked(uint32_t aRejectedReason) {
  LOG(("HttpChannelParent::NotifyCookieBlocked [this=%p]\n", this));
  if (!mIPCClosed) {
    MOZ_ASSERT(mBgParent);
    Unused << NS_WARN_IF(!mBgParent->OnNotifyCookieBlocked(aRejectedReason));
  }
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelParent::SetClassifierMatchedInfo(const nsACString& aList,
                                            const nsACString& aProvider,
                                            const nsACString& aFullHash) {
  LOG(("HttpChannelParent::SetClassifierMatchedInfo [this=%p]\n", this));
  if (!mIPCClosed) {
    MOZ_ASSERT(mBgParent);
    Unused << mBgParent->OnSetClassifierMatchedInfo(aList, aProvider,
                                                    aFullHash);
  }
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelParent::SetClassifierMatchedTrackingInfo(
    const nsACString& aLists, const nsACString& aFullHashes) {
  LOG(("HttpChannelParent::SetClassifierMatchedTrackingInfo [this=%p]\n",
       this));
  if (!mIPCClosed) {
    MOZ_ASSERT(mBgParent);
    Unused << mBgParent->OnSetClassifierMatchedTrackingInfo(aLists,
                                                            aFullHashes);
  }
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelParent::NotifyClassificationFlags(uint32_t aClassificationFlags,
                                             bool aIsThirdParty) {
  LOG(
      ("HttpChannelParent::NotifyClassificationFlags "
       "classificationFlags=%" PRIu32 ", thirdparty=%d [this=%p]\n",
       aClassificationFlags, static_cast<int>(aIsThirdParty), this));
  if (!mIPCClosed) {
    MOZ_ASSERT(mBgParent);
    Unused << mBgParent->OnNotifyClassificationFlags(aClassificationFlags,
                                                     aIsThirdParty);
  }
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelParent::NotifyFlashPluginStateChanged(
    nsIHttpChannel::FlashPluginState aState) {
  LOG(("HttpChannelParent::NotifyFlashPluginStateChanged [this=%p]\n", this));
  if (!mIPCClosed) {
    MOZ_ASSERT(mBgParent);
    Unused << mBgParent->OnNotifyFlashPluginStateChanged(aState);
  }
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelParent::Delete() {
  if (!mIPCClosed) Unused << DoSendDeleteSelf();

  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelParent::nsIParentRedirectingChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParent::StartRedirect(uint32_t registrarId, nsIChannel* newChannel,
                                 uint32_t redirectFlags,
                                 nsIAsyncVerifyRedirectCallback* callback) {
  nsresult rv;

  LOG(("HttpChannelParent::StartRedirect [this=%p, registrarId=%" PRIu32 " "
       "newChannel=%p callback=%p]\n",
       this, registrarId, newChannel, callback));

  if (mIPCClosed) {
    return NS_BINDING_ABORTED;
  }

  // If this is an internal redirect for service worker interception, then
  // hide it from the child process.  The original e10s interception code
  // was not designed with this in mind and its not necessary to replace
  // the HttpChannelChild/Parent objects in this case.
  if (redirectFlags & nsIChannelEventSink::REDIRECT_INTERNAL) {
    nsCOMPtr<nsIInterceptedChannel> oldIntercepted =
        do_QueryInterface(static_cast<nsIChannel*>(mChannel.get()));
    nsCOMPtr<nsIInterceptedChannel> newIntercepted =
        do_QueryInterface(newChannel);

    // We only want to hide the special internal redirect from nsHttpChannel
    // to InterceptedHttpChannel.  We want to allow through internal redirects
    // initiated from the InterceptedHttpChannel even if they are to another
    // InterceptedHttpChannel.
    if (!oldIntercepted && newIntercepted) {
      // We need to move across the reserved and initial client information
      // to the new channel.  Normally this would be handled by the child
      // ClientChannelHelper, but that is not notified of this redirect since
      // we're not propagating it back to the child process.
      nsCOMPtr<nsILoadInfo> oldLoadInfo = mChannel->LoadInfo();

      nsCOMPtr<nsILoadInfo> newLoadInfo = newChannel->LoadInfo();

      Maybe<ClientInfo> reservedClientInfo(
          oldLoadInfo->GetReservedClientInfo());
      if (reservedClientInfo.isSome()) {
        newLoadInfo->SetReservedClientInfo(reservedClientInfo.ref());
      }

      Maybe<ClientInfo> initialClientInfo(oldLoadInfo->GetInitialClientInfo());
      if (initialClientInfo.isSome()) {
        newLoadInfo->SetInitialClientInfo(initialClientInfo.ref());
      }

      // Re-link the HttpChannelParent to the new InterceptedHttpChannel.
      nsCOMPtr<nsIChannel> linkedChannel;
      rv = NS_LinkRedirectChannels(registrarId, this,
                                   getter_AddRefs(linkedChannel));
      NS_ENSURE_SUCCESS(rv, rv);
      MOZ_ASSERT(linkedChannel == newChannel);

      // We immediately store the InterceptedHttpChannel as our nested
      // mChannel.  None of the redirect IPC messaging takes place.
      mChannel = do_QueryObject(newChannel);

      callback->OnRedirectVerifyCallback(NS_OK);
      return NS_OK;
    }
  }

  // Sending down the original URI, because that is the URI we have
  // to construct the channel from - this is the URI we've been actually
  // redirected to.  URI of the channel may be an inner channel URI.
  // URI of the channel will be reconstructed by the protocol handler
  // on the child process, no need to send it then.
  nsCOMPtr<nsIURI> newOriginalURI;
  newChannel->GetOriginalURI(getter_AddRefs(newOriginalURI));

  URIParams uriParams;
  SerializeURI(newOriginalURI, uriParams);

  uint32_t newLoadFlags = nsIRequest::LOAD_NORMAL;
  MOZ_ALWAYS_SUCCEEDS(newChannel->GetLoadFlags(&newLoadFlags));

  nsCString secInfoSerialization;
  UpdateAndSerializeSecurityInfo(secInfoSerialization);

  // If the channel is a HTTP channel, we also want to inform the child
  // about the parent's channelId attribute, so that both parent and child
  // share the same ID. Useful for monitoring channel activity in devtools.
  uint64_t channelId = 0;
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(newChannel);
  if (httpChannel) {
    rv = httpChannel->GetChannelId(&channelId);
    NS_ENSURE_SUCCESS(rv, NS_BINDING_ABORTED);
  }

  nsCOMPtr<nsILoadInfo> loadInfo = mChannel->LoadInfo();

  ParentLoadInfoForwarderArgs loadInfoForwarderArg;
  mozilla::ipc::LoadInfoToParentLoadInfoForwarder(loadInfo,
                                                  &loadInfoForwarderArg);

  nsHttpResponseHead* responseHead = mChannel->GetResponseHead();

  nsHttpResponseHead cleanedUpResponseHead;
  if (responseHead && responseHead->HasHeader(nsHttp::Set_Cookie)) {
    cleanedUpResponseHead = *responseHead;
    cleanedUpResponseHead.ClearHeader(nsHttp::Set_Cookie);
    responseHead = &cleanedUpResponseHead;
  }

  if (!responseHead) {
    responseHead = &cleanedUpResponseHead;
  }

  bool result = false;
  if (!mIPCClosed) {
    result = SendRedirect1Begin(registrarId, uriParams, newLoadFlags,
                                redirectFlags, loadInfoForwarderArg,
                                *responseHead, secInfoSerialization, channelId,
                                mChannel->GetPeerAddr());
  }
  if (!result) {
    // Bug 621446 investigation
    mSentRedirect1BeginFailed = true;
    return NS_BINDING_ABORTED;
  }

  // Bug 621446 investigation
  // Store registrar Id of the new channel to find the redirect
  // HttpChannelParent later in verification phase.
  mRedirectRegistrarId = registrarId;

  // Result is handled in RecvRedirect2Verify above

  mRedirectChannel = newChannel;
  mRedirectCallback = callback;
  return NS_OK;
}

void HttpChannelParent::CancelChildCrossProcessRedirect() {
  MOZ_ASSERT(!mDoingCrossProcessRedirect, "Already redirected");
  MOZ_ASSERT(NS_IsMainThread());

  mDoingCrossProcessRedirect = true;
  if (!mIPCClosed) {
    Unused << SendCancelRedirected();
  }
}

already_AddRefed<HttpChannelParentListener>
HttpChannelParent::GetParentListener() {
  RefPtr<HttpChannelParentListener> listener = mParentListener;
  return listener.forget();
}

NS_IMETHODIMP
HttpChannelParent::CompleteRedirect(bool succeeded) {
  LOG(("HttpChannelParent::CompleteRedirect [this=%p succeeded=%d]\n", this,
       succeeded));

  if (mDoingCrossProcessRedirect) {
    LOG(("Child was cancelled for cross-process redirect. Bail."));
    return NS_OK;
  }

  // If this was an internal redirect for a service worker interception then
  // we will not have a redirecting channel here.  Hide this redirect from
  // the child.
  if (!mRedirectChannel) {
    return NS_OK;
  }

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
nsresult HttpChannelParent::SuspendForDiversion() {
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
  nsCOMPtr<nsIChannelWithDivertableParentListener> divertChannel =
      do_QueryInterface(static_cast<nsIChannel*>(mChannel.get()));
  divertChannel->MessageDiversionStarted(this);

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
    rv = divertChannel->SuspendInternal();
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

  // After we suspend for diversion, we don't need the flow control since the
  // channel is suspended until all the data is consumed and no more e10s later.
  // No point to have another redundant suspension.
  if (mSuspendedForFlowControl) {
    LOG(("  resume the channel due to e10s backpressure relief by diversion"));
    Unused << mChannel->Resume();
    mSuspendedForFlowControl = false;
  }

  // Once this is set, no more OnStart/OnData/OnStop callbacks should be sent
  // to the child.
  mDivertingFromChild = true;

  return NS_OK;
}

nsresult HttpChannelParent::SuspendMessageDiversion() {
  LOG(("HttpChannelParent::SuspendMessageDiversion [this=%p]", this));
  // This only needs to suspend message queue.
  mEventQ->Suspend();
  return NS_OK;
}

nsresult HttpChannelParent::ResumeMessageDiversion() {
  LOG(("HttpChannelParent::SuspendMessageDiversion [this=%p]", this));
  // This only needs to resumes message queue.
  mEventQ->Resume();
  return NS_OK;
}

nsresult HttpChannelParent::CancelDiversion() {
  LOG(("HttpChannelParent::CancelDiversion [this=%p]", this));
  if (!mIPCClosed) {
    Unused << SendCancelDiversion();
  }
  return NS_OK;
}

/* private, supporting function for ADivertableParentChannel */
nsresult HttpChannelParent::ResumeForDiversion() {
  LOG(("HttpChannelParent::ResumeForDiversion [this=%p]\n", this));
  MOZ_ASSERT(mChannel);
  if (NS_WARN_IF(!mDivertingFromChild)) {
    MOZ_ASSERT(mDivertingFromChild,
               "Cannot ResumeForDiversion if not diverting!");
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsIChannelWithDivertableParentListener> divertChannel =
      do_QueryInterface(static_cast<nsIChannel*>(mChannel.get()));
  divertChannel->MessageDiversionStop();

  if (mSuspendedForDiversion) {
    // The nsHttpChannel will deliver remaining OnData/OnStop for the transfer.
    nsresult rv = divertChannel->ResumeInternal();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    mSuspendedForDiversion = false;
  }

  if (NS_WARN_IF(mIPCClosed || !DoSendDeleteSelf())) {
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

void HttpChannelParent::DivertTo(nsIStreamListener* aListener) {
  LOG(("HttpChannelParent::DivertTo [this=%p aListener=%p]\n", this,
       aListener));
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
      NewRunnableMethod("net::HttpChannelParent::StartDiversion", this,
                        &HttpChannelParent::StartDiversion));
}

void HttpChannelParent::StartDiversion() {
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
    nsresult rv = mDivertListener->OnStartRequest(mChannel);
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
  Unused << mChannel->DoApplyContentConversions(
      mDivertListener, getter_AddRefs(converterListener));
  if (converterListener) {
    mDivertListener = converterListener.forget();
  }

  // Now mParentListener can be diverted to mDivertListener.
  DebugOnly<nsresult> rvdbg = mParentListener->DivertTo(mDivertListener);
  MOZ_ASSERT(NS_SUCCEEDED(rvdbg));
  mDivertListener = nullptr;

  // Either IPC channel is closed or background channel
  // is ready to send FlushedForDiversion and DivertMessages.
  MOZ_ASSERT(mIPCClosed || mBgParent);

  if (NS_WARN_IF(mIPCClosed || !mBgParent || !mBgParent->OnDiversion())) {
    FailDiversion(NS_ERROR_UNEXPECTED);
    return;
  }
}

class HTTPFailDiversionEvent : public Runnable {
 public:
  HTTPFailDiversionEvent(HttpChannelParent* aChannelParent, nsresult aErrorCode)
      : Runnable("net::HTTPFailDiversionEvent"),
        mChannelParent(aChannelParent),
        mErrorCode(aErrorCode) {
    MOZ_RELEASE_ASSERT(aChannelParent);
    MOZ_RELEASE_ASSERT(NS_FAILED(aErrorCode));
  }
  NS_IMETHOD Run() override {
    mChannelParent->NotifyDiversionFailed(mErrorCode);
    return NS_OK;
  }

 private:
  RefPtr<HttpChannelParent> mChannelParent;
  nsresult mErrorCode;
};

void HttpChannelParent::FailDiversion(nsresult aErrorCode) {
  MOZ_RELEASE_ASSERT(NS_FAILED(aErrorCode));
  MOZ_RELEASE_ASSERT(mDivertingFromChild);
  MOZ_RELEASE_ASSERT(mParentListener);
  MOZ_RELEASE_ASSERT(mChannel);

  NS_DispatchToCurrentThread(new HTTPFailDiversionEvent(this, aErrorCode));
}

void HttpChannelParent::NotifyDiversionFailed(nsresult aErrorCode) {
  LOG(("HttpChannelParent::NotifyDiversionFailed [this=%p aErrorCode=%" PRIx32
       "]\n",
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
    nsCOMPtr<nsIChannelWithDivertableParentListener> divertChannel =
        do_QueryInterface(static_cast<nsIChannel*>(mChannel.get()));
    divertChannel->ResumeInternal();
  }
  // Channel has already sent OnStartRequest to the child, so ensure that we
  // call it here if it hasn't already been called.
  if (!mDivertedOnStartRequest) {
    mChannel->ForcePending(true);
    mParentListener->OnStartRequest(mChannel);
    mChannel->ForcePending(false);
  }
  // If the channel is pending, it will call OnStopRequest itself; otherwise, do
  // it here.
  if (!isPending) {
    mParentListener->OnStopRequest(mChannel, aErrorCode);
  }

  if (!mIPCClosed) {
    Unused << DoSendDeleteSelf();
  }

  // DoSendDeleteSelf will need channel Id to remove the strong reference in
  // BackgroundChannelRegistrar if channel pairing is aborted.
  // Thus we need to keep mChannel until DoSendDeleteSelf is done.
  mParentListener = nullptr;
  mChannel = nullptr;
}

nsresult HttpChannelParent::OpenAlternativeOutputStream(
    const nsACString& type, int64_t predictedSize,
    nsIAsyncOutputStream** _retval) {
  // We need to make sure the child does not call SendDocumentChannelCleanup()
  // before opening the altOutputStream, because that clears mCacheEntry.
  if (!mCacheEntry) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  nsresult rv =
      mCacheEntry->OpenAlternativeOutputStream(type, predictedSize, _retval);
  if (NS_SUCCEEDED(rv)) {
    mCacheEntry->SetMetaDataElement("alt-data-from-child", "1");
  }
  return rv;
}

NS_IMETHODIMP
HttpChannelParent::GetAuthPrompt(uint32_t aPromptReason, const nsIID& iid,
                                 void** aResult) {
  nsCOMPtr<nsIAuthPrompt2> prompt =
      new NeckoParent::NestedFrameAuthPrompt(Manager(), mNestedFrameId);
  prompt.forget(aResult);
  return NS_OK;
}

void HttpChannelParent::UpdateAndSerializeSecurityInfo(
    nsACString& aSerializedSecurityInfoOut) {
  nsCOMPtr<nsISupports> secInfoSupp;
  mChannel->GetSecurityInfo(getter_AddRefs(secInfoSupp));
  if (secInfoSupp) {
    nsCOMPtr<nsISerializable> secInfoSer = do_QueryInterface(secInfoSupp);
    if (secInfoSer) {
      NS_SerializeToString(secInfoSer, aSerializedSecurityInfoOut);
    }
  }
}

bool HttpChannelParent::DoSendDeleteSelf() {
  mIPCClosed = true;
  bool rv = SendDeleteSelf();

  CleanupBackgroundChannel();

  return rv;
}

mozilla::ipc::IPCResult HttpChannelParent::RecvDeletingChannel() {
  // We need to ensure that the parent channel will not be sending any more IPC
  // messages after this, as the child is going away. DoSendDeleteSelf will
  // set mIPCClosed = true;
  if (!DoSendDeleteSelf()) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult HttpChannelParent::RecvFinishInterceptedRedirect() {
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

nsresult HttpChannelParent::ReportSecurityMessage(
    const nsAString& aMessageTag, const nsAString& aMessageCategory) {
  if (mIPCClosed || NS_WARN_IF(!SendReportSecurityMessage(
                        nsString(aMessageTag), nsString(aMessageCategory)))) {
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelParent::IssueWarning(uint32_t aWarning, bool aAsError) {
  Unused << SendIssueDeprecationWarning(aWarning, aAsError);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsIAsyncVerifyRedirectReadyCallback
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParent::ReadyToVerify(nsresult aResult) {
  LOG(("HttpChannelParent::ReadyToVerify [this=%p result=%" PRIx32 "]\n", this,
       static_cast<uint32_t>(aResult)));
  MOZ_ASSERT(NS_IsMainThread());

  ContinueRedirect2Verify(aResult);

  return NS_OK;
}

void HttpChannelParent::DoSendSetPriority(int16_t aValue) {
  if (!mIPCClosed) {
    Unused << SendSetPriority(aValue);
  }
}

nsresult HttpChannelParent::LogBlockedCORSRequest(const nsAString& aMessage,
                                                  const nsACString& aCategory) {
  if (mIPCClosed || NS_WARN_IF(!SendLogBlockedCORSRequest(
                        nsString(aMessage), nsCString(aCategory)))) {
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

nsresult HttpChannelParent::LogMimeTypeMismatch(const nsACString& aMessageName,
                                                bool aWarning,
                                                const nsAString& aURL,
                                                const nsAString& aContentType) {
  if (mIPCClosed || NS_WARN_IF(!SendLogMimeTypeMismatch(
                        nsCString(aMessageName), aWarning, nsString(aURL),
                        nsString(aContentType)))) {
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
