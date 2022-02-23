/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "mozilla/ConsoleReportCollector.h"
#include "mozilla/ipc/FileDescriptorSetParent.h"
#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/net/HttpChannelParent.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentProcessManager.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ServiceWorkerUtils.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/net/NeckoParent.h"
#include "mozilla/InputStreamLengthHelper.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/StoragePrincipalHelper.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "HttpBackgroundChannelParent.h"
#include "ParentChannelListener.h"
#include "nsICacheInfoChannel.h"
#include "nsHttpHandler.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsISupportsPriority.h"
#include "mozilla/net/BackgroundChannelRegistrar.h"
#include "nsSerializationHelper.h"
#include "nsISerializable.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/ipc/URIUtils.h"
#include "SerializedLoadContext.h"
#include "nsIAuthPrompt.h"
#include "nsIAuthPrompt2.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/LoadInfo.h"
#include "nsQueryObject.h"
#include "mozilla/BasePrincipal.h"
#include "nsCORSListenerProxy.h"
#include "nsIIPCSerializableInputStream.h"
#include "nsIPrompt.h"
#include "nsIPromptFactory.h"
#include "mozilla/net/ChannelEventQueue.h"
#include "mozilla/net/RedirectChannelRegistrar.h"
#include "nsIWindowWatcher.h"
#include "mozilla/dom/Document.h"
#include "nsISecureBrowserUI.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"
#include "nsThreadUtils.h"
#include "nsQueryObject.h"
#include "nsIMultiPartChannel.h"
#include "nsIViewSourceChannel.h"

using mozilla::BasePrincipal;
using namespace mozilla::dom;
using namespace mozilla::ipc;

namespace mozilla {
namespace net {

HttpChannelParent::HttpChannelParent(dom::BrowserParent* iframeEmbedding,
                                     nsILoadContext* aLoadContext,
                                     PBOverrideStatus aOverrideStatus)
    : mLoadContext(aLoadContext),
      mIPCClosed(false),
      mPBOverride(aOverrideStatus),
      mStatus(NS_OK),
      mIgnoreProgress(false),
      mSentRedirect1BeginFailed(false),
      mReceivedRedirect2Verify(false),
      mHasSuspendedByBackPressure(false),
      mCacheNeedFlowControlInitialized(false),
      mNeedFlowControl(true),
      mSuspendedForFlowControl(false),
      mAfterOnStartRequestBegun(false),
      mDataSentToChildProcess(false) {
  LOG(("Creating HttpChannelParent [this=%p]\n", this));

  // Ensure gHttpHandler is initialized: we need the atom table up and running.
  nsCOMPtr<nsIHttpProtocolHandler> dummyInitializer =
      do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http");

  MOZ_ASSERT(gHttpHandler);
  mHttpHandler = gHttpHandler;

  mBrowserParent = iframeEmbedding;

  mSendWindowSize = gHttpHandler->SendWindowSize();

  mEventQ =
      new ChannelEventQueue(static_cast<nsIParentRedirectingChannel*>(this));
}

HttpChannelParent::~HttpChannelParent() {
  LOG(("Destroying HttpChannelParent [this=%p]\n", this));
  CleanupBackgroundChannel();

  MOZ_ASSERT(!mRedirectCallback);
  if (NS_WARN_IF(mRedirectCallback)) {
    mRedirectCallback->OnRedirectVerifyCallback(NS_ERROR_UNEXPECTED);
    mRedirectCallback = nullptr;
  }
  mEventQ->NotifyReleasingOwner();
}

void HttpChannelParent::ActorDestroy(ActorDestroyReason why) {
  // We may still have refcount>0 if nsHttpChannel hasn't called OnStopRequest
  // yet, but child process has crashed.  We must not try to send any more msgs
  // to child, or IPDL will kill chrome process, too.
  mIPCClosed = true;
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
          a.allowSpdy(), a.allowHttp3(), a.allowAltSvc(), a.beConservative(),
          a.bypassProxy(), a.tlsFlags(), a.loadInfo(), a.cacheKey(),
          a.requestContextID(), a.preflightArgs(), a.initialRwin(),
          a.blockAuthPrompt(), a.allowStaleCacheContent(),
          a.preferCacheLoadOverBypass(), a.contentTypeHint(), a.corsMode(),
          a.redirectMode(), a.channelId(), a.integrityMetadata(),
          a.contentWindowId(), a.preferredAlternativeTypes(),
          a.topBrowsingContextId(), a.launchServiceWorkerStart(),
          a.launchServiceWorkerEnd(), a.dispatchFetchEventStart(),
          a.dispatchFetchEventEnd(), a.handleFetchEventStart(),
          a.handleFetchEventEnd(), a.forceMainDocumentChannel(),
          a.navigationStartTimeStamp());
    }
    case HttpChannelCreationArgs::THttpChannelConnectArgs: {
      const HttpChannelConnectArgs& cArgs = aArgs.get_HttpChannelConnectArgs();
      return ConnectChannel(cArgs.registrarId());
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
    RefPtr<HttpBackgroundChannelParent> bgParent = std::move(mBgParent);
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
  return PHttpChannelParent::OtherPid();
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
  NS_INTERFACE_MAP_ENTRY(nsIParentRedirectingChannel)
  NS_INTERFACE_MAP_ENTRY(nsIAsyncVerifyRedirectReadyCallback)
  NS_INTERFACE_MAP_ENTRY(nsIChannelEventSink)
  NS_INTERFACE_MAP_ENTRY(nsIRedirectResultListener)
  NS_INTERFACE_MAP_ENTRY(nsIMultiPartChannelListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIParentRedirectingChannel)
  NS_INTERFACE_MAP_ENTRY_CONCRETE(HttpChannelParent)
NS_INTERFACE_MAP_END

//-----------------------------------------------------------------------------
// HttpChannelParent::nsIInterfaceRequestor
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParent::GetInterface(const nsIID& aIID, void** result) {
  // A system XHR can be created without reference to a window, hence mTabParent
  // may be null.  In that case we want to let the window watcher pick a prompt
  // directly.
  if (!mBrowserParent && (aIID.Equals(NS_GET_IID(nsIAuthPrompt)) ||
                          aIID.Equals(NS_GET_IID(nsIAuthPrompt2)))) {
    nsresult rv;
    nsCOMPtr<nsIWindowWatcher> wwatch =
        do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_NO_INTERFACE);

    bool hasWindowCreator = false;
    Unused << wwatch->HasWindowCreator(&hasWindowCreator);
    if (!hasWindowCreator) {
      return NS_ERROR_NO_INTERFACE;
    }

    nsCOMPtr<nsIPromptFactory> factory = do_QueryInterface(wwatch);
    if (!factory) {
      return NS_ERROR_NO_INTERFACE;
    }
    rv = factory->GetPrompt(nullptr, aIID, reinterpret_cast<void**>(result));
    if (NS_FAILED(rv)) {
      return NS_ERROR_NO_INTERFACE;
    }
    return NS_OK;
  }

  // Only support nsILoadContext if child channel's callbacks did too
  if (aIID.Equals(NS_GET_IID(nsILoadContext)) && mLoadContext) {
    nsCOMPtr<nsILoadContext> copy = mLoadContext;
    copy.forget(result);
    return NS_OK;
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
  // ParentChannelListener, and nsHttpChannel to avoid memory leakage.
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
    const uint64_t& startPos, const nsCString& entityID, const bool& allowSpdy,
    const bool& allowHttp3, const bool& allowAltSvc, const bool& beConservative,
    const bool& bypassProxy, const uint32_t& tlsFlags,
    const Maybe<LoadInfoArgs>& aLoadInfoArgs, const uint32_t& aCacheKey,
    const uint64_t& aRequestContextID,
    const Maybe<CorsPreflightArgs>& aCorsPreflightArgs,
    const uint32_t& aInitialRwin, const bool& aBlockAuthPrompt,
    const bool& aAllowStaleCacheContent, const bool& aPreferCacheLoadOverBypass,
    const nsCString& aContentTypeHint, const uint32_t& aCorsMode,
    const uint32_t& aRedirectMode, const uint64_t& aChannelId,
    const nsString& aIntegrityMetadata, const uint64_t& aContentWindowId,
    const nsTArray<PreferredAlternativeDataTypeParams>&
        aPreferredAlternativeTypes,
    const uint64_t& aTopBrowsingContextId,
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
       " top bid=%" PRIx64 "]\n",
       this, uri->GetSpecOrDefault().get(), aChannelId, aTopBrowsingContextId));

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
  httpChannel->SetTopBrowsingContextId(aTopBrowsingContextId);

  httpChannel->SetIntegrityMetadata(aIntegrityMetadata);

  RefPtr<nsHttpChannel> httpChannelImpl = do_QueryObject(httpChannel);
  if (httpChannelImpl) {
    httpChannelImpl->SetWarningReporter(this);
  }
  httpChannel->SetTimingEnabled(true);
  if (mPBOverride != kPBOverride_Unset) {
    httpChannel->SetPrivate(mPBOverride == kPBOverride_Private);
  }

  if (doResumeAt) httpChannel->ResumeAt(startPos, entityID);

  if (originalUri) httpChannel->SetOriginalURI(originalUri);
  if (docUri) httpChannel->SetDocumentURI(docUri);
  if (aReferrerInfo) {
    // Referrer header is computed in child no need to recompute here
    rv =
        httpChannel->SetReferrerInfoInternal(aReferrerInfo, false, false, true);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  if (apiRedirectToUri) httpChannel->RedirectTo(apiRedirectToUri);
  if (topWindowUri) {
    httpChannel->SetTopWindowURI(topWindowUri);
  }

  if (aLoadFlags != nsIRequest::LOAD_NORMAL) {
    httpChannel->SetLoadFlags(aLoadFlags);
  }

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

  RefPtr<ParentChannelListener> parentListener = new ParentChannelListener(
      this, mBrowserParent ? mBrowserParent->GetBrowsingContext() : nullptr,
      mLoadContext && mLoadContext->UsePrivateBrowsing());

  httpChannel->SetRequestMethod(nsDependentCString(requestMethod.get()));

  if (aCorsPreflightArgs.isSome()) {
    const CorsPreflightArgs& args = aCorsPreflightArgs.ref();
    httpChannel->SetCorsPreflightParameters(args.unsafeHeaders(), false);
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

  nsCOMPtr<nsICacheInfoChannel> cacheChannel =
      do_QueryInterface(static_cast<nsIChannel*>(httpChannel.get()));
  if (cacheChannel) {
    cacheChannel->SetCacheKey(aCacheKey);
    for (const auto& data : aPreferredAlternativeTypes) {
      cacheChannel->PreferAlternativeDataType(data.type(), data.contentType(),
                                              data.deliverAltData());
    }

    cacheChannel->SetAllowStaleCacheContent(aAllowStaleCacheContent);
    cacheChannel->SetPreferCacheLoadOverBypass(aPreferCacheLoadOverBypass);

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
  httpChannel->SetAllowHttp3(allowHttp3);
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
  httpChannel->SetRequestContextID(aRequestContextID);

  // Store the strong reference of channel and parent listener object until
  // all the initialization procedure is complete without failure, to remove
  // cycle reference in fail case and to avoid memory leakage.
  mChannel = std::move(httpChannel);
  mParentListener = std::move(parentListener);
  mChannel->SetNotificationCallbacks(mParentListener);

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

  if (!mChannel) {
    return GenericNonExclusivePromise::CreateAndReject(NS_ERROR_FAILURE,
                                                       __func__);
  }

  nsCOMPtr<nsIBackgroundChannelRegistrar> registrar =
      BackgroundChannelRegistrar::GetOrCreate();
  MOZ_ASSERT(registrar);
  registrar->LinkHttpChannel(mChannel->ChannelId(), this);

  if (mBgParent) {
    return GenericNonExclusivePromise::CreateAndResolve(true, __func__);
  }

  return mPromise.Ensure(__func__);
}

bool HttpChannelParent::ConnectChannel(const uint32_t& registrarId) {
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

  if (mPBOverride != kPBOverride_Unset) {
    // redirected-to channel may not support PB
    nsCOMPtr<nsIPrivateBrowsingChannel> pbChannel = do_QueryObject(mChannel);
    if (pbChannel) {
      pbChannel->SetPrivate(mPBOverride == kPBOverride_Private);
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

mozilla::ipc::IPCResult HttpChannelParent::RecvCancel(
    const nsresult& status, const uint32_t& requestBlockingReason) {
  LOG(("HttpChannelParent::RecvCancel [this=%p]\n", this));

  // May receive cancel before channel has been constructed!
  if (mChannel) {
    mChannel->Cancel(status);

    if (MOZ_UNLIKELY(requestBlockingReason !=
                     nsILoadInfo::BLOCKING_REASON_NONE)) {
      nsCOMPtr<nsILoadInfo> loadInfo = mChannel->LoadInfo();
      loadInfo->SetRequestBlockingReason(requestBlockingReason);
    }

    // Once we receive |Cancel|, child will stop sending RecvBytesRead. Force
    // the channel resumed if needed.
    if (mSuspendedForFlowControl) {
      LOG(("  resume the channel due to e10s backpressure relief by cancel"));
      Unused << mChannel->Resume();
      mSuspendedForFlowControl = false;
    }
  } else if (!mIPCClosed) {
    // Make sure that the child correctly delivers all stream listener
    // notifications.
    Unused << SendFailedAsyncOpen(status);
  }

  // We won't need flow control anymore. Toggle the flag to avoid |Suspend|
  // since OnDataAvailable could be off-main-thread.
  mCacheNeedFlowControlInitialized = true;
  mNeedFlowControl = false;
  return IPC_OK();
}

mozilla::ipc::IPCResult HttpChannelParent::RecvRedirect2Verify(
    const nsresult& aResult, const RequestHeaderTuples& changedHeaders,
    const uint32_t& aSourceRequestBlockingReason,
    const Maybe<ChildLoadInfoForwarderArgs>& aTargetLoadInfoForwarder,
    const uint32_t& loadFlags, nsIReferrerInfo* aReferrerInfo,
    const Maybe<URIParams>& aAPIRedirectURI,
    const Maybe<CorsPreflightArgs>& aCorsPreflightArgs) {
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
        newInternalChannel->SetCorsPreflightParameters(args.unsafeHeaders(),
                                                       false);
      }

      if (aReferrerInfo) {
        RefPtr<HttpBaseChannel> baseChannel = do_QueryObject(newHttpChannel);
        MOZ_ASSERT(baseChannel);
        if (baseChannel) {
          // Referrer header is computed in child no need to recompute here
          rv = baseChannel->SetReferrerInfoInternal(aReferrerInfo, false, false,
                                                    true);
          MOZ_ASSERT(NS_SUCCEEDED(rv));
        }
      }

      if (aTargetLoadInfoForwarder.isSome()) {
        nsCOMPtr<nsILoadInfo> newLoadInfo = newHttpChannel->LoadInfo();
        rv = MergeChildLoadInfoForwarder(aTargetLoadInfoForwarder.ref(),
                                         newLoadInfo);
        if (NS_FAILED(rv) && NS_SUCCEEDED(result)) {
          result = rv;
        }
      }
    }
  }

  // If the redirect is vetoed, reason is set on the source (current) channel's
  // load info, so we must carry iver the change.
  // The channel may have already been cleaned up, so there is nothing we can
  // do.
  if (MOZ_UNLIKELY(aSourceRequestBlockingReason !=
                   nsILoadInfo::BLOCKING_REASON_NONE) &&
      mChannel) {
    nsCOMPtr<nsILoadInfo> sourceLoadInfo = mChannel->LoadInfo();
    sourceLoadInfo->SetRequestBlockingReason(aSourceRequestBlockingReason);
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
  rv = redirectReg->GetParentChannel(mRedirectChannelId,
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
    if (mReceivedRedirect2Verify) {
      LOG(("RecvRedirect2Verify[%p]: Duplicate fire", this));
    }
    if (mSentRedirect1BeginFailed) {
      LOG(("RecvRedirect2Verify[%p]: Send to child failed", this));
    }
    if ((mRedirectChannelId > 0) && NS_FAILED(aResult)) {
      LOG(("RecvRedirect2Verify[%p]: Redirect failed", this));
    }
    if ((mRedirectChannelId > 0) && NS_SUCCEEDED(aResult)) {
      LOG(("RecvRedirect2Verify[%p]: Redirect succeeded", this));
    }
    if (!mRedirectChannel) {
      LOG(("RecvRedirect2Verify[%p]: Missing redirect channel", this));
    }

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

mozilla::ipc::IPCResult HttpChannelParent::RecvRemoveCorsPreflightCacheEntry(
    const URIParams& uri,
    const mozilla::ipc::PrincipalInfo& requestingPrincipal,
    const OriginAttributes& originAttributes) {
  nsCOMPtr<nsIURI> deserializedURI = DeserializeURI(uri);
  if (!deserializedURI) {
    return IPC_FAIL_NO_REASON(this);
  }
  auto principalOrErr = PrincipalInfoToPrincipal(requestingPrincipal);
  if (NS_WARN_IF(principalOrErr.isErr())) {
    return IPC_FAIL_NO_REASON(this);
  }
  nsCOMPtr<nsIPrincipal> principal = principalOrErr.unwrap();
  nsCORSListenerProxy::RemoveFromCorsPreflightCache(deserializedURI, principal,
                                                    originAttributes);
  return IPC_OK();
}

//-----------------------------------------------------------------------------
// HttpChannelParent::nsIRequestObserver
//-----------------------------------------------------------------------------

static ResourceTimingStructArgs GetTimingAttributes(HttpBaseChannel* aChannel) {
  ResourceTimingStructArgs args;
  TimeStamp timeStamp;
  aChannel->GetDomainLookupStart(&timeStamp);
  args.domainLookupStart() = timeStamp;
  aChannel->GetDomainLookupEnd(&timeStamp);
  args.domainLookupEnd() = timeStamp;
  aChannel->GetConnectStart(&timeStamp);
  args.connectStart() = timeStamp;
  aChannel->GetTcpConnectEnd(&timeStamp);
  args.tcpConnectEnd() = timeStamp;
  aChannel->GetSecureConnectionStart(&timeStamp);
  args.secureConnectionStart() = timeStamp;
  aChannel->GetConnectEnd(&timeStamp);
  args.connectEnd() = timeStamp;
  aChannel->GetRequestStart(&timeStamp);
  args.requestStart() = timeStamp;
  aChannel->GetResponseStart(&timeStamp);
  args.responseStart() = timeStamp;
  aChannel->GetResponseEnd(&timeStamp);
  args.responseEnd() = timeStamp;
  aChannel->GetAsyncOpen(&timeStamp);
  args.fetchStart() = timeStamp;
  aChannel->GetRedirectStart(&timeStamp);
  args.redirectStart() = timeStamp;
  aChannel->GetRedirectEnd(&timeStamp);
  args.redirectEnd() = timeStamp;

  uint64_t size = 0;
  aChannel->GetTransferSize(&size);
  args.transferSize() = size;

  aChannel->GetEncodedBodySize(&size);
  args.encodedBodySize() = size;
  // decodedBodySize can be computed in the child process so it doesn't need
  // to be passed down.

  nsCString protocolVersion;
  aChannel->GetProtocolVersion(protocolVersion);
  args.protocolVersion() = protocolVersion;

  aChannel->GetCacheReadStart(&timeStamp);
  args.cacheReadStart() = timeStamp;

  aChannel->GetCacheReadEnd(&timeStamp);
  args.cacheReadEnd() = timeStamp;
  return args;
}

NS_IMETHODIMP
HttpChannelParent::OnStartRequest(nsIRequest* aRequest) {
  nsresult rv;

  LOG(("HttpChannelParent::OnStartRequest [this=%p, aRequest=%p]\n", this,
       aRequest));
  MOZ_ASSERT(NS_IsMainThread());

  Maybe<uint32_t> multiPartID;
  bool isLastPartOfMultiPart = false;
  DebugOnly<bool> isMultiPart = false;

  RefPtr<HttpBaseChannel> chan = do_QueryObject(aRequest);
  if (!chan) {
    if (nsCOMPtr<nsIMultiPartChannel> multiPartChannel =
            do_QueryInterface(aRequest)) {
      isMultiPart = true;
      nsCOMPtr<nsIChannel> baseChannel;
      multiPartChannel->GetBaseChannel(getter_AddRefs(baseChannel));
      chan = do_QueryObject(baseChannel);

      uint32_t partID = 0;
      multiPartChannel->GetPartID(&partID);
      multiPartID = Some(partID);
      multiPartChannel->GetIsLastPart(&isLastPartOfMultiPart);
    } else if (nsCOMPtr<nsIViewSourceChannel> viewSourceChannel =
                   do_QueryInterface(aRequest)) {
      chan = do_QueryObject(viewSourceChannel->GetInnerChannel());
    }
  }
  MOZ_ASSERT(multiPartID || !isMultiPart, "Changed multi-part state?");

  if (!chan) {
    LOG(("  aRequest is not HttpBaseChannel"));
    NS_ERROR(
        "Expecting only HttpBaseChannel as aRequest in "
        "HttpChannelParent::OnStartRequest");
    return NS_ERROR_UNEXPECTED;
  }

  mAfterOnStartRequestBegun = true;

  // Todo: re-enable when bug 1589749 is fixed.
  /*MOZ_ASSERT(mChannel == chan,
             "HttpChannelParent getting OnStartRequest from a different "
             "HttpBaseChannel instance");*/

  HttpChannelOnStartRequestArgs args;

  // Send down any permissions/cookies which are relevant to this URL if we are
  // performing a document load. We can't do that if mIPCClosed is set.
  if (!mIPCClosed) {
    PContentParent* pcp = Manager()->Manager();
    MOZ_ASSERT(pcp, "We should have a manager if our IPC isn't closed");
    DebugOnly<nsresult> rv =
        static_cast<ContentParent*>(pcp)->AboutToLoadHttpFtpDocumentForChild(
            chan, &args.shouldWaitForOnStartRequestSent());
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  args.multiPartID() = multiPartID;
  args.isLastPartOfMultiPart() = isLastPartOfMultiPart;

  args.cacheExpirationTime() = nsICacheEntry::NO_EXPIRATION_TIME;

  RefPtr<nsHttpChannel> httpChannelImpl = do_QueryObject(chan);

  if (httpChannelImpl) {
    httpChannelImpl->IsFromCache(&args.isFromCache());
    httpChannelImpl->IsRacing(&args.isRacing());
    httpChannelImpl->GetCacheEntryId(&args.cacheEntryId());
    httpChannelImpl->GetCacheTokenFetchCount(&args.cacheFetchCount());
    httpChannelImpl->GetCacheTokenExpirationTime(&args.cacheExpirationTime());

    mDataSentToChildProcess = httpChannelImpl->DataSentToChildProcess();

    // If RCWN is enabled and cache wins, we can't use the ODA from socket
    // process.
    if (args.isRacing()) {
      mDataSentToChildProcess =
          httpChannelImpl->DataSentToChildProcess() && !args.isFromCache();
    }
    args.dataFromSocketProcess() = mDataSentToChildProcess;
  }

  // Propagate whether or not conversion should occur from the parent-side
  // channel to the child-side channel.  Then disable the parent-side
  // conversion so that it only occurs in the child.
  Unused << chan->GetApplyConversion(&args.applyConversion());
  chan->SetApplyConversion(false);

  // If we've already applied the conversion (as can happen if we installed
  // a multipart converted), then don't apply it again on the child.
  if (chan->HasAppliedConversion()) {
    args.applyConversion() = false;
  }

  chan->GetStatus(&args.channelStatus());

  // Keep the cache entry for future use when opening alternative streams.
  // It could be already released by nsHttpChannel at that time.
  nsCOMPtr<nsISupports> cacheEntry;

  if (httpChannelImpl) {
    httpChannelImpl->GetCacheToken(getter_AddRefs(cacheEntry));
    mCacheEntry = do_QueryInterface(cacheEntry);
    args.cacheEntryAvailable() = static_cast<bool>(mCacheEntry);

    httpChannelImpl->GetCacheKey(&args.cacheKey());
    httpChannelImpl->GetAlternativeDataType(args.altDataType());
  }

  args.altDataLength() = chan->GetAltDataLength();
  args.deliveringAltData() = chan->IsDeliveringAltData();

  UpdateAndSerializeSecurityInfo(args.securityInfoSerialization());

  chan->GetRedirectCount(&args.redirectCount());
  chan->GetHasHTTPSRR(&args.hasHTTPSRR());

  nsCOMPtr<nsILoadInfo> loadInfo = chan->LoadInfo();
  mozilla::ipc::LoadInfoToParentLoadInfoForwarder(loadInfo,
                                                  &args.loadInfoForwarder());

  nsHttpResponseHead* responseHead = chan->GetResponseHead();
  bool useResponseHead = !!responseHead;
  nsHttpResponseHead cleanedUpResponseHead;

  if (responseHead &&
      (responseHead->HasHeader(nsHttp::Set_Cookie) || multiPartID)) {
    cleanedUpResponseHead = *responseHead;
    cleanedUpResponseHead.ClearHeader(nsHttp::Set_Cookie);
    if (multiPartID) {
      nsCOMPtr<nsIChannel> multiPartChannel = do_QueryInterface(aRequest);
      // For the multipart channel, use the parsed subtype instead. Note that
      // `chan` is the underlying base channel of the multipart channel in this
      // case, which is different from `multiPartChannel`.
      MOZ_ASSERT(multiPartChannel);
      nsAutoCString contentType;
      multiPartChannel->GetContentType(contentType);
      cleanedUpResponseHead.SetContentType(contentType);
    }
    responseHead = &cleanedUpResponseHead;
  }

  if (!responseHead) {
    responseHead = &cleanedUpResponseHead;
  }

  chan->GetIsResolvedByTRR(&args.isResolvedByTRR());
  chan->GetAllRedirectsSameOrigin(&args.allRedirectsSameOrigin());
  chan->GetCrossOriginOpenerPolicy(&args.openerPolicy());
  args.selfAddr() = chan->GetSelfAddr();
  args.peerAddr() = chan->GetPeerAddr();
  args.timing() = GetTimingAttributes(mChannel);
  if (mOverrideReferrerInfo) {
    args.overrideReferrerInfo() = ToRefPtr(std::move(mOverrideReferrerInfo));
  }
  if (!mCookie.IsEmpty()) {
    args.cookie() = std::move(mCookie);
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

  rv = NS_OK;

  nsCOMPtr<nsICacheEntry> altDataSource;
  nsCOMPtr<nsICacheInfoChannel> cacheChannel =
      do_QueryInterface(static_cast<nsIChannel*>(mChannel.get()));
  if (cacheChannel) {
    for (const auto& pref : cacheChannel->PreferredAlternativeDataTypes()) {
      if (pref.type() == args.altDataType() &&
          pref.deliverAltData() ==
              nsICacheInfoChannel::PreferredAlternativeDataDeliveryType::
                  SERIALIZE) {
        altDataSource = mCacheEntry;
        break;
      }
    }
  }

  if (mIPCClosed ||
      !mBgParent->OnStartRequest(
          *responseHead, useResponseHead,
          cleanedUpRequest ? cleanedUpRequestHeaders : requestHead->Headers(),
          args, altDataSource)) {
    rv = NS_ERROR_UNEXPECTED;
  }
  requestHead->Exit();

  // Need to wait for the cookies/permissions to content process, which is sent
  // via PContent in AboutToLoadHttpFtpDocumentForChild. For multipart channel,
  // send only one time since the cookies/permissions are the same.
  if (NS_SUCCEEDED(rv) && args.shouldWaitForOnStartRequestSent() &&
      multiPartID.valueOr(0) == 0) {
    LOG(("HttpChannelParent::SendOnStartRequestSent\n"));
    Unused << SendOnStartRequestSent();
  }

  return rv;
}

NS_IMETHODIMP
HttpChannelParent::OnStopRequest(nsIRequest* aRequest, nsresult aStatusCode) {
  LOG(("HttpChannelParent::OnStopRequest: [this=%p aRequest=%p status=%" PRIx32
       "]\n",
       this, aRequest, static_cast<uint32_t>(aStatusCode)));
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<nsHttpChannel> httpChannelImpl = do_QueryObject(mChannel);
  if (httpChannelImpl) {
    httpChannelImpl->SetWarningReporter(nullptr);
  }

  nsHttpHeaderArray* responseTrailer = mChannel->GetResponseTrailers();

  nsTArray<ConsoleReportCollected> consoleReports;

  RefPtr<HttpBaseChannel> httpChannel = do_QueryObject(mChannel);
  if (httpChannel) {
    httpChannel->StealConsoleReports(consoleReports);
  }

  // Either IPC channel is closed or background channel
  // is ready to send OnStopRequest.
  MOZ_ASSERT(mIPCClosed || mBgParent);

  if (mDataSentToChildProcess) {
    if (mIPCClosed || !mBgParent ||
        !mBgParent->OnConsoleReport(consoleReports)) {
      return NS_ERROR_UNEXPECTED;
    }
    return NS_OK;
  }

  // If we're handling a multi-part stream, then send this directly
  // over PHttpChannel to make synchronization easier.
  if (mIPCClosed || !mBgParent ||
      !mBgParent->OnStopRequest(
          aStatusCode, GetTimingAttributes(mChannel),
          responseTrailer ? *responseTrailer : nsHttpHeaderArray(),
          consoleReports)) {
    return NS_ERROR_UNEXPECTED;
  }

  if (NeedFlowControl()) {
    bool isLocal = false;
    NetAddr peerAddr = mChannel->GetPeerAddr();

#if defined(XP_UNIX)
    // Unix-domain sockets are always local.
    isLocal = (peerAddr.raw.family == PR_AF_LOCAL);
#endif

    isLocal = isLocal || peerAddr.IsLoopbackAddr();

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
// HttpChannelParent::nsIMultiPartChannelListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParent::OnAfterLastPart(nsresult aStatus) {
  LOG(("HttpChannelParent::OnAfterLastPart [this=%p]\n", this));
  MOZ_ASSERT(NS_IsMainThread());

  // If IPC channel is closed, there is nothing we can do. Just return NS_OK.
  if (mIPCClosed) {
    return NS_OK;
  }

  // If IPC channel is open, background channel should be ready to send
  // OnAfterLastPart.
  MOZ_ASSERT(mBgParent);

  if (!mBgParent || !mBgParent->OnAfterLastPart(aStatus)) {
    return NS_ERROR_UNEXPECTED;
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

  if (mDataSentToChildProcess) {
    uint32_t n;
    return aInputStream->ReadSegments(NS_DiscardSegment, nullptr, aCount, &n);
  }

  nsresult channelStatus = NS_OK;
  mChannel->GetStatus(&channelStatus);

  nsresult transportStatus = NS_NET_STATUS_RECEIVING_FROM;
  RefPtr<nsHttpChannel> httpChannelImpl = do_QueryObject(mChannel);
  if (httpChannelImpl) {
    if (httpChannelImpl->IsReadingFromCache()) {
      transportStatus = NS_NET_STATUS_READING;
    }
  }

  nsCString data;
  nsresult rv = NS_ReadInputStreamToString(aInputStream, data, aCount);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Either IPC channel is closed or background channel
  // is ready to send OnTransportAndData.
  MOZ_ASSERT(mIPCClosed || mBgParent);

  if (mIPCClosed || !mBgParent ||
      !mBgParent->OnTransportAndData(channelStatus, transportStatus, aOffset,
                                     aCount, data)) {
    return NS_ERROR_UNEXPECTED;
  }

  int32_t count = static_cast<int32_t>(aCount);

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
  // d. data will be sent from socket process to child process directly
  // Note that we served the cached resource first for partical cache, which is
  // ignored here since we only take the first ODA into consideration.
  if (gHttpHandler->SendWindowSize() == 0 || !httpChannelImpl ||
      httpChannelImpl->IsReadingFromCache() ||
      NS_FAILED(httpChannelImpl->GetContentLength(&contentLength)) ||
      contentLength < gHttpHandler->SendWindowSize() ||
      mDataSentToChildProcess) {
    mNeedFlowControl = false;
  }
  mCacheNeedFlowControlInitialized = true;
  return mNeedFlowControl;
}

mozilla::ipc::IPCResult HttpChannelParent::RecvBytesRead(
    const int32_t& aCount) {
  if (!NeedFlowControl()) {
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

//-----------------------------------------------------------------------------
// HttpChannelParent::nsIProgressEventSink
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParent::OnProgress(nsIRequest* aRequest, int64_t aProgress,
                              int64_t aProgressMax) {
  LOG(("HttpChannelParent::OnProgress [this=%p progress=%" PRId64 "max=%" PRId64
       "]\n",
       this, aProgress, aProgressMax));
  MOZ_ASSERT(NS_IsMainThread());

  // If IPC channel is closed, there is nothing we can do. Just return NS_OK.
  if (mIPCClosed) {
    return NS_OK;
  }

  // If it indicates this precedes OnDataAvailable, child can derive the value
  // in ODA.
  if (mIgnoreProgress) {
    mIgnoreProgress = false;
    return NS_OK;
  }

  // If IPC channel is open, background channel should be ready to send
  // OnProgress.
  MOZ_ASSERT(mBgParent);

  // Send OnProgress events to the child for data upload progress notifications
  // (i.e. status == NS_NET_STATUS_SENDING_TO) or if the channel has
  // LOAD_BACKGROUND set.
  if (!mBgParent || !mBgParent->OnProgress(aProgress, aProgressMax)) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

NS_IMETHODIMP
HttpChannelParent::OnStatus(nsIRequest* aRequest, nsresult aStatus,
                            const char16_t* aStatusArg) {
  LOG(("HttpChannelParent::OnStatus [this=%p status=%" PRIx32 "]\n", this,
       static_cast<uint32_t>(aStatus)));
  MOZ_ASSERT(NS_IsMainThread());

  // If IPC channel is closed, there is nothing we can do. Just return NS_OK.
  if (mIPCClosed) {
    return NS_OK;
  }

  // If this precedes OnDataAvailable, transportStatus will be derived in ODA.
  if (aStatus == NS_NET_STATUS_RECEIVING_FROM ||
      aStatus == NS_NET_STATUS_READING) {
    // The transport status and progress generated by ODA will be coalesced
    // into one IPC message. Therefore, we can ignore the next OnProgress event
    // since it is generated by ODA as well.
    mIgnoreProgress = true;
    return NS_OK;
  }

  // If IPC channel is open, background channel should be ready to send
  // OnStatus.
  MOZ_ASSERT(mIPCClosed || mBgParent);

  // Otherwise, send to child now
  if (!mBgParent || !mBgParent->OnStatus(aStatus)) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelParent::nsIParentChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParent::SetParentListener(ParentChannelListener* aListener) {
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

NS_IMETHODIMP
HttpChannelParent::GetRemoteType(nsACString& aRemoteType) {
  if (!CanSend()) {
    return NS_ERROR_UNEXPECTED;
  }

  dom::PContentParent* pcp = Manager()->Manager();
  aRemoteType = static_cast<dom::ContentParent*>(pcp)->GetRemoteType();
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpChannelParent::nsIParentRedirectingChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParent::StartRedirect(nsIChannel* newChannel, uint32_t redirectFlags,
                                 nsIAsyncVerifyRedirectCallback* callback) {
  nsresult rv;

  LOG(("HttpChannelParent::StartRedirect [this=%p, newChannel=%p callback=%p]",
       this, newChannel, callback));

  // Register the new channel and obtain id for it
  nsCOMPtr<nsIRedirectChannelRegistrar> registrar =
      RedirectChannelRegistrar::GetOrCreate();
  MOZ_ASSERT(registrar);

  mRedirectChannelId = nsContentUtils::GenerateLoadIdentifier();
  rv = registrar->RegisterChannel(newChannel, mRedirectChannelId);
  NS_ENSURE_SUCCESS(rv, rv);

  LOG(("Registered %p channel under id=%" PRIx64, newChannel,
       mRedirectChannelId));

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
      rv = NS_LinkRedirectChannels(mRedirectChannelId, this,
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
    result = SendRedirect1Begin(
        mRedirectChannelId, uriParams, newLoadFlags, redirectFlags,
        loadInfoForwarderArg, *responseHead, secInfoSerialization, channelId,
        mChannel->GetPeerAddr(), GetTimingAttributes(mChannel));
  }
  if (!result) {
    // Bug 621446 investigation
    mSentRedirect1BeginFailed = true;
    return NS_BINDING_ABORTED;
  }

  // Result is handled in RecvRedirect2Verify above

  mRedirectChannel = newChannel;
  mRedirectCallback = callback;
  return NS_OK;
}

NS_IMETHODIMP
HttpChannelParent::CompleteRedirect(bool succeeded) {
  LOG(("HttpChannelParent::CompleteRedirect [this=%p succeeded=%d]\n", this,
       succeeded));

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

//-----------------------------------------------------------------------------
// nsIChannelEventSink
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParent::AsyncOnChannelRedirect(
    nsIChannel* aOldChannel, nsIChannel* aNewChannel, uint32_t aRedirectFlags,
    nsIAsyncVerifyRedirectCallback* aCallback) {
  LOG(
      ("HttpChannelParent::AsyncOnChannelRedirect [this=%p, old=%p, "
       "new=%p, flags=%u]",
       this, aOldChannel, aNewChannel, aRedirectFlags));

  return StartRedirect(aNewChannel, aRedirectFlags, aCallback);
}

//-----------------------------------------------------------------------------
// nsIRedirectResultListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpChannelParent::OnRedirectResult(bool succeeded) {
  LOG(("HttpChannelParent::OnRedirectResult [this=%p, suc=%d]", this,
       succeeded));

  nsresult rv;

  nsCOMPtr<nsIParentChannel> redirectChannel;
  if (mRedirectChannelId) {
    nsCOMPtr<nsIRedirectChannelRegistrar> registrar =
        RedirectChannelRegistrar::GetOrCreate();
    MOZ_ASSERT(registrar);

    rv = registrar->GetParentChannel(mRedirectChannelId,
                                     getter_AddRefs(redirectChannel));
    if (NS_FAILED(rv) || !redirectChannel) {
      // Redirect might get canceled before we got AsyncOnChannelRedirect
      LOG(("Registered parent channel not found under id=%" PRIx64,
           mRedirectChannelId));

      nsCOMPtr<nsIChannel> newChannel;
      rv = registrar->GetRegisteredChannel(mRedirectChannelId,
                                           getter_AddRefs(newChannel));
      MOZ_ASSERT(newChannel, "Already registered channel not found");

      if (NS_SUCCEEDED(rv)) {
        newChannel->Cancel(NS_BINDING_ABORTED);
      }
    }

    // Release all previously registered channels, they are no longer need to be
    // kept in the registrar from this moment.
    registrar->DeregisterChannels(mRedirectChannelId);

    mRedirectChannelId = 0;
  }

  if (!redirectChannel) {
    succeeded = false;
  }

  CompleteRedirect(succeeded);

  if (succeeded) {
    if (!SameCOMIdentity(redirectChannel,
                         static_cast<nsIParentRedirectingChannel*>(this))) {
      Delete();
      mParentListener->SetListenerAfterRedirect(redirectChannel);
      redirectChannel->SetParentListener(mParentListener);
    }
  } else if (redirectChannel) {
    // Delete the redirect target channel: continue using old channel
    redirectChannel->Delete();
  }

  return NS_OK;
}

void HttpChannelParent::OverrideReferrerInfoDuringBeginConnect(
    nsIReferrerInfo* aReferrerInfo) {
  MOZ_ASSERT(aReferrerInfo);
  MOZ_ASSERT(!mAfterOnStartRequestBegun);

  mOverrideReferrerInfo = aReferrerInfo;
}

auto HttpChannelParent::AttachStreamFilter(
    Endpoint<extensions::PStreamFilterParent>&& aParentEndpoint,
    Endpoint<extensions::PStreamFilterChild>&& aChildEndpoint)
    -> RefPtr<ChildEndpointPromise> {
  LOG(("HttpChannelParent::AttachStreamFilter [this=%p]", this));
  MOZ_ASSERT(!mAfterOnStartRequestBegun);

  if (mIPCClosed) {
    return ChildEndpointPromise::CreateAndReject(false, __func__);
  }

  // If IPC channel is open, background channel should be ready to send
  // SendAttachStreamFilter.
  MOZ_ASSERT(mBgParent);
  return InvokeAsync(mBgParent->GetBackgroundTarget(), mBgParent.get(),
                     __func__, &HttpBackgroundChannelParent::AttachStreamFilter,
                     std::move(aParentEndpoint), std::move(aChildEndpoint));
}

void HttpChannelParent::SetCookie(nsCString&& aCookie) {
  LOG(("HttpChannelParent::SetCookie [this=%p]", this));
  MOZ_ASSERT(!mAfterOnStartRequestBegun);
  MOZ_ASSERT(mCookie.IsEmpty());
  mCookie = std::move(aCookie);
}

}  // namespace net
}  // namespace mozilla
