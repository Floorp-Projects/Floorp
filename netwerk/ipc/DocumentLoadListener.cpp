/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocumentLoadListener.h"

#include "mozilla/AntiTrackingUtils.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/MozPromiseInlines.h"  // For MozPromise::FromDomPromise
#include "mozilla/StaticPrefs_extensions.h"
#include "mozilla/StaticPrefs_fission.h"
#include "mozilla/StaticPrefs_security.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/ClientChannelHelper.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentProcessManager.h"
#include "mozilla/dom/SessionHistoryEntry.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/net/CookieJarSettings.h"
#include "mozilla/net/HttpChannelParent.h"
#include "mozilla/net/RedirectChannelRegistrar.h"
#include "mozilla/net/UrlClassifierCommon.h"
#include "nsContentSecurityUtils.h"
#include "nsDocShell.h"
#include "nsDocShellLoadState.h"
#include "nsDocShellLoadTypes.h"
#include "nsExternalHelperAppService.h"
#include "nsHttpChannel.h"
#include "nsIHttpChannelInternal.h"
#include "nsIBrowser.h"
#include "nsIE10SUtils.h"
#include "nsIStreamConverterService.h"
#include "nsIViewSourceChannel.h"
#include "nsImportModule.h"
#include "nsMimeTypes.h"
#include "nsRedirectHistoryEntry.h"
#include "nsSandboxFlags.h"
#include "nsURILoader.h"
#include "nsWebNavigationInfo.h"

#ifdef ANDROID
#  include "mozilla/widget/nsWindow.h"
#endif /* ANDROID */

mozilla::LazyLogModule gDocumentChannelLog("DocumentChannel");
#define LOG(fmt) MOZ_LOG(gDocumentChannelLog, mozilla::LogLevel::Verbose, fmt)

using namespace mozilla::dom;

namespace mozilla {
namespace net {

static void SetNeedToAddURIVisit(nsIChannel* aChannel,
                                 bool aNeedToAddURIVisit) {
  nsCOMPtr<nsIWritablePropertyBag2> props(do_QueryInterface(aChannel));
  if (!props) {
    return;
  }

  props->SetPropertyAsBool(NS_LITERAL_STRING("docshell.needToAddURIVisit"),
                           aNeedToAddURIVisit);
}

/**
 * An extension to nsDocumentOpenInfo that we run in the parent process, so
 * that we can make the decision to retarget to content handlers or the external
 * helper app, before we make process switching decisions.
 *
 * This modifies the behaviour of nsDocumentOpenInfo so that it can do
 * retargeting, but doesn't do stream conversion (but confirms that we will be
 * able to do so later).
 *
 * We still run nsDocumentOpenInfo in the content process, but disable
 * retargeting, so that it can only apply stream conversion, and then send data
 * to the docshell.
 */
class ParentProcessDocumentOpenInfo final : public nsDocumentOpenInfo,
                                            public nsIMultiPartChannelListener {
 public:
  ParentProcessDocumentOpenInfo(ParentChannelListener* aListener,
                                uint32_t aFlags,
                                mozilla::dom::BrowsingContext* aBrowsingContext)
      : nsDocumentOpenInfo(aFlags, false),
        mBrowsingContext(aBrowsingContext),
        mListener(aListener) {
    LOG(("ParentProcessDocumentOpenInfo ctor [this=%p]", this));
  }

  NS_DECL_ISUPPORTS_INHERITED

  // The default content listener is always a docshell, so this manually
  // implements the same checks, and if it succeeds, uses the parent
  // channel listener so that we forward onto DocumentLoadListener.
  bool TryDefaultContentListener(nsIChannel* aChannel,
                                 const nsCString& aContentType) {
    uint32_t canHandle = nsWebNavigationInfo::IsTypeSupported(
        aContentType, mBrowsingContext->GetAllowPlugins());
    if (canHandle != nsIWebNavigationInfo::UNSUPPORTED) {
      m_targetStreamListener = mListener;
      nsLoadFlags loadFlags = 0;
      aChannel->GetLoadFlags(&loadFlags);
      aChannel->SetLoadFlags(loadFlags | nsIChannel::LOAD_TARGETED);
      return true;
    }
    return false;
  }

  bool TryDefaultContentListener(nsIChannel* aChannel) override {
    return TryDefaultContentListener(aChannel, mContentType);
  }

  // Generally we only support stream converters that can tell
  // use exactly what type they'll output. If we find one, then
  // we just target to our default listener directly (without
  // conversion), and the content process nsDocumentOpenInfo will
  // run and do the actual conversion.
  nsresult TryStreamConversion(nsIChannel* aChannel) override {
    // The one exception is nsUnknownDecoder, which works in the parent
    // (and we need to know what the content type is before we can
    // decide if it will be handled in the parent), so we run that here.
    if (mContentType.LowerCaseEqualsASCII(UNKNOWN_CONTENT_TYPE)) {
      return nsDocumentOpenInfo::TryStreamConversion(aChannel);
    }

    nsresult rv;
    nsCOMPtr<nsIStreamConverterService> streamConvService =
        do_GetService(NS_STREAMCONVERTERSERVICE_CONTRACTID, &rv);
    nsAutoCString str;
    rv = streamConvService->ConvertedType(mContentType, aChannel, str);
    NS_ENSURE_SUCCESS(rv, rv);

    // We only support passing data to the default content listener
    // (docshell), and we don't supported chaining converters.
    if (TryDefaultContentListener(aChannel, str)) {
      mContentType = str;
      return NS_OK;
    }
    // This is the same result as nsStreamConverterService uses when it
    // can't find a converter
    return NS_ERROR_FAILURE;
  }

  nsresult TryExternalHelperApp(nsIExternalHelperAppService* aHelperAppService,
                                nsIChannel* aChannel) override {
    RefPtr<nsExternalAppHandler> handler;
    nsresult rv = aHelperAppService->CreateListener(
        mContentType, aChannel, mBrowsingContext, false, nullptr,
        getter_AddRefs(handler));
    if (NS_SUCCEEDED(rv)) {
      m_targetStreamListener = handler;
    }
    return rv;
  }

  nsDocumentOpenInfo* Clone() override {
    mCloned = true;
    return new ParentProcessDocumentOpenInfo(mListener, mFlags,
                                             mBrowsingContext);
  }

  NS_IMETHOD OnStartRequest(nsIRequest* request) override {
    LOG(("ParentProcessDocumentOpenInfo OnStartRequest [this=%p]", this));

    nsresult rv = nsDocumentOpenInfo::OnStartRequest(request);

    // If we didn't find a content handler,
    // and we don't have a listener, then just forward to our
    // default listener. This happens when the channel is in
    // an error state, and we want to just forward that on to be
    // handled in the content process.
    if (!mUsedContentHandler && !m_targetStreamListener) {
      m_targetStreamListener = mListener;
      return m_targetStreamListener->OnStartRequest(request);
    }
    if (m_targetStreamListener != mListener) {
      LOG(
          ("ParentProcessDocumentOpenInfo targeted to non-default listener "
           "[this=%p]",
           this));
      // If this is the only part, then we can immediately tell our listener
      // that it won't be getting any content and disconnect it. For multipart
      // channels we have to wait until we've handled all parts before we know.
      // This does mean that the content process can still Cancel() a multipart
      // response while the response is being handled externally, but this
      // matches the single-process behaviour.
      // If we got cloned, then we don't need to do this, as only the last link
      // needs to do it.
      // Multi-part channels are guaranteed to call OnAfterLastPart, which we
      // forward to the listeners, so it will handle disconnection at that
      // point.
      nsCOMPtr<nsIMultiPartChannel> multiPartChannel =
          do_QueryInterface(request);
      if (!multiPartChannel && !mCloned) {
        DisconnectChildListeners();
      }
    }
    return rv;
  }

  NS_IMETHOD OnAfterLastPart(nsresult aStatus) override {
    mListener->OnAfterLastPart(aStatus);
    return NS_OK;
  }

 private:
  virtual ~ParentProcessDocumentOpenInfo() {
    LOG(("ParentProcessDocumentOpenInfo dtor [this=%p]", this));
  }

  void DisconnectChildListeners() {
    // Tell the DocumentLoadListener to notify the content process that it's
    // been entirely retargeted, and to stop waiting.
    // Clear mListener's pointer to the DocumentLoadListener to break the
    // reference cycle.
    RefPtr<DocumentLoadListener> doc = do_GetInterface(ToSupports(mListener));
    MOZ_ASSERT(doc);
    doc->DisconnectChildListeners(NS_BINDING_RETARGETED, NS_OK);
    mListener->SetListenerAfterRedirect(nullptr);
  }

  RefPtr<mozilla::dom::BrowsingContext> mBrowsingContext;
  RefPtr<ParentChannelListener> mListener;

  /**
   * Set to true if we got cloned to create a chained listener.
   */
  bool mCloned = false;
};

NS_IMPL_ADDREF_INHERITED(ParentProcessDocumentOpenInfo, nsDocumentOpenInfo)
NS_IMPL_RELEASE_INHERITED(ParentProcessDocumentOpenInfo, nsDocumentOpenInfo)

NS_INTERFACE_MAP_BEGIN(ParentProcessDocumentOpenInfo)
  NS_INTERFACE_MAP_ENTRY(nsIMultiPartChannelListener)
NS_INTERFACE_MAP_END_INHERITING(nsDocumentOpenInfo)

NS_IMPL_ADDREF(DocumentLoadListener)
NS_IMPL_RELEASE(DocumentLoadListener)

NS_INTERFACE_MAP_BEGIN(DocumentLoadListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsIParentChannel)
  NS_INTERFACE_MAP_ENTRY(nsIAsyncVerifyRedirectReadyCallback)
  NS_INTERFACE_MAP_ENTRY(nsIChannelEventSink)
  NS_INTERFACE_MAP_ENTRY(nsIMultiPartChannelListener)
  NS_INTERFACE_MAP_ENTRY_CONCRETE(DocumentLoadListener)
NS_INTERFACE_MAP_END

DocumentLoadListener::DocumentLoadListener(
    CanonicalBrowsingContext* aBrowsingContext, ADocumentChannelBridge* aBridge)
    : mDocumentChannelBridge(aBridge) {
  MOZ_ASSERT(aBridge);
  LOG(("DocumentLoadListener ctor [this=%p]", this));
  mParentChannelListener = new ParentChannelListener(
      this, aBrowsingContext, aBrowsingContext->UsePrivateBrowsing());
}

DocumentLoadListener::DocumentLoadListener(
    CanonicalBrowsingContext* aBrowsingContext,
    base::ProcessId aPendingBridgeProcess) {
  LOG(("DocumentLoadListener ctor [this=%p]", this));
  mParentChannelListener = new ParentChannelListener(
      this, aBrowsingContext, aBrowsingContext->UsePrivateBrowsing());
  mPendingDocumentChannelBridgeProcess = Some(aPendingBridgeProcess);
}

DocumentLoadListener::~DocumentLoadListener() {
  LOG(("DocumentLoadListener dtor [this=%p]", this));
}

void DocumentLoadListener::AddURIVisit(nsIChannel* aChannel,
                                       uint32_t aLoadFlags) {
  if (mLoadStateLoadType == LOAD_ERROR_PAGE ||
      mLoadStateLoadType == LOAD_BYPASS_HISTORY) {
    return;
  }

  nsCOMPtr<nsIURI> uri;
  NS_GetFinalChannelURI(aChannel, getter_AddRefs(uri));

  nsCOMPtr<nsIURI> previousURI;
  uint32_t previousFlags = 0;
  if (mLoadStateLoadType & nsIDocShell::LOAD_CMD_RELOAD) {
    previousURI = uri;
  } else {
    nsDocShell::ExtractLastVisit(aChannel, getter_AddRefs(previousURI),
                                 &previousFlags);
  }

  // Get the HTTP response code, if available.
  uint32_t responseStatus = 0;
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel);
  if (httpChannel) {
    Unused << httpChannel->GetResponseStatus(&responseStatus);
  }

  RefPtr<CanonicalBrowsingContext> browsingContext =
      mParentChannelListener->GetBrowsingContext();
  nsCOMPtr<nsIWidget> widget =
      browsingContext->GetParentProcessWidgetContaining();

  nsDocShell::InternalAddURIVisit(uri, previousURI, previousFlags,
                                  responseStatus, browsingContext, widget,
                                  mLoadStateLoadType);
}

already_AddRefed<LoadInfo> DocumentLoadListener::CreateLoadInfo(
    CanonicalBrowsingContext* aBrowsingContext, nsDocShellLoadState* aLoadState,
    uint64_t aOuterWindowId) {
  // TODO: Block copied from nsDocShell::DoURILoad, refactor out somewhere
  bool inheritPrincipal = false;

  if (aLoadState->PrincipalToInherit()) {
    bool isSrcdoc =
        aLoadState->HasLoadFlags(nsDocShell::INTERNAL_LOAD_FLAGS_IS_SRCDOC);
    bool inheritAttrs = nsContentUtils::ChannelShouldInheritPrincipal(
        aLoadState->PrincipalToInherit(), aLoadState->URI(),
        true,  // aInheritForAboutBlank
        isSrcdoc);

    bool isURIUniqueOrigin =
        StaticPrefs::security_data_uri_unique_opaque_origin() &&
        SchemeIsData(aLoadState->URI());
    inheritPrincipal = inheritAttrs && !isURIUniqueOrigin;
  }

  nsSecurityFlags securityFlags =
      nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL;
  uint32_t sandboxFlags = aBrowsingContext->GetSandboxFlags();

  if (aLoadState->LoadType() == LOAD_ERROR_PAGE) {
    securityFlags |= nsILoadInfo::SEC_LOAD_ERROR_PAGE;
  }

  if (inheritPrincipal) {
    securityFlags |= nsILoadInfo::SEC_FORCE_INHERIT_PRINCIPAL;
  }

  RefPtr<LoadInfo> loadInfo;
  if (aBrowsingContext->GetParent()) {
    // Build LoadInfo for TYPE_SUBDOCUMENT
    loadInfo = new LoadInfo(aBrowsingContext, aLoadState->TriggeringPrincipal(),
                            aOuterWindowId, securityFlags, sandboxFlags);
  } else {
    // Build LoadInfo for TYPE_DOCUMENT
    OriginAttributes attrs;
    aBrowsingContext->GetOriginAttributes(attrs);
    loadInfo = new LoadInfo(aBrowsingContext, aLoadState->TriggeringPrincipal(),
                            attrs, aOuterWindowId, securityFlags, sandboxFlags);
  }

  loadInfo->SetHasValidUserGestureActivation(
      aLoadState->HasValidUserGestureActivation());

  return loadInfo.forget();
}

CanonicalBrowsingContext* DocumentLoadListener::GetBrowsingContext() {
  if (!mParentChannelListener) {
    return nullptr;
  }
  return mParentChannelListener->GetBrowsingContext();
}

bool DocumentLoadListener::Open(
    nsDocShellLoadState* aLoadState, uint32_t aCacheKey,
    const Maybe<uint64_t>& aChannelId, const TimeStamp& aAsyncOpenTime,
    nsDOMNavigationTiming* aTiming, Maybe<ClientInfo>&& aInfo,
    uint64_t aOuterWindowId, bool aHasGesture, Maybe<bool> aUriModified,
    Maybe<bool> aIsXFOError, nsresult* aRv) {
  LOG(("DocumentLoadListener Open [this=%p, uri=%s]", this,
       aLoadState->URI()->GetSpecOrDefault().get()));
  RefPtr<CanonicalBrowsingContext> browsingContext =
      mParentChannelListener->GetBrowsingContext();

  OriginAttributes attrs;
  browsingContext->GetOriginAttributes(attrs);

  MOZ_DIAGNOSTIC_ASSERT_IF(browsingContext->GetParent(),
                           browsingContext->GetParentWindowContext());

  // If this is a top-level load, then rebuild the LoadInfo from scratch,
  // since the goal is to be able to initiate loads in the parent, where the
  // content process won't have provided us with an existing one.
  RefPtr<LoadInfo> loadInfo =
      CreateLoadInfo(browsingContext, aLoadState, aOuterWindowId);

  nsLoadFlags loadFlags = aLoadState->CalculateChannelLoadFlags(
      browsingContext, std::move(aUriModified), std::move(aIsXFOError));

  if (!nsDocShell::CreateAndConfigureRealChannelForLoadState(
          browsingContext, aLoadState, loadInfo, mParentChannelListener,
          nullptr, attrs, loadFlags, aCacheKey, *aRv,
          getter_AddRefs(mChannel))) {
    mParentChannelListener = nullptr;
    return false;
  }

  nsCOMPtr<nsIURI> uriBeingLoaded =
      AntiTrackingUtils::MaybeGetDocumentURIBeingLoaded(mChannel);

  RefPtr<HttpBaseChannel> httpBaseChannel = do_QueryObject(mChannel, aRv);
  if (httpBaseChannel) {
    nsCOMPtr<nsIURI> topWindowURI;
    if (browsingContext->IsTop()) {
      // If this is for the top level loading, the top window URI should be the
      // URI which we are loading.
      topWindowURI = uriBeingLoaded;
    } else if (RefPtr<WindowGlobalParent> topWindow = AntiTrackingUtils::
                   GetTopWindowExcludingExtensionAccessibleContentFrames(
                       browsingContext, uriBeingLoaded)) {
      nsCOMPtr<nsIPrincipal> topWindowPrincipal =
          topWindow->DocumentPrincipal();
      if (topWindowPrincipal && !topWindowPrincipal->GetIsNullPrincipal()) {
        auto* basePrin = BasePrincipal::Cast(topWindowPrincipal);
        basePrin->GetURI(getter_AddRefs(topWindowURI));
      }
    }
    httpBaseChannel->SetTopWindowURI(topWindowURI);
  }

  nsCOMPtr<nsIIdentChannel> identChannel = do_QueryInterface(mChannel);
  if (identChannel && aChannelId) {
    Unused << identChannel->SetChannelId(*aChannelId);
  }

  RefPtr<nsHttpChannel> httpChannelImpl = do_QueryObject(mChannel);
  if (httpChannelImpl) {
    httpChannelImpl->SetWarningReporter(this);
  }

  nsCOMPtr<nsITimedChannel> timedChannel = do_QueryInterface(mChannel);
  if (timedChannel) {
    timedChannel->SetAsyncOpen(aAsyncOpenTime);
  }

  if (nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(mChannel)) {
    Unused << httpChannel->SetRequestContextID(
        browsingContext->GetRequestContextId());
  }

  // nsViewSourceChannel normally replaces the nsIRequest passed to
  // OnStart/StopRequest with itself. We don't need this, and instead
  // we want the original request so that we get different ones for
  // each part of a multipart channel.
  nsCOMPtr<nsIViewSourceChannel> viewSourceChannel;
  if (OtherPid() && (viewSourceChannel = do_QueryInterface(mChannel))) {
    viewSourceChannel->SetReplaceRequest(false);
  }

  // Setup a ClientChannelHelper to watch for redirects, and copy
  // across any serviceworker related data between channels as needed.
  AddClientChannelHelperInParent(mChannel, std::move(aInfo));

  // Recalculate the openFlags, matching the logic in use in Content process.
  // NOTE: The only case not handled here to mirror Content process is
  // redirecting to re-use the channel.
  MOZ_ASSERT(!aLoadState->GetPendingRedirectedChannel());
  uint32_t openFlags = nsDocShell::ComputeURILoaderFlags(
      browsingContext, aLoadState->LoadType());

  RefPtr<ParentProcessDocumentOpenInfo> openInfo =
      new ParentProcessDocumentOpenInfo(mParentChannelListener, openFlags,
                                        browsingContext);
  openInfo->Prepare();

#ifdef ANDROID
  RefPtr<MozPromise<bool, bool, false>> promise;
  if (aLoadState->LoadType() != LOAD_ERROR_PAGE &&
      !(aLoadState->HasLoadFlags(
          nsDocShell::INTERNAL_LOAD_FLAGS_BYPASS_LOAD_URI_DELEGATE)) &&
      !(aLoadState->LoadType() & LOAD_HISTORY)) {
    nsCOMPtr<nsIWidget> widget =
        browsingContext->GetParentProcessWidgetContaining();
    RefPtr<nsWindow> window = nsWindow::From(widget);

    if (window) {
      promise = window->OnLoadRequest(
          aLoadState->URI(), nsIBrowserDOMWindow::OPEN_CURRENTWINDOW,
          aLoadState->LoadFlags(), aLoadState->TriggeringPrincipal(),
          aHasGesture, browsingContext->IsTopContent());
    }
  }

  if (promise) {
    RefPtr<DocumentLoadListener> self = this;
    promise->Then(
        GetCurrentThreadSerialEventTarget(), __func__,
        [=](const MozPromise<bool, bool, false>::ResolveOrRejectValue& aValue) {
          if (aValue.IsResolve()) {
            bool handled = aValue.ResolveValue();
            if (handled) {
              self->DisconnectChildListeners(NS_ERROR_ABORT, NS_ERROR_ABORT);
              mParentChannelListener = nullptr;
            } else {
              nsresult rv = mChannel->AsyncOpen(openInfo);
              if (NS_FAILED(rv)) {
                self->DisconnectChildListeners(NS_ERROR_ABORT, NS_ERROR_ABORT);
                mParentChannelListener = nullptr;
              }
            }
          }
        });
  } else
#endif /* ANDROID */
  {
    *aRv = mChannel->AsyncOpen(openInfo);
    if (NS_FAILED(*aRv)) {
      mParentChannelListener = nullptr;
      return false;
    }
  }

  mChannelCreationURI = aLoadState->URI();
  mLoadStateLoadFlags = aLoadState->LoadFlags();
  mLoadStateLoadType = aLoadState->LoadType();
  mTiming = aTiming;
  mSrcdocData = aLoadState->SrcdocData();
  mBaseURI = aLoadState->BaseURI();
  if (StaticPrefs::fission_sessionHistoryInParent() &&
      browsingContext->GetSessionHistory()) {
    mSessionHistoryInfo =
        browsingContext->CreateSessionHistoryEntryForLoad(aLoadState, mChannel);
  }

  if (auto* ctx = GetBrowsingContext()) {
    ctx->StartDocumentLoad(this);
  }
  return true;
}

/* static */
bool DocumentLoadListener::OpenFromParent(
    dom::CanonicalBrowsingContext* aBrowsingContext,
    nsDocShellLoadState* aLoadState, uint64_t aOuterWindowId,
    uint32_t* aOutIdent) {
  LOG(("DocumentLoadListener::OpenFromParent"));

  // We currently only support passing nullptr for aLoadInfo for
  // top level browsing contexts.
  if (!aBrowsingContext->IsTopContent() ||
      !aBrowsingContext->GetContentParent()) {
    LOG(("DocumentLoadListener::OpenFromParent failed because of subdoc"));
    return false;
  }

  if (nsCOMPtr<nsIContentSecurityPolicy> csp = aLoadState->Csp()) {
    // Check CSP navigate-to
    bool allowsNavigateTo = false;
    nsresult rv = csp->GetAllowsNavigateTo(aLoadState->URI(),
                                           aLoadState->IsFormSubmission(),
                                           false, /* aWasRedirected */
                                           false, /* aEnforceWhitelist */
                                           &allowsNavigateTo);
    if (NS_FAILED(rv) || !allowsNavigateTo) {
      return false;
    }
  }

  // Any sort of reload/history load would need the cacheKey, and session
  // history data for load flags. We don't have those available in the parent
  // yet, so don't support these load types.
  auto loadType = aLoadState->LoadType();
  if (loadType == LOAD_HISTORY || loadType == LOAD_RELOAD_NORMAL ||
      loadType == LOAD_RELOAD_CHARSET_CHANGE ||
      loadType == LOAD_RELOAD_CHARSET_CHANGE_BYPASS_CACHE ||
      loadType == LOAD_RELOAD_CHARSET_CHANGE_BYPASS_PROXY_AND_CACHE) {
    LOG(
        ("DocumentLoadListener::OpenFromParent failed because of history "
         "load"));
    return false;
  }

  // Clone because this mutates the load flags in the load state, which
  // breaks nsDocShells expectations of being able to do it.
  RefPtr<nsDocShellLoadState> loadState = new nsDocShellLoadState(*aLoadState);
  loadState->CalculateLoadURIFlags();

  RefPtr<nsDOMNavigationTiming> timing = new nsDOMNavigationTiming(nullptr);
  timing->NotifyNavigationStart(
      aBrowsingContext->GetIsActive()
          ? nsDOMNavigationTiming::DocShellState::eActive
          : nsDOMNavigationTiming::DocShellState::eInactive);

  // We're not a history load or a reload, so we don't need this.
  uint32_t cacheKey = 0;

  // Loads start in the content process might have exposed a channel id to
  // observers, so we need to preserve the value in the parent. That can't have
  // happened here, so Nothing() is fine.
  Maybe<uint64_t> channelId = Nothing();

  // Initial client info is only relevant for subdocument loads, which we're
  // not supporting yet.
  Maybe<dom::ClientInfo> initialClientInfo;

  RefPtr<DocumentLoadListener> listener = new DocumentLoadListener(
      aBrowsingContext, aBrowsingContext->GetContentParent()->OtherPid());

  nsresult rv;
  bool result =
      listener->Open(loadState, cacheKey, channelId, TimeStamp::Now(), timing,
                     std::move(initialClientInfo), aOuterWindowId, false,
                     Nothing(), Nothing(), &rv);
  if (result) {
    // Create an entry in the redirect channel registrar to
    // allocate an identifier for this load.
    nsCOMPtr<nsIRedirectChannelRegistrar> registrar =
        RedirectChannelRegistrar::GetOrCreate();
    rv = registrar->RegisterChannel(nullptr, aOutIdent);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    // Register listener (as an nsIParentChannel) under our new identifier.
    rv = registrar->LinkChannels(*aOutIdent, listener, nullptr);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }
  return result;
}

void DocumentLoadListener::CleanupParentLoadAttempt(uint32_t aLoadIdent) {
  nsCOMPtr<nsIRedirectChannelRegistrar> registrar =
      RedirectChannelRegistrar::GetOrCreate();

  nsCOMPtr<nsIParentChannel> parentChannel;
  registrar->GetParentChannel(aLoadIdent, getter_AddRefs(parentChannel));
  RefPtr<DocumentLoadListener> loadListener = do_QueryObject(parentChannel);

  if (loadListener) {
    // If the load listener is still registered, then we must have failed
    // to connect DocumentChannel into it. Better cancel it!
    loadListener->NotifyBridgeFailed();
  }

  registrar->DeregisterChannels(aLoadIdent);
}

already_AddRefed<DocumentLoadListener> DocumentLoadListener::ClaimParentLoad(
    uint32_t aLoadIdent, ADocumentChannelBridge* aBridge) {
  nsCOMPtr<nsIRedirectChannelRegistrar> registrar =
      RedirectChannelRegistrar::GetOrCreate();

  nsCOMPtr<nsIParentChannel> parentChannel;
  registrar->GetParentChannel(aLoadIdent, getter_AddRefs(parentChannel));
  RefPtr<DocumentLoadListener> loadListener = do_QueryObject(parentChannel);
  registrar->DeregisterChannels(aLoadIdent);

  MOZ_ASSERT(loadListener);
  if (loadListener) {
    loadListener->NotifyBridgeConnected(aBridge);
  }
  return loadListener.forget();
}

void DocumentLoadListener::NotifyBridgeConnected(
    ADocumentChannelBridge* aBridge) {
  LOG(("DocumentLoadListener NotifyBridgeConnected [this=%p]", this));
  MOZ_ASSERT(!mDocumentChannelBridge);
  MOZ_ASSERT(mPendingDocumentChannelBridgeProcess);
  MOZ_ASSERT(aBridge->OtherPid() == *mPendingDocumentChannelBridgeProcess);

  mDocumentChannelBridge = aBridge;
  mPendingDocumentChannelBridgeProcess.reset();
  mBridgePromise.ResolveIfExists(aBridge, __func__);
}

void DocumentLoadListener::NotifyBridgeFailed() {
  LOG(("DocumentLoadListener NotifyBridgeFailed [this=%p]", this));
  MOZ_ASSERT(!mDocumentChannelBridge);
  MOZ_ASSERT(mPendingDocumentChannelBridgeProcess);
  mPendingDocumentChannelBridgeProcess.reset();

  Cancel(NS_BINDING_ABORTED);

  mBridgePromise.RejectIfExists(false, __func__);
}

void DocumentLoadListener::DocumentChannelBridgeDisconnected() {
  LOG(("DocumentLoadListener DocumentChannelBridgeDisconnected [this=%p]",
       this));
  // The nsHttpChannel may have a reference to this parent, release it
  // to avoid circular references.
  RefPtr<nsHttpChannel> httpChannelImpl = do_QueryObject(mChannel);
  if (httpChannelImpl) {
    httpChannelImpl->SetWarningReporter(nullptr);
  }
  mDocumentChannelBridge = nullptr;

  if (auto* ctx = GetBrowsingContext()) {
    ctx->EndDocumentLoad(this);
  }
}

void DocumentLoadListener::Cancel(const nsresult& aStatusCode) {
  LOG(
      ("DocumentLoadListener Cancel [this=%p, "
       "aStatusCode=%" PRIx32 " ]",
       this, static_cast<uint32_t>(aStatusCode)));
  mCancelled = true;

  if (mDoingProcessSwitch) {
    // If we've already initiated process-switching
    // then we can no longer be cancelled and we'll
    // disconnect the old listeners when done.
    return;
  }

  if (mChannel) {
    mChannel->Cancel(aStatusCode);
  }

  DisconnectChildListeners(aStatusCode, aStatusCode);
}

void DocumentLoadListener::DisconnectChildListeners(nsresult aStatus,
                                                    nsresult aLoadGroupStatus) {
  LOG(
      ("DocumentLoadListener DisconnectChildListener [this=%p, "
       "aStatus=%" PRIx32 " aLoadGroupStatus=%" PRIx32 " ]",
       this, static_cast<uint32_t>(aStatus),
       static_cast<uint32_t>(aLoadGroupStatus)));
  RefPtr<DocumentLoadListener> keepAlive(this);
  if (mDocumentChannelBridge) {
    // This will drop the bridge's reference to us, so we use keepAlive to
    // make sure we don't get deleted until we exit the function.
    mDocumentChannelBridge->DisconnectChildListeners(aStatus, aLoadGroupStatus);
  } else if (mPendingDocumentChannelBridgeProcess) {
    EnsureBridge()->Then(
        GetCurrentThreadSerialEventTarget(), __func__,
        [keepAlive, aStatus,
         aLoadGroupStatus](ADocumentChannelBridge* aBridge) {
          aBridge->DisconnectChildListeners(aStatus, aLoadGroupStatus);
          keepAlive->mDocumentChannelBridge = nullptr;
        },
        [](bool aDummy) {});
  }
  DocumentChannelBridgeDisconnected();

  // If we're not going to send anything else to the content process, and
  // we haven't yet consumed a stream filter promise, then we're never going
  // to.
  // TODO: This might be because we retargeted the stream to the download
  // handler or similar. Do we need to attach a stream filter to that?
  mStreamFilterRequests.Clear();
}

void DocumentLoadListener::RedirectToRealChannelFinished(nsresult aRv) {
  LOG(
      ("DocumentLoadListener RedirectToRealChannelFinished [this=%p, "
       "aRv=%" PRIx32 " ]",
       this, static_cast<uint32_t>(aRv)));
  if (NS_FAILED(aRv) || !mRedirectChannelId) {
    FinishReplacementChannelSetup(aRv);
    return;
  }

  // Wait for background channel ready on target channel
  nsCOMPtr<nsIRedirectChannelRegistrar> redirectReg =
      RedirectChannelRegistrar::GetOrCreate();
  MOZ_ASSERT(redirectReg);

  nsCOMPtr<nsIParentChannel> redirectParentChannel;
  redirectReg->GetParentChannel(mRedirectChannelId,
                                getter_AddRefs(redirectParentChannel));
  if (!redirectParentChannel) {
    FinishReplacementChannelSetup(NS_ERROR_FAILURE);
    return;
  }

  nsCOMPtr<nsIParentRedirectingChannel> redirectingParent =
      do_QueryInterface(redirectParentChannel);
  if (!redirectingParent) {
    // Continue verification procedure if redirecting to non-Http protocol
    FinishReplacementChannelSetup(NS_OK);
    return;
  }

  // Ask redirected channel if verification can proceed.
  // ReadyToVerify will be invoked when redirected channel is ready.
  redirectingParent->ContinueVerification(this);
}

NS_IMETHODIMP
DocumentLoadListener::ReadyToVerify(nsresult aResultCode) {
  FinishReplacementChannelSetup(aResultCode);
  return NS_OK;
}

void DocumentLoadListener::FinishReplacementChannelSetup(nsresult aResult) {
  LOG(
      ("DocumentLoadListener FinishReplacementChannelSetup [this=%p, "
       "aResult=%x]",
       this, int(aResult)));

  if (mDoingProcessSwitch) {
    DisconnectChildListeners(NS_BINDING_ABORTED, NS_BINDING_ABORTED);
  }

  if (!mRedirectChannelId) {
    if (NS_FAILED(aResult)) {
      mChannel->Cancel(aResult);
      mChannel->Resume();
      DisconnectChildListeners(aResult, aResult);
      return;
    }
    ApplyPendingFunctions(mChannel);
    // ResumeSuspendedChannel will be called later as RedirectToRealChannel
    // continues, so we can return early.
    return;
  }

  nsCOMPtr<nsIRedirectChannelRegistrar> registrar =
      RedirectChannelRegistrar::GetOrCreate();
  MOZ_ASSERT(registrar);

  nsCOMPtr<nsIParentChannel> redirectChannel;
  nsresult rv = registrar->GetParentChannel(mRedirectChannelId,
                                            getter_AddRefs(redirectChannel));
  if (NS_FAILED(rv) || !redirectChannel) {
    // Redirect might get canceled before we got AsyncOnChannelRedirect
    nsCOMPtr<nsIChannel> newChannel;
    rv = registrar->GetRegisteredChannel(mRedirectChannelId,
                                         getter_AddRefs(newChannel));
    MOZ_ASSERT(newChannel, "Already registered channel not found");

    if (NS_SUCCEEDED(rv)) {
      newChannel->Cancel(NS_ERROR_FAILURE);
    }
    if (!redirectChannel) {
      aResult = NS_ERROR_FAILURE;
    }
  }

  // Release all previously registered channels, they are no longer needed to
  // be kept in the registrar from this moment.
  registrar->DeregisterChannels(mRedirectChannelId);
  mRedirectChannelId = 0;
  if (NS_FAILED(aResult)) {
    if (redirectChannel) {
      redirectChannel->Delete();
    }
    mChannel->Cancel(aResult);
    mChannel->Resume();
    if (auto* ctx = GetBrowsingContext()) {
      ctx->EndDocumentLoad(this);
    }
    return;
  }

  MOZ_ASSERT(
      !SameCOMIdentity(redirectChannel, static_cast<nsIParentChannel*>(this)));

  Delete();
  redirectChannel->SetParentListener(mParentChannelListener);

  ApplyPendingFunctions(redirectChannel);

  ResumeSuspendedChannel(redirectChannel);
}

void DocumentLoadListener::ApplyPendingFunctions(nsISupports* aChannel) const {
  // We stored the values from all nsIParentChannel functions called since we
  // couldn't handle them. Copy them across to the real channel since it
  // should know what to do.

  nsCOMPtr<nsIParentChannel> parentChannel = do_QueryInterface(aChannel);
  if (parentChannel) {
    for (auto& variant : mIParentChannelFunctions) {
      variant.match(
          [parentChannel](const nsIHttpChannel::FlashPluginState& aState) {
            parentChannel->NotifyFlashPluginStateChanged(aState);
          },
          [parentChannel](const ClassifierMatchedInfoParams& aParams) {
            parentChannel->SetClassifierMatchedInfo(
                aParams.mList, aParams.mProvider, aParams.mFullHash);
          },
          [parentChannel](const ClassifierMatchedTrackingInfoParams& aParams) {
            parentChannel->SetClassifierMatchedTrackingInfo(
                aParams.mLists, aParams.mFullHashes);
          },
          [parentChannel](const ClassificationFlagsParams& aParams) {
            parentChannel->NotifyClassificationFlags(
                aParams.mClassificationFlags, aParams.mIsThirdParty);
          });
    }
  } else {
    for (auto& variant : mIParentChannelFunctions) {
      variant.match(
          [&](const nsIHttpChannel::FlashPluginState& aState) {
            // For now, only HttpChannel use this attribute.
            RefPtr<HttpBaseChannel> httpChannel = do_QueryObject(aChannel);
            if (httpChannel) {
              httpChannel->SetFlashPluginState(aState);
            }
          },
          [&](const ClassifierMatchedInfoParams& aParams) {
            nsCOMPtr<nsIClassifiedChannel> classifiedChannel =
                do_QueryInterface(aChannel);
            if (classifiedChannel) {
              classifiedChannel->SetMatchedInfo(
                  aParams.mList, aParams.mProvider, aParams.mFullHash);
            }
          },
          [&](const ClassifierMatchedTrackingInfoParams& aParams) {
            nsCOMPtr<nsIClassifiedChannel> classifiedChannel =
                do_QueryInterface(aChannel);
            if (classifiedChannel) {
              nsTArray<nsCString> lists, fullhashes;
              for (const nsACString& token : aParams.mLists.Split(',')) {
                lists.AppendElement(token);
              }
              for (const nsACString& token : aParams.mFullHashes.Split(',')) {
                fullhashes.AppendElement(token);
              }
              classifiedChannel->SetMatchedTrackingInfo(lists, fullhashes);
            }
          },
          [&](const ClassificationFlagsParams& aParams) {
            UrlClassifierCommon::SetClassificationFlagsHelper(
                static_cast<nsIChannel*>(aChannel),
                aParams.mClassificationFlags, aParams.mIsThirdParty);
          });
    }
  }

  RefPtr<HttpChannelSecurityWarningReporter> reporter;
  if (RefPtr<HttpChannelParent> httpParent = do_QueryObject(aChannel)) {
    reporter = httpParent;
  } else if (RefPtr<nsHttpChannel> httpChannel = do_QueryObject(aChannel)) {
    reporter = httpChannel->GetWarningReporter();
  }
  if (reporter) {
    for (auto& variant : mSecurityWarningFunctions) {
      variant.match(
          [reporter](const ReportSecurityMessageParams& aParams) {
            Unused << reporter->ReportSecurityMessage(aParams.mMessageTag,
                                                      aParams.mMessageCategory);
          },
          [reporter](const LogBlockedCORSRequestParams& aParams) {
            Unused << reporter->LogBlockedCORSRequest(aParams.mMessage,
                                                      aParams.mCategory);
          },
          [reporter](const LogMimeTypeMismatchParams& aParams) {
            Unused << reporter->LogMimeTypeMismatch(
                aParams.mMessageName, aParams.mWarning, aParams.mURL,
                aParams.mContentType);
          });
    }
  }
}

bool DocumentLoadListener::ResumeSuspendedChannel(
    nsIStreamListener* aListener) {
  LOG(("DocumentLoadListener ResumeSuspendedChannel [this=%p]", this));
  RefPtr<nsHttpChannel> httpChannel = do_QueryObject(mChannel);
  if (httpChannel) {
    httpChannel->SetApplyConversion(mOldApplyConversion);
  }

  if (!mIsFinished) {
    mParentChannelListener->SetListenerAfterRedirect(aListener);
  }

  // If we failed to suspend the channel, then we might have received
  // some messages while the redirected was being handled.
  // Manually send them on now.
  nsTArray<StreamListenerFunction> streamListenerFunctions =
      std::move(mStreamListenerFunctions);
  if (!aListener) {
    streamListenerFunctions.Clear();
  }
  nsresult rv = NS_OK;
  for (auto& variant : streamListenerFunctions) {
    variant.match(
        [&](const OnStartRequestParams& aParams) {
          rv = aListener->OnStartRequest(aParams.request);
          if (NS_FAILED(rv)) {
            aParams.request->Cancel(rv);
          }
        },
        [&](const OnDataAvailableParams& aParams) {
          // Don't deliver OnDataAvailable if we've
          // already failed.
          if (NS_FAILED(rv)) {
            return;
          }
          nsCOMPtr<nsIInputStream> stringStream;
          rv = NS_NewByteInputStream(
              getter_AddRefs(stringStream),
              Span<const char>(aParams.data.get(), aParams.count),
              NS_ASSIGNMENT_DEPEND);
          if (NS_SUCCEEDED(rv)) {
            rv = aListener->OnDataAvailable(aParams.request, stringStream,
                                            aParams.offset, aParams.count);
          }
          if (NS_FAILED(rv)) {
            aParams.request->Cancel(rv);
          }
        },
        [&](const OnStopRequestParams& aParams) {
          if (NS_SUCCEEDED(rv)) {
            aListener->OnStopRequest(aParams.request, aParams.status);
          } else {
            aListener->OnStopRequest(aParams.request, rv);
          }
          rv = NS_OK;
        },
        [&](const OnAfterLastPartParams& aParams) {
          nsCOMPtr<nsIMultiPartChannelListener> multiListener =
              do_QueryInterface(aListener);
          if (multiListener) {
            if (NS_SUCCEEDED(rv)) {
              multiListener->OnAfterLastPart(aParams.status);
            } else {
              multiListener->OnAfterLastPart(rv);
            }
          }
        });
  }
  // We don't expect to get new stream listener functions added
  // via re-entrancy. If this ever happens, we should understand
  // exactly why before allowing it.
  NS_ASSERTION(mStreamListenerFunctions.IsEmpty(),
               "Should not have added new stream listener function!");

  mChannel->Resume();

  if (auto* ctx = GetBrowsingContext()) {
    ctx->EndDocumentLoad(this);
  }

  return !mIsFinished;
}

void DocumentLoadListener::SerializeRedirectData(
    RedirectToRealChannelArgs& aArgs, bool aIsCrossProcess,
    uint32_t aRedirectFlags, uint32_t aLoadFlags,
    ContentParent* aParent) const {
  // Use the original URI of the current channel, as this is what
  // we'll use to construct the channel in the content process.
  aArgs.uri() = mChannelCreationURI;
  if (!aArgs.uri()) {
    mChannel->GetOriginalURI(getter_AddRefs(aArgs.uri()));
  }

  // I previously used HttpBaseChannel::CloneLoadInfoForRedirect, but that
  // clears the principal to inherit, which fails tests (probably because this
  // 'redirect' is usually just an implementation detail). It's also http
  // only, and mChannel can be anything that we redirected to.
  nsCOMPtr<nsILoadInfo> channelLoadInfo;
  mChannel->GetLoadInfo(getter_AddRefs(channelLoadInfo));

  nsCOMPtr<nsIPrincipal> principalToInherit;
  channelLoadInfo->GetPrincipalToInherit(getter_AddRefs(principalToInherit));

  const RefPtr<nsHttpChannel> baseChannel = do_QueryObject(mChannel.get());

  nsCOMPtr<nsILoadContext> loadContext;
  NS_QueryNotificationCallbacks(mChannel, loadContext);
  nsCOMPtr<nsILoadInfo> redirectLoadInfo;

  // Only use CloneLoadInfoForRedirect if we have a load context,
  // since it internally tries to pull OriginAttributes from the
  // the load context and asserts if they don't match the load info.
  // We can end up without a load context if the channel has been aborted
  // and the callbacks have been cleared.
  if (baseChannel && loadContext) {
    redirectLoadInfo = baseChannel->CloneLoadInfoForRedirect(
        aArgs.uri(), nsIChannelEventSink::REDIRECT_INTERNAL);
    redirectLoadInfo->SetResultPrincipalURI(aArgs.uri());

    // The clone process clears this, and then we fail tests..
    // docshell/test/mochitest/test_forceinheritprincipal_overrule_owner.html
    if (principalToInherit) {
      redirectLoadInfo->SetPrincipalToInherit(principalToInherit);
    }
  } else {
    redirectLoadInfo =
        static_cast<mozilla::net::LoadInfo*>(channelLoadInfo.get())->Clone();

    nsCOMPtr<nsIPrincipal> uriPrincipal;
    nsIScriptSecurityManager* sm = nsContentUtils::GetSecurityManager();
    sm->GetChannelURIPrincipal(mChannel, getter_AddRefs(uriPrincipal));

    nsCOMPtr<nsIRedirectHistoryEntry> entry =
        new nsRedirectHistoryEntry(uriPrincipal, nullptr, EmptyCString());

    redirectLoadInfo->AppendRedirectHistoryEntry(entry, true);
  }

  const Maybe<ClientInfo>& reservedClientInfo =
      channelLoadInfo->GetReservedClientInfo();
  if (reservedClientInfo) {
    redirectLoadInfo->SetReservedClientInfo(*reservedClientInfo);
  }

  aArgs.registrarId() = mRedirectChannelId;

  MOZ_ALWAYS_SUCCEEDS(
      ipc::LoadInfoToLoadInfoArgs(redirectLoadInfo, &aArgs.loadInfo()));

  mChannel->GetOriginalURI(getter_AddRefs(aArgs.originalURI()));

  // mChannel can be a nsHttpChannel as well as InterceptedHttpChannel so we
  // can't use baseChannel here.
  if (nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(mChannel)) {
    MOZ_ALWAYS_SUCCEEDS(httpChannel->GetChannelId(&aArgs.channelId()));
  }

  aArgs.redirectMode() = nsIHttpChannelInternal::REDIRECT_MODE_FOLLOW;
  nsCOMPtr<nsIHttpChannelInternal> httpChannelInternal =
      do_QueryInterface(mChannel);
  if (httpChannelInternal) {
    MOZ_ALWAYS_SUCCEEDS(
        httpChannelInternal->GetRedirectMode(&aArgs.redirectMode()));
  }

  if (baseChannel) {
    aArgs.init() =
        Some(baseChannel
                 ->CloneReplacementChannelConfig(
                     true, aRedirectFlags,
                     HttpBaseChannel::ReplacementReason::DocumentChannel)
                 .Serialize(aParent));
  }

  uint32_t contentDispositionTemp;
  nsresult rv = mChannel->GetContentDisposition(&contentDispositionTemp);
  if (NS_SUCCEEDED(rv)) {
    aArgs.contentDisposition() = Some(contentDispositionTemp);
  }

  nsString contentDispositionFilenameTemp;
  rv = mChannel->GetContentDispositionFilename(contentDispositionFilenameTemp);
  if (NS_SUCCEEDED(rv)) {
    aArgs.contentDispositionFilename() = Some(contentDispositionFilenameTemp);
  }

  SetNeedToAddURIVisit(mChannel, false);

  aArgs.newLoadFlags() = aLoadFlags;
  aArgs.redirectFlags() = aRedirectFlags;
  aArgs.redirectIdentifier() = mCrossProcessRedirectIdentifier;
  aArgs.properties() = do_QueryObject(mChannel.get());
  aArgs.srcdocData() = mSrcdocData;
  aArgs.baseUri() = mBaseURI;
  aArgs.loadStateLoadFlags() = mLoadStateLoadFlags;
  aArgs.loadStateLoadType() = mLoadStateLoadType;
  if (mSessionHistoryInfo) {
    aArgs.sessionHistoryInfo().emplace(
        mSessionHistoryInfo->mId, MakeUnique<mozilla::dom::SessionHistoryInfo>(
                                      *mSessionHistoryInfo->mInfo));
  }
}

bool DocumentLoadListener::MaybeTriggerProcessSwitch(
    bool* aWillSwitchToRemote) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_DIAGNOSTIC_ASSERT(!mDoingProcessSwitch,
                        "Already in the middle of switching?");
  MOZ_DIAGNOSTIC_ASSERT(mChannel);
  MOZ_DIAGNOSTIC_ASSERT(mParentChannelListener);
  MOZ_DIAGNOSTIC_ASSERT(aWillSwitchToRemote);

  LOG(("DocumentLoadListener MaybeTriggerProcessSwitch [this=%p]", this));

  // Get the BrowsingContext which will be switching processes.
  RefPtr<CanonicalBrowsingContext> browsingContext =
      mParentChannelListener->GetBrowsingContext();
  if (NS_WARN_IF(!browsingContext)) {
    LOG(("Process Switch Abort: no browsing context"));
    return false;
  }
  if (!browsingContext->IsContent()) {
    LOG(("Process Switch Abort: non-content browsing context"));
    return false;
  }
  if (browsingContext->GetParent() && !browsingContext->UseRemoteSubframes()) {
    LOG(("Process Switch Abort: remote subframes disabled"));
    return false;
  }

  if (browsingContext->GetParentWindowContext() &&
      browsingContext->GetParentWindowContext()->IsInProcess()) {
    LOG(("Process Switch Abort: Subframe with in-process parent"));
    return false;
  }

  // We currently can't switch processes for toplevel loads unless they're
  // loaded within a browser tab.
  // FIXME: Ideally we won't do this in the future.
  nsCOMPtr<nsIBrowser> browser;
  bool isPreloadSwitch = false;
  if (!browsingContext->GetParent()) {
    Element* browserElement = browsingContext->GetEmbedderElement();
    if (!browserElement) {
      LOG(("Process Switch Abort: cannot get browser element"));
      return false;
    }
    browser = browserElement->AsBrowser();
    if (!browser) {
      LOG(("Process Switch Abort: not loaded within nsIBrowser"));
      return false;
    }
    bool loadedInTab = false;
    if (NS_FAILED(browser->GetCanPerformProcessSwitch(&loadedInTab)) ||
        !loadedInTab) {
      LOG(("Process Switch Abort: browser is not loaded in a tab"));
      return false;
    }

    // Leaving about:newtab from a used to be preloaded browser should run the
    // process selecting algorithm again.
    nsAutoString isPreloadBrowserStr;
    if (browserElement->GetAttr(kNameSpaceID_None, nsGkAtoms::preloadedState,
                                isPreloadBrowserStr)) {
      if (isPreloadBrowserStr.EqualsLiteral("consumed")) {
        nsCOMPtr<nsIURI> originalURI;
        if (NS_SUCCEEDED(
                mChannel->GetOriginalURI(getter_AddRefs(originalURI)))) {
          if (!originalURI->GetSpecOrDefault().EqualsLiteral("about:newtab")) {
            LOG(("Process Switch: leaving preloaded browser"));
            isPreloadSwitch = true;
            browserElement->UnsetAttr(kNameSpaceID_None,
                                      nsGkAtoms::preloadedState, true);
          }
        }
      }
    }
  }

  // Get information about the current document loaded in our BrowsingContext.
  nsCOMPtr<nsIPrincipal> currentPrincipal;
  if (RefPtr<WindowGlobalParent> wgp =
          browsingContext->GetCurrentWindowGlobal()) {
    currentPrincipal = wgp->DocumentPrincipal();
  }
  RefPtr<ContentParent> contentParent = browsingContext->GetContentParent();
  MOZ_ASSERT(!OtherPid() || contentParent,
             "Only PPDC is allowed to not have an existing ContentParent");

  // Get the final principal, used to select which process to load into.
  nsCOMPtr<nsIPrincipal> resultPrincipal;
  nsresult rv = nsContentUtils::GetSecurityManager()->GetChannelResultPrincipal(
      mChannel, getter_AddRefs(resultPrincipal));
  if (NS_FAILED(rv)) {
    LOG(("Process Switch Abort: failed to get channel result principal"));
    return false;
  }

  // Determine our COOP status, which will be used to determine our preferred
  // remote type.
  bool isCOOPSwitch = HasCrossOriginOpenerPolicyMismatch();
  nsILoadInfo::CrossOriginOpenerPolicy coop =
      nsILoadInfo::OPENER_POLICY_UNSAFE_NONE;
  if (!browsingContext->IsTop()) {
    coop = browsingContext->Top()->GetOpenerPolicy();
  } else if (nsCOMPtr<nsIHttpChannelInternal> httpChannel =
                 do_QueryInterface(mChannel)) {
    MOZ_ALWAYS_SUCCEEDS(httpChannel->GetCrossOriginOpenerPolicy(&coop));
  }

  nsAutoString currentRemoteType;
  if (contentParent) {
    currentRemoteType = contentParent->GetRemoteType();
  } else {
    currentRemoteType = VoidString();
  }
  nsAutoString preferredRemoteType = currentRemoteType;
  if (coop ==
      nsILoadInfo::OPENER_POLICY_SAME_ORIGIN_EMBEDDER_POLICY_REQUIRE_CORP) {
    // We want documents with SAME_ORIGIN_EMBEDDER_POLICY_REQUIRE_CORP COOP
    // policy to be loaded in a separate process in which we can enable
    // high-resolution timers.
    nsAutoCString siteOrigin;
    resultPrincipal->GetSiteOrigin(siteOrigin);
    preferredRemoteType.Assign(
        NS_LITERAL_STRING(WITH_COOP_COEP_REMOTE_TYPE_PREFIX));
    preferredRemoteType.Append(NS_ConvertUTF8toUTF16(siteOrigin));
  } else if (isCOOPSwitch) {
    // If we're doing a COOP switch, we do not need any affinity to the current
    // remote type. Clear it back to the default value.
    preferredRemoteType.Assign(NS_LITERAL_STRING(DEFAULT_REMOTE_TYPE));
  }
  MOZ_DIAGNOSTIC_ASSERT(!contentParent || !preferredRemoteType.IsEmpty(),
                        "Unexpected empty remote type!");

  LOG(
      ("DocumentLoadListener GetRemoteTypeForPrincipal "
       "[this=%p, contentParent=%s, preferredRemoteType=%s]",
       this, NS_ConvertUTF16toUTF8(currentRemoteType).get(),
       NS_ConvertUTF16toUTF8(preferredRemoteType).get()));

  nsCOMPtr<nsIE10SUtils> e10sUtils =
      do_ImportModule("resource://gre/modules/E10SUtils.jsm", "E10SUtils");
  if (!e10sUtils) {
    LOG(("Process Switch Abort: Could not import E10SUtils"));
    return false;
  }

  nsAutoString remoteType;
  rv = e10sUtils->GetRemoteTypeForPrincipal(
      resultPrincipal, mChannelCreationURI, browsingContext->UseRemoteTabs(),
      browsingContext->UseRemoteSubframes(), preferredRemoteType,
      currentPrincipal, browsingContext->GetParent(), remoteType);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG(("Process Switch Abort: getRemoteTypeForPrincipal threw an exception"));
    return false;
  }

  LOG(("GetRemoteTypeForPrincipal -> current:%s remoteType:%s",
       NS_ConvertUTF16toUTF8(currentRemoteType).get(),
       NS_ConvertUTF16toUTF8(remoteType).get()));

  // Check if a process switch is needed.
  if (currentRemoteType == remoteType && !isCOOPSwitch && !isPreloadSwitch) {
    LOG(("Process Switch Abort: type (%s) is compatible",
         NS_ConvertUTF16toUTF8(remoteType).get()));
    return false;
  }
  if (NS_WARN_IF(remoteType.IsEmpty())) {
    LOG(("Process Switch Abort: non-remote target process"));
    return false;
  }

  *aWillSwitchToRemote = !remoteType.IsEmpty();

  LOG(("Process Switch: Changing Remoteness from '%s' to '%s'",
       NS_ConvertUTF16toUTF8(currentRemoteType).get(),
       NS_ConvertUTF16toUTF8(remoteType).get()));

  // XXX: This is super hacky, and we should be able to do something better.
  static uint64_t sNextCrossProcessRedirectIdentifier = 0;
  mCrossProcessRedirectIdentifier = ++sNextCrossProcessRedirectIdentifier;
  mDoingProcessSwitch = true;

  RefPtr<DocumentLoadListener> self = this;
  // At this point, we're actually going to perform a process switch, which
  // involves calling into other logic.
  if (browsingContext->GetParent()) {
    LOG(("Process Switch: Calling ChangeFrameRemoteness"));
    // If we're switching a subframe, ask BrowsingContext to do it for us.
    MOZ_ASSERT(!isCOOPSwitch);
    browsingContext
        ->ChangeFrameRemoteness(remoteType, mCrossProcessRedirectIdentifier)
        ->Then(
            GetMainThreadSerialEventTarget(), __func__,
            [self](BrowserParent* aBrowserParent) {
              MOZ_ASSERT(self->mChannel,
                         "Something went wrong, channel got cancelled");
              self->TriggerRedirectToRealChannel(
                  Some(aBrowserParent->Manager()->ChildID()));
            },
            [self](nsresult aStatusCode) {
              MOZ_ASSERT(NS_FAILED(aStatusCode), "Status should be error");
              self->RedirectToRealChannelFinished(aStatusCode);
            });
    return true;
  }

  LOG(("Process Switch: Calling nsIBrowser::PerformProcessSwitch"));
  // We're switching a toplevel BrowsingContext's process. This has to be done
  // using nsIBrowser.
  RefPtr<Promise> domPromise;
  browser->PerformProcessSwitch(remoteType, mCrossProcessRedirectIdentifier,
                                isCOOPSwitch, getter_AddRefs(domPromise));
  MOZ_DIAGNOSTIC_ASSERT(domPromise,
                        "PerformProcessSwitch didn't return a promise");

  MozPromise<uint64_t, nsresult, true>::FromDomPromise(domPromise)
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [self](uint64_t aCpId) {
            MOZ_ASSERT(self->mChannel,
                       "Something went wrong, channel got cancelled");
            self->TriggerRedirectToRealChannel(Some(aCpId));
          },
          [self](nsresult aStatusCode) {
            MOZ_ASSERT(NS_FAILED(aStatusCode), "Status should be error");
            self->RedirectToRealChannelFinished(aStatusCode);
          });
  return true;
}

auto DocumentLoadListener::EnsureBridge() -> RefPtr<EnsureBridgePromise> {
  MOZ_ASSERT(mDocumentChannelBridge || mPendingDocumentChannelBridgeProcess);
  if (mDocumentChannelBridge) {
    MOZ_ASSERT(mBridgePromise.IsEmpty());
    return EnsureBridgePromise::CreateAndResolve(mDocumentChannelBridge,
                                                 __func__);
  }

  return mBridgePromise.Ensure(__func__);
}

RefPtr<PDocumentChannelParent::RedirectToRealChannelPromise>
DocumentLoadListener::RedirectToRealChannel(
    uint32_t aRedirectFlags, uint32_t aLoadFlags,
    const Maybe<uint64_t>& aDestinationProcess,
    nsTArray<ParentEndpoint>&& aStreamFilterEndpoints) {
  LOG(
      ("DocumentLoadListener RedirectToRealChannel [this=%p] "
       "aRedirectFlags=%" PRIx32 ", aLoadFlags=%" PRIx32,
       this, aRedirectFlags, aLoadFlags));

  // TODO(djg): Add the last URI visit to history if success. Is there a better
  // place to handle this? Need access to the updated aLoadFlags.
  nsresult status = NS_OK;
  mChannel->GetStatus(&status);
  bool updateGHistory =
      nsDocShell::ShouldUpdateGlobalHistory(mLoadStateLoadType);
  if (NS_SUCCEEDED(status) && updateGHistory && !net::ChannelIsPost(mChannel)) {
    AddURIVisit(mChannel, aLoadFlags);
  }

  if (aDestinationProcess || OtherPid()) {
    // Register the new channel and obtain id for it
    nsCOMPtr<nsIRedirectChannelRegistrar> registrar =
        RedirectChannelRegistrar::GetOrCreate();
    MOZ_ASSERT(registrar);
    MOZ_ALWAYS_SUCCEEDS(
        registrar->RegisterChannel(mChannel, &mRedirectChannelId));
  }

  if (aDestinationProcess) {
    dom::ContentParent* cp =
        dom::ContentProcessManager::GetSingleton()->GetContentProcessById(
            ContentParentId{*aDestinationProcess});
    if (!cp) {
      return PDocumentChannelParent::RedirectToRealChannelPromise::
          CreateAndReject(ipc::ResponseRejectReason::SendError, __func__);
    }

    RedirectToRealChannelArgs args;
    SerializeRedirectData(args, !!aDestinationProcess, aRedirectFlags,
                          aLoadFlags, cp);
    if (mTiming) {
      mTiming->Anonymize(args.uri());
      args.timing() = Some(std::move(mTiming));
    }

    auto loadInfo = args.loadInfo();

    if (loadInfo.isNothing()) {
      return PDocumentChannelParent::RedirectToRealChannelPromise::
          CreateAndReject(ipc::ResponseRejectReason::SendError, __func__);
    }

    auto triggeringPrincipalOrErr =
        PrincipalInfoToPrincipal(loadInfo.ref().triggeringPrincipalInfo());

    if (triggeringPrincipalOrErr.isOk()) {
      nsCOMPtr<nsIPrincipal> triggeringPrincipal =
          triggeringPrincipalOrErr.unwrap();
      cp->TransmitBlobDataIfBlobURL(args.uri(), triggeringPrincipal);
    }

    return cp->SendCrossProcessRedirect(args,
                                        std::move(aStreamFilterEndpoints));
  }

  return EnsureBridge()->Then(
      GetCurrentThreadSerialEventTarget(), __func__,
      [self = RefPtr<DocumentLoadListener>(this),
       endpoints = std::move(aStreamFilterEndpoints), aRedirectFlags,
       aLoadFlags](ADocumentChannelBridge* aBridge) mutable {
        if (self->mCancelled) {
          return PDocumentChannelParent::RedirectToRealChannelPromise::
              CreateAndResolve(NS_BINDING_ABORTED, __func__);
        }
        return aBridge->RedirectToRealChannel(std::move(endpoints),
                                              aRedirectFlags, aLoadFlags);
      },
      [](bool aDummy) {
        return PDocumentChannelParent::RedirectToRealChannelPromise::
            CreateAndReject(ipc::ResponseRejectReason::ActorDestroyed,
                            __func__);
      });
}

void DocumentLoadListener::TriggerRedirectToRealChannel(
    const Maybe<uint64_t>& aDestinationProcess) {
  LOG((
      "DocumentLoadListener::TriggerRedirectToRealChannel [this=%p] "
      "aDestinationProcess=%" PRId64,
      this, aDestinationProcess ? int64_t(*aDestinationProcess) : int64_t(-1)));
  // This initiates replacing the current DocumentChannel with a
  // protocol specific 'real' channel, maybe in a different process than
  // the current DocumentChannelChild, if aDestinationProces is set.
  // It registers the current mChannel with the registrar to get an ID
  // so that the remote end can setup a new IPDL channel and lookup
  // the same underlying channel.
  // We expect this process to finish with FinishReplacementChannelSetup
  // (for both in-process and process switch cases), where we cleanup
  // the registrar and copy across any needed state to the replacing
  // IPDL parent object.

  nsTArray<ParentEndpoint> parentEndpoints(mStreamFilterRequests.Length());
  if (!mStreamFilterRequests.IsEmpty()) {
    base::ProcessId pid = OtherPid();
    if (aDestinationProcess) {
      dom::ContentParent* cp =
          dom::ContentProcessManager::GetSingleton()->GetContentProcessById(
              ContentParentId(*aDestinationProcess));
      if (cp) {
        pid = cp->OtherPid();
      }
    }

    for (StreamFilterRequest& request : mStreamFilterRequests) {
      ParentEndpoint parent;
      nsresult rv = extensions::PStreamFilter::CreateEndpoints(
          pid, request.mChildProcessId, &parent, &request.mChildEndpoint);

      if (NS_FAILED(rv)) {
        request.mPromise->Reject(false, __func__);
        request.mPromise = nullptr;
      } else {
        parentEndpoints.AppendElement(std::move(parent));
      }
    }
  }

  // If we didn't have any redirects, then we pass the REDIRECT_INTERNAL flag
  // for this channel switch so that it isn't recorded in session history etc.
  // If there were redirect(s), then we want this switch to be recorded as a
  // real one, since we have a new URI.
  uint32_t redirectFlags = 0;
  if (!mHaveVisibleRedirect) {
    redirectFlags = nsIChannelEventSink::REDIRECT_INTERNAL;
  }

  uint32_t newLoadFlags = nsIRequest::LOAD_NORMAL;
  MOZ_ALWAYS_SUCCEEDS(mChannel->GetLoadFlags(&newLoadFlags));
  // We're pulling our flags from the inner channel, which may not have this
  // flag set on it. This is the case when loading a 'view-source' channel.
  newLoadFlags |= nsIChannel::LOAD_DOCUMENT_URI;
  if (!aDestinationProcess) {
    newLoadFlags |= nsIChannel::LOAD_REPLACE;
  }

  // INHIBIT_PERSISTENT_CACHING is clearing during http redirects (from
  // both parent and content process channel instances), but only ever
  // re-added to the parent-side nsHttpChannel.
  // To match that behaviour, we want to explicitly avoid copying this flag
  // back to our newly created content side channel, otherwise it can
  // affect sub-resources loads in the same load group.
  nsCOMPtr<nsIURI> uri;
  mChannel->GetURI(getter_AddRefs(uri));
  if (uri && uri->SchemeIs("https")) {
    newLoadFlags &= ~nsIRequest::INHIBIT_PERSISTENT_CACHING;
  }

  RefPtr<DocumentLoadListener> self = this;
  RedirectToRealChannel(redirectFlags, newLoadFlags, aDestinationProcess,
                        std::move(parentEndpoints))
      ->Then(
          GetCurrentThreadSerialEventTarget(), __func__,
          [self, requests = std::move(mStreamFilterRequests)](
              const nsresult& aResponse) mutable {
            for (StreamFilterRequest& request : requests) {
              if (request.mPromise) {
                request.mPromise->Resolve(std::move(request.mChildEndpoint),
                                          __func__);
                request.mPromise = nullptr;
              }
            }
            self->RedirectToRealChannelFinished(aResponse);
          },
          [self](const mozilla::ipc::ResponseRejectReason) {
            self->RedirectToRealChannelFinished(NS_ERROR_FAILURE);
          });
}

NS_IMETHODIMP
DocumentLoadListener::OnStartRequest(nsIRequest* aRequest) {
  LOG(("DocumentLoadListener OnStartRequest [this=%p]", this));
  nsCOMPtr<nsIMultiPartChannel> multiPartChannel = do_QueryInterface(aRequest);
  if (multiPartChannel) {
    multiPartChannel->GetBaseChannel(getter_AddRefs(mChannel));
  } else {
    mChannel = do_QueryInterface(aRequest);
  }
  MOZ_DIAGNOSTIC_ASSERT(mChannel);
  RefPtr<nsHttpChannel> httpChannel = do_QueryObject(mChannel);

  if (!mDocumentChannelBridge && !mPendingDocumentChannelBridgeProcess) {
    return NS_ERROR_UNEXPECTED;
  }

  // Enforce CSP frame-ancestors and x-frame-options checks which
  // might cancel the channel.
  nsContentSecurityUtils::PerformCSPFrameAncestorAndXFOCheck(mChannel);

  // Generally we want to switch to a real channel even if the request failed,
  // since the listener might want to access protocol-specific data (like http
  // response headers) in its error handling.
  // An exception to this is when nsExtProtocolChannel handled the request and
  // returned NS_ERROR_NO_CONTENT, since creating a real one in the content
  // process will attempt to handle the URI a second time.
  nsresult status = NS_OK;
  aRequest->GetStatus(&status);
  if (status == NS_ERROR_NO_CONTENT) {
    DisconnectChildListeners(status, status);
    return NS_OK;
  }

  mStreamListenerFunctions.AppendElement(StreamListenerFunction{
      VariantIndex<0>{}, OnStartRequestParams{aRequest}});

  if (!mInitiatedRedirectToRealChannel) {
    mChannel->Suspend();
  } else {
    // This can be called multiple time if we have a multipart
    // decoder. Since we've already added the reqest to
    // mStreamListenerFunctions, we don't need to do anything else.
    return NS_OK;
  }
  mInitiatedRedirectToRealChannel = true;

  // Determine if a new process needs to be spawned. If it does, this will
  // trigger a cross process switch, and we should hold off on redirecting to
  // the real channel.
  bool willBeRemote = false;
  if (status == NS_BINDING_ABORTED ||
      !MaybeTriggerProcessSwitch(&willBeRemote)) {
    TriggerRedirectToRealChannel();

    // If we're not switching, then check if we're currently remote.
    if (GetBrowsingContext() && GetBrowsingContext()->GetContentParent()) {
      willBeRemote = true;
    }
  }

  // If we're going to be delivering this channel to a remote content
  // process, then we want to install any required content conversions
  // in the content process.
  // The caller of this OnStartRequest will install a conversion
  // helper after we return if we haven't disabled conversion. Normally
  // HttpChannelParent::OnStartRequest would disable conversion, but we're
  // defering calling that until later. Manually disable it now to prevent the
  // converter from being installed (since we want the child to do it), and
  // also save the value so that when we do call
  // HttpChannelParent::OnStartRequest, we can have the value as it originally
  // was.
  if (httpChannel) {
    Unused << httpChannel->GetApplyConversion(&mOldApplyConversion);
    if (willBeRemote) {
      httpChannel->SetApplyConversion(false);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
DocumentLoadListener::OnStopRequest(nsIRequest* aRequest,
                                    nsresult aStatusCode) {
  LOG(("DocumentLoadListener OnStopRequest [this=%p]", this));
  mStreamListenerFunctions.AppendElement(StreamListenerFunction{
      VariantIndex<2>{}, OnStopRequestParams{aRequest, aStatusCode}});

  // If we're not a multi-part channel, then we're finished and we don't
  // expect any further events. If we are, then this might be called again,
  // so wait for OnAfterLastPart instead.
  nsCOMPtr<nsIMultiPartChannel> multiPartChannel = do_QueryInterface(aRequest);
  if (!multiPartChannel) {
    mIsFinished = true;
  }

  mStreamFilterRequests.Clear();

  return NS_OK;
}

NS_IMETHODIMP
DocumentLoadListener::OnDataAvailable(nsIRequest* aRequest,
                                      nsIInputStream* aInputStream,
                                      uint64_t aOffset, uint32_t aCount) {
  LOG(("DocumentLoadListener OnDataAvailable [this=%p]", this));
  // This isn't supposed to happen, since we suspended the channel, but
  // sometimes Suspend just doesn't work. This can happen when we're routing
  // through nsUnknownDecoder to sniff the content type, and it doesn't handle
  // being suspended. Let's just store the data and manually forward it to our
  // redirected channel when it's ready.
  nsCString data;
  nsresult rv = NS_ReadInputStreamToString(aInputStream, data, aCount);
  NS_ENSURE_SUCCESS(rv, rv);

  mStreamListenerFunctions.AppendElement(StreamListenerFunction{
      VariantIndex<1>{},
      OnDataAvailableParams{aRequest, data, aOffset, aCount}});

  return NS_OK;
}

//-----------------------------------------------------------------------------
// DoucmentLoadListener::nsIMultiPartChannelListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
DocumentLoadListener::OnAfterLastPart(nsresult aStatus) {
  LOG(("DocumentLoadListener OnAfterLastPart [this=%p]", this));
  if (!mInitiatedRedirectToRealChannel) {
    // if we get here, and we haven't initiated a redirect to a real
    // channel, then it means we never got OnStartRequest (maybe a problem?)
    // and we retargeted everything.
    LOG(("DocumentLoadListener Disconnecting child"));
    DisconnectChildListeners(NS_BINDING_RETARGETED, NS_OK);
    return NS_OK;
  }
  mStreamListenerFunctions.AppendElement(StreamListenerFunction{
      VariantIndex<3>{}, OnAfterLastPartParams{aStatus}});
  mIsFinished = true;
  return NS_OK;
}

NS_IMETHODIMP
DocumentLoadListener::SetParentListener(
    mozilla::net::ParentChannelListener* listener) {
  // We don't need this (do we?)
  return NS_OK;
}

NS_IMETHODIMP
DocumentLoadListener::GetInterface(const nsIID& aIID, void** result) {
  RefPtr<CanonicalBrowsingContext> browsingContext =
      mParentChannelListener->GetBrowsingContext();
  if (aIID.Equals(NS_GET_IID(nsILoadContext)) && browsingContext) {
    browsingContext.forget(result);
    return NS_OK;
  }

  return QueryInterface(aIID, result);
}

// Rather than forwarding all these nsIParentChannel functions to the child,
// we cache a list of them, and then ask the 'real' channel to forward them
// for us after it's created.
NS_IMETHODIMP
DocumentLoadListener::NotifyFlashPluginStateChanged(
    nsIHttpChannel::FlashPluginState aState) {
  mIParentChannelFunctions.AppendElement(
      IParentChannelFunction{VariantIndex<0>{}, aState});
  return NS_OK;
}

NS_IMETHODIMP
DocumentLoadListener::SetClassifierMatchedInfo(const nsACString& aList,
                                               const nsACString& aProvider,
                                               const nsACString& aFullHash) {
  ClassifierMatchedInfoParams params;
  params.mList = aList;
  params.mProvider = aProvider;
  params.mFullHash = aFullHash;

  mIParentChannelFunctions.AppendElement(
      IParentChannelFunction{VariantIndex<1>{}, std::move(params)});
  return NS_OK;
}

NS_IMETHODIMP
DocumentLoadListener::SetClassifierMatchedTrackingInfo(
    const nsACString& aLists, const nsACString& aFullHash) {
  ClassifierMatchedTrackingInfoParams params;
  params.mLists = aLists;
  params.mFullHashes = aFullHash;

  mIParentChannelFunctions.AppendElement(
      IParentChannelFunction{VariantIndex<2>{}, std::move(params)});
  return NS_OK;
}

NS_IMETHODIMP
DocumentLoadListener::NotifyClassificationFlags(uint32_t aClassificationFlags,
                                                bool aIsThirdParty) {
  mIParentChannelFunctions.AppendElement(IParentChannelFunction{
      VariantIndex<3>{},
      ClassificationFlagsParams{aClassificationFlags, aIsThirdParty}});
  return NS_OK;
}

NS_IMETHODIMP
DocumentLoadListener::Delete() {
  if (mDocumentChannelBridge) {
    mDocumentChannelBridge->Delete();
  }
  return NS_OK;
}

NS_IMETHODIMP
DocumentLoadListener::AsyncOnChannelRedirect(
    nsIChannel* aOldChannel, nsIChannel* aNewChannel, uint32_t aFlags,
    nsIAsyncVerifyRedirectCallback* aCallback) {
  LOG(("DocumentLoadListener AsyncOnChannelRedirect [this=%p, aFlags=%" PRIx32
       "]",
       this, aFlags));
  // We generally don't want to notify the content process about redirects,
  // so just update our channel and tell the callback that we're good to go.
  mChannel = aNewChannel;

  // Since we're redirecting away from aOldChannel, we should check if it
  // had a COOP mismatch, since we want the final result for this to
  // include the state of all channels we redirected through.
  nsCOMPtr<nsIHttpChannelInternal> httpChannel = do_QueryInterface(aOldChannel);
  if (httpChannel) {
    bool isCOOPMismatch = false;
    Unused << NS_WARN_IF(NS_FAILED(
        httpChannel->HasCrossOriginOpenerPolicyMismatch(&isCOOPMismatch)));
    mHasCrossOriginOpenerPolicyMismatch |= isCOOPMismatch;
  }

  // We don't need to confirm internal redirects or record any
  // history for them, so just immediately verify and return.
  if (aFlags & nsIChannelEventSink::REDIRECT_INTERNAL) {
    LOG(
        ("DocumentLoadListener AsyncOnChannelRedirect [this=%p] "
         "flags=REDIRECT_INTERNAL",
         this));
    aCallback->OnRedirectVerifyCallback(NS_OK);
    return NS_OK;
  }

  if (!net::ChannelIsPost(aOldChannel)) {
    AddURIVisit(aOldChannel, 0);

    nsCOMPtr<nsIURI> oldURI;
    aOldChannel->GetURI(getter_AddRefs(oldURI));
    nsDocShell::SaveLastVisit(aNewChannel, oldURI, aFlags);
  }
  mHaveVisibleRedirect |= true;

  LOG(
      ("DocumentLoadListener AsyncOnChannelRedirect [this=%p] "
       "mHaveVisibleRedirect=%c",
       this, mHaveVisibleRedirect ? 'T' : 'F'));

  // If this is a cross-origin redirect, then we should no longer allow
  // mixed content. The destination docshell checks this in its redirect
  // handling, but if we deliver to a new docshell (with a process switch)
  // then this doesn't happen.
  // Manually remove the allow mixed content flags.
  nsresult rv = nsContentUtils::CheckSameOrigin(aOldChannel, aNewChannel);
  if (NS_FAILED(rv)) {
    if (mLoadStateLoadType == LOAD_NORMAL_ALLOW_MIXED_CONTENT) {
      mLoadStateLoadType = LOAD_NORMAL;
    } else if (mLoadStateLoadType == LOAD_RELOAD_ALLOW_MIXED_CONTENT) {
      mLoadStateLoadType = LOAD_RELOAD_NORMAL;
    }
    MOZ_ASSERT(!LOAD_TYPE_HAS_FLAGS(
        mLoadStateLoadType, nsIWebNavigation::LOAD_FLAGS_ALLOW_MIXED_CONTENT));
  }

  // We need the original URI of the current channel to use to open the real
  // channel in the content process. Unfortunately we overwrite the original
  // uri of the new channel with the original pre-redirect URI, so grab
  // a copy of it now.
  aNewChannel->GetOriginalURI(getter_AddRefs(mChannelCreationURI));

  // Clear out our nsIParentChannel functions, since a normal parent
  // channel would actually redirect and not have those values on the new one.
  // We expect the URI classifier to run on the redirected channel with
  // the new URI and set these again.
  mIParentChannelFunctions.Clear();

#ifdef ANDROID
  nsCOMPtr<nsIURI> uriBeingLoaded =
      AntiTrackingUtils::MaybeGetDocumentURIBeingLoaded(mChannel);
  RefPtr<CanonicalBrowsingContext> bc =
      mParentChannelListener->GetBrowsingContext();

  RefPtr<MozPromise<bool, bool, false>> promise;
  nsCOMPtr<nsIWidget> widget = bc->GetParentProcessWidgetContaining();
  RefPtr<nsWindow> window = nsWindow::From(widget);

  if (window) {
    promise = window->OnLoadRequest(uriBeingLoaded,
                                    nsIBrowserDOMWindow::OPEN_CURRENTWINDOW,
                                    nsIWebNavigation::LOAD_FLAGS_IS_REDIRECT,
                                    nullptr, false, bc->IsTopContent());
  }

  if (promise) {
    RefPtr<nsIAsyncVerifyRedirectCallback> cb = aCallback;
    promise->Then(
        GetCurrentThreadSerialEventTarget(), __func__,
        [=](const MozPromise<bool, bool, false>::ResolveOrRejectValue& aValue) {
          if (aValue.IsResolve()) {
            bool handled = aValue.ResolveValue();
            if (handled) {
              cb->OnRedirectVerifyCallback(NS_ERROR_ABORT);
            } else {
              cb->OnRedirectVerifyCallback(NS_OK);
            }
          }
        });
  } else
#endif /* ANDROID */
  {
    aCallback->OnRedirectVerifyCallback(NS_OK);
  }
  return NS_OK;
}

// This method returns the cached result of running the Cross-Origin-Opener
// policy compare algorithm by calling ComputeCrossOriginOpenerPolicyMismatch
bool DocumentLoadListener::HasCrossOriginOpenerPolicyMismatch() const {
  // If we found a COOP mismatch on an earlier channel and then
  // redirected away from that, we should use that result.
  if (mHasCrossOriginOpenerPolicyMismatch) {
    return true;
  }

  nsCOMPtr<nsIHttpChannelInternal> httpChannel = do_QueryInterface(mChannel);
  if (!httpChannel) {
    // Not an nsIHttpChannelInternal assume it's okay to switch.
    return false;
  }

  bool isCOOPMismatch = false;
  Unused << NS_WARN_IF(NS_FAILED(
      httpChannel->HasCrossOriginOpenerPolicyMismatch(&isCOOPMismatch)));
  return isCOOPMismatch;
}

auto DocumentLoadListener::AttachStreamFilter(base::ProcessId aChildProcessId)
    -> RefPtr<ChildEndpointPromise> {
  LOG(("DocumentLoadListener AttachStreamFilter [this=%p]", this));

  StreamFilterRequest* request = mStreamFilterRequests.AppendElement();
  request->mPromise = new ChildEndpointPromise::Private(__func__);
  request->mChildProcessId = aChildProcessId;
  return request->mPromise;
}

}  // namespace net
}  // namespace mozilla

#undef LOG
