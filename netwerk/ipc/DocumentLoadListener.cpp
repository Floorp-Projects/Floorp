/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocumentLoadListener.h"

#include "mozilla/AntiTrackingUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/MozPromiseInlines.h"  // For MozPromise::FromDomPromise
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticPrefs_extensions.h"
#include "mozilla/StaticPrefs_fission.h"
#include "mozilla/StaticPrefs_security.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/BrowsingContextGroup.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/ChildProcessChannelListener.h"
#include "mozilla/dom/ClientChannelHelper.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentProcessManager.h"
#include "mozilla/dom/SessionHistoryEntry.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/net/CookieJarSettings.h"
#include "mozilla/net/HttpChannelParent.h"
#include "mozilla/net/RedirectChannelRegistrar.h"
#include "nsContentSecurityUtils.h"
#include "nsDocShell.h"
#include "nsDocShellLoadState.h"
#include "nsDocShellLoadTypes.h"
#include "nsDOMNavigationTiming.h"
#include "nsExternalHelperAppService.h"
#include "nsHttpChannel.h"
#include "nsIBrowser.h"
#include "nsIE10SUtils.h"
#include "nsIHttpChannelInternal.h"
#include "nsIStreamConverterService.h"
#include "nsIViewSourceChannel.h"
#include "nsImportModule.h"
#include "nsIXULRuntime.h"
#include "nsMimeTypes.h"
#include "nsQueryObject.h"
#include "nsRedirectHistoryEntry.h"
#include "nsSandboxFlags.h"
#include "nsSHistory.h"
#include "nsStringStream.h"
#include "nsURILoader.h"
#include "nsWebNavigationInfo.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/nsHTTPSOnlyUtils.h"
#include "mozilla/dom/RemoteWebProgress.h"
#include "mozilla/dom/RemoteWebProgressRequest.h"
#include "mozilla/net/UrlClassifierFeatureFactory.h"
#include "mozilla/ExtensionPolicyService.h"

#ifdef ANDROID
#  include "mozilla/widget/nsWindow.h"
#endif /* ANDROID */

mozilla::LazyLogModule gDocumentChannelLog("DocumentChannel");
#define LOG(fmt) MOZ_LOG(gDocumentChannelLog, mozilla::LogLevel::Verbose, fmt)

extern mozilla::LazyLogModule gSHIPBFCacheLog;

using namespace mozilla::dom;

namespace mozilla {
namespace net {

static void SetNeedToAddURIVisit(nsIChannel* aChannel,
                                 bool aNeedToAddURIVisit) {
  nsCOMPtr<nsIWritablePropertyBag2> props(do_QueryInterface(aChannel));
  if (!props) {
    return;
  }

  props->SetPropertyAsBool(u"docshell.needToAddURIVisit"_ns,
                           aNeedToAddURIVisit);
}

static auto SecurityFlagsForLoadInfo(nsDocShellLoadState* aLoadState)
    -> nsSecurityFlags {
  // TODO: Block copied from nsDocShell::DoURILoad, refactor out somewhere
  nsSecurityFlags securityFlags =
      nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL;

  if (aLoadState->LoadType() == LOAD_ERROR_PAGE) {
    securityFlags |= nsILoadInfo::SEC_LOAD_ERROR_PAGE;
  }

  if (aLoadState->PrincipalToInherit()) {
    bool isSrcdoc = aLoadState->HasInternalLoadFlags(
        nsDocShell::INTERNAL_LOAD_FLAGS_IS_SRCDOC);
    bool inheritAttrs = nsContentUtils::ChannelShouldInheritPrincipal(
        aLoadState->PrincipalToInherit(), aLoadState->URI(),
        true,  // aInheritForAboutBlank
        isSrcdoc);

    bool isData = SchemeIsData(aLoadState->URI());
    if (inheritAttrs && !isData) {
      securityFlags |= nsILoadInfo::SEC_FORCE_INHERIT_PRINCIPAL;
    }
  }

  return securityFlags;
}

// Construct a LoadInfo object to use when creating the internal channel for a
// Document/SubDocument load.
static auto CreateDocumentLoadInfo(CanonicalBrowsingContext* aBrowsingContext,
                                   nsDocShellLoadState* aLoadState)
    -> already_AddRefed<LoadInfo> {
  uint32_t sandboxFlags = aBrowsingContext->GetSandboxFlags();
  RefPtr<LoadInfo> loadInfo;

  auto securityFlags = SecurityFlagsForLoadInfo(aLoadState);

  if (aBrowsingContext->GetParent()) {
    loadInfo = LoadInfo::CreateForFrame(aBrowsingContext,
                                        aLoadState->TriggeringPrincipal(),
                                        securityFlags, sandboxFlags);
  } else {
    OriginAttributes attrs;
    aBrowsingContext->GetOriginAttributes(attrs);
    loadInfo = LoadInfo::CreateForDocument(aBrowsingContext,
                                           aLoadState->TriggeringPrincipal(),
                                           attrs, securityFlags, sandboxFlags);
  }

  loadInfo->SetTriggeringSandboxFlags(aLoadState->TriggeringSandboxFlags());
  loadInfo->SetHasValidUserGestureActivation(
      aLoadState->HasValidUserGestureActivation());

  return loadInfo.forget();
}

// Construct a LoadInfo object to use when creating the internal channel for an
// Object/Embed load.
static auto CreateObjectLoadInfo(nsDocShellLoadState* aLoadState,
                                 uint64_t aInnerWindowId,
                                 nsContentPolicyType aContentPolicyType,
                                 uint32_t aSandboxFlags)
    -> already_AddRefed<LoadInfo> {
  RefPtr<WindowGlobalParent> wgp =
      WindowGlobalParent::GetByInnerWindowId(aInnerWindowId);
  MOZ_RELEASE_ASSERT(wgp);

  auto securityFlags = SecurityFlagsForLoadInfo(aLoadState);

  RefPtr<LoadInfo> loadInfo = LoadInfo::CreateForNonDocument(
      wgp, wgp->DocumentPrincipal(), aContentPolicyType, securityFlags,
      aSandboxFlags);

  loadInfo->SetHasValidUserGestureActivation(
      aLoadState->HasValidUserGestureActivation());
  loadInfo->SetTriggeringSandboxFlags(aLoadState->TriggeringSandboxFlags());

  return loadInfo.forget();
}

static auto WebProgressForBrowsingContext(
    CanonicalBrowsingContext* aBrowsingContext)
    -> already_AddRefed<BrowsingContextWebProgress> {
  return RefPtr<BrowsingContextWebProgress>(aBrowsingContext->GetWebProgress())
      .forget();
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
                                mozilla::dom::BrowsingContext* aBrowsingContext,
                                bool aIsDocumentLoad)
      : nsDocumentOpenInfo(aFlags, false),
        mBrowsingContext(aBrowsingContext),
        mListener(aListener),
        mIsDocumentLoad(aIsDocumentLoad) {
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
    RefPtr<nsIStreamListener> listener;
    nsresult rv = aHelperAppService->CreateListener(
        mContentType, aChannel, mBrowsingContext, false, nullptr,
        getter_AddRefs(listener));
    if (NS_SUCCEEDED(rv)) {
      m_targetStreamListener = listener;
    }
    return rv;
  }

  nsDocumentOpenInfo* Clone() override {
    mCloned = true;
    return new ParentProcessDocumentOpenInfo(mListener, mFlags,
                                             mBrowsingContext, mIsDocumentLoad);
  }

  nsresult OnDocumentStartRequest(nsIRequest* request) {
    LOG(("ParentProcessDocumentOpenInfo OnDocumentStartRequest [this=%p]",
         this));

    nsresult rv = nsDocumentOpenInfo::OnStartRequest(request);

    // If we didn't find a content handler,
    // and we don't have a listener, then just forward to our
    // default listener. This happens when the channel is in
    // an error state, and we want to just forward that on to be
    // handled in the content process.
    if (NS_SUCCEEDED(rv) && !mUsedContentHandler && !m_targetStreamListener) {
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
        DisconnectChildListeners(NS_FAILED(rv) ? rv : NS_BINDING_RETARGETED,
                                 rv);
      }
    }
    return rv;
  }

  nsresult OnObjectStartRequest(nsIRequest* request) {
    LOG(("ParentProcessDocumentOpenInfo OnObjectStartRequest [this=%p]", this));
    // Just redirect to the nsObjectLoadingContent in the content process.
    m_targetStreamListener = mListener;
    return m_targetStreamListener->OnStartRequest(request);
  }

  NS_IMETHOD OnStartRequest(nsIRequest* request) override {
    LOG(("ParentProcessDocumentOpenInfo OnStartRequest [this=%p]", this));

    if (mIsDocumentLoad) {
      return OnDocumentStartRequest(request);
    }

    return OnObjectStartRequest(request);
  }

  NS_IMETHOD OnAfterLastPart(nsresult aStatus) override {
    mListener->OnAfterLastPart(aStatus);
    return NS_OK;
  }

 private:
  virtual ~ParentProcessDocumentOpenInfo() {
    LOG(("ParentProcessDocumentOpenInfo dtor [this=%p]", this));
  }

  void DisconnectChildListeners(nsresult aStatus, nsresult aLoadGroupStatus) {
    // Tell the DocumentLoadListener to notify the content process that it's
    // been entirely retargeted, and to stop waiting.
    // Clear mListener's pointer to the DocumentLoadListener to break the
    // reference cycle.
    RefPtr<DocumentLoadListener> doc = do_GetInterface(ToSupports(mListener));
    MOZ_ASSERT(doc);
    doc->DisconnectListeners(aStatus, aLoadGroupStatus);
    mListener->SetListenerAfterRedirect(nullptr);
  }

  RefPtr<mozilla::dom::BrowsingContext> mBrowsingContext;
  RefPtr<ParentChannelListener> mListener;
  const bool mIsDocumentLoad;

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
  NS_INTERFACE_MAP_ENTRY(nsIProgressEventSink)
  NS_INTERFACE_MAP_ENTRY_CONCRETE(DocumentLoadListener)
NS_INTERFACE_MAP_END

DocumentLoadListener::DocumentLoadListener(
    CanonicalBrowsingContext* aLoadingBrowsingContext, bool aIsDocumentLoad)
    : mIsDocumentLoad(aIsDocumentLoad) {
  LOG(("DocumentLoadListener ctor [this=%p]", this));
  mParentChannelListener =
      new ParentChannelListener(this, aLoadingBrowsingContext,
                                aLoadingBrowsingContext->UsePrivateBrowsing());
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
      GetDocumentBrowsingContext();
  nsCOMPtr<nsIWidget> widget =
      browsingContext->GetParentProcessWidgetContaining();

  nsDocShell::InternalAddURIVisit(uri, previousURI, previousFlags,
                                  responseStatus, browsingContext, widget,
                                  mLoadStateLoadType);
}

CanonicalBrowsingContext* DocumentLoadListener::GetLoadingBrowsingContext()
    const {
  return mParentChannelListener ? mParentChannelListener->GetBrowsingContext()
                                : nullptr;
}

CanonicalBrowsingContext* DocumentLoadListener::GetDocumentBrowsingContext()
    const {
  return mIsDocumentLoad ? GetLoadingBrowsingContext() : nullptr;
}

CanonicalBrowsingContext* DocumentLoadListener::GetTopBrowsingContext() const {
  auto* loadingContext = GetLoadingBrowsingContext();
  return loadingContext ? loadingContext->Top() : nullptr;
}

WindowGlobalParent* DocumentLoadListener::GetParentWindowContext() const {
  return mParentWindowContext;
}

auto DocumentLoadListener::Open(nsDocShellLoadState* aLoadState,
                                LoadInfo* aLoadInfo, nsLoadFlags aLoadFlags,
                                uint32_t aCacheKey,
                                const Maybe<uint64_t>& aChannelId,
                                const TimeStamp& aAsyncOpenTime,
                                nsDOMNavigationTiming* aTiming,
                                Maybe<ClientInfo>&& aInfo, bool aUrgentStart,
                                base::ProcessId aPid, nsresult* aRv)
    -> RefPtr<OpenPromise> {
  auto* loadingContext = GetLoadingBrowsingContext();

  MOZ_DIAGNOSTIC_ASSERT_IF(loadingContext->GetParent(),
                           loadingContext->GetParentWindowContext());

  OriginAttributes attrs;
  loadingContext->GetOriginAttributes(attrs);

  mLoadIdentifier = aLoadState->GetLoadIdentifier();

  if (!nsDocShell::CreateAndConfigureRealChannelForLoadState(
          loadingContext, aLoadState, aLoadInfo, mParentChannelListener,
          nullptr, attrs, aLoadFlags, aCacheKey, *aRv,
          getter_AddRefs(mChannel))) {
    LOG(("DocumentLoadListener::Open failed to create channel [this=%p]",
         this));
    mParentChannelListener = nullptr;
    return nullptr;
  }

  auto* documentContext = GetDocumentBrowsingContext();
  if (documentContext && aLoadState->LoadType() != LOAD_ERROR_PAGE &&
      mozilla::SessionHistoryInParent()) {
    // It's hard to know at this point whether session history will be enabled
    // in the browsing context, so we always create an entry for a load here.
    mLoadingSessionHistoryInfo =
        documentContext->CreateLoadingSessionHistoryEntryForLoad(aLoadState,
                                                                 mChannel);
    if (!mLoadingSessionHistoryInfo) {
      *aRv = NS_BINDING_ABORTED;
      mParentChannelListener = nullptr;
      mChannel = nullptr;
      return nullptr;
    }
  }

  nsCOMPtr<nsIURI> uriBeingLoaded;
  Unused << NS_WARN_IF(
      NS_FAILED(mChannel->GetURI(getter_AddRefs(uriBeingLoaded))));

  RefPtr<HttpBaseChannel> httpBaseChannel = do_QueryObject(mChannel, aRv);
  if (uriBeingLoaded && httpBaseChannel) {
    nsCOMPtr<nsIURI> topWindowURI;
    if (mIsDocumentLoad && loadingContext->IsTop()) {
      // If this is for the top level loading, the top window URI should be the
      // URI which we are loading.
      topWindowURI = uriBeingLoaded;
    } else if (RefPtr<WindowGlobalParent> topWindow = AntiTrackingUtils::
                   GetTopWindowExcludingExtensionAccessibleContentFrames(
                       loadingContext, uriBeingLoaded)) {
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
  mDocumentChannelId = aChannelId;

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
        loadingContext->GetRequestContextId());

    nsCOMPtr<nsIClassOfService> cos(do_QueryInterface(httpChannel));
    if (cos && aUrgentStart) {
      cos->AddClassFlags(nsIClassOfService::UrgentStart);
    }
  }

  // Setup a ClientChannelHelper to watch for redirects, and copy
  // across any serviceworker related data between channels as needed.
  AddClientChannelHelperInParent(mChannel, std::move(aInfo));

  if (documentContext && !documentContext->StartDocumentLoad(this)) {
    LOG(("DocumentLoadListener::Open failed StartDocumentLoad [this=%p]",
         this));
    *aRv = NS_BINDING_ABORTED;
    mParentChannelListener = nullptr;
    mChannel = nullptr;
    return nullptr;
  }

  // Recalculate the openFlags, matching the logic in use in Content process.
  // NOTE: The only case not handled here to mirror Content process is
  // redirecting to re-use the channel.
  MOZ_ASSERT(!aLoadState->GetPendingRedirectedChannel());
  uint32_t openFlags =
      nsDocShell::ComputeURILoaderFlags(loadingContext, aLoadState->LoadType());

  RefPtr<ParentProcessDocumentOpenInfo> openInfo =
      new ParentProcessDocumentOpenInfo(mParentChannelListener, openFlags,
                                        loadingContext, mIsDocumentLoad);
  openInfo->Prepare();

#ifdef ANDROID
  RefPtr<MozPromise<bool, bool, false>> promise;
  if (documentContext && aLoadState->LoadType() != LOAD_ERROR_PAGE &&
      !(aLoadState->HasInternalLoadFlags(
          nsDocShell::INTERNAL_LOAD_FLAGS_BYPASS_LOAD_URI_DELEGATE)) &&
      !(aLoadState->LoadType() & LOAD_HISTORY)) {
    nsCOMPtr<nsIWidget> widget =
        documentContext->GetParentProcessWidgetContaining();
    RefPtr<nsWindow> window = nsWindow::From(widget);

    if (window) {
      promise = window->OnLoadRequest(
          aLoadState->URI(), nsIBrowserDOMWindow::OPEN_CURRENTWINDOW,
          aLoadState->InternalLoadFlags(), aLoadState->TriggeringPrincipal(),
          aLoadState->HasValidUserGestureActivation(),
          documentContext->IsTopContent());
    }
  }

  if (promise) {
    RefPtr<DocumentLoadListener> self = this;
    promise->Then(
        GetCurrentSerialEventTarget(), __func__,
        [=](const MozPromise<bool, bool, false>::ResolveOrRejectValue& aValue) {
          if (aValue.IsResolve()) {
            bool handled = aValue.ResolveValue();
            if (handled) {
              self->DisconnectListeners(NS_ERROR_ABORT, NS_ERROR_ABORT);
              mParentChannelListener = nullptr;
            } else {
              nsresult rv = mChannel->AsyncOpen(openInfo);
              if (NS_FAILED(rv)) {
                self->DisconnectListeners(rv, rv);
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
      LOG(("DocumentLoadListener::Open failed AsyncOpen [this=%p rv=%" PRIx32
           "]",
           this, static_cast<uint32_t>(*aRv)));
      if (documentContext) {
        documentContext->EndDocumentLoad(false);
      }
      mParentChannelListener = nullptr;
      return nullptr;
    }
  }

  // HTTPS-Only Mode fights potential timeouts caused by upgrades. Instantly
  // after opening the document channel we have to kick off countermeasures.
  nsHTTPSOnlyUtils::PotentiallyFireHttpRequestToShortenTimout(this);

  mOtherPid = aPid;
  mChannelCreationURI = aLoadState->URI();
  mLoadStateExternalLoadFlags = aLoadState->LoadFlags();
  mLoadStateInternalLoadFlags = aLoadState->InternalLoadFlags();
  mLoadStateLoadType = aLoadState->LoadType();
  mTiming = aTiming;
  mSrcdocData = aLoadState->SrcdocData();
  mBaseURI = aLoadState->BaseURI();
  mOriginalUriString = aLoadState->GetOriginalURIString();
  if (documentContext) {
    mParentWindowContext = documentContext->GetParentWindowContext();
  } else {
    mParentWindowContext =
        WindowGlobalParent::GetByInnerWindowId(aLoadInfo->GetInnerWindowID());
    MOZ_RELEASE_ASSERT(mParentWindowContext->GetBrowsingContext() ==
                           GetLoadingBrowsingContext(),
                       "mismatched parent window context?");
  }

  *aRv = NS_OK;
  mOpenPromise = new OpenPromise::Private(__func__);
  // We make the promise use direct task dispatch in order to reduce the number
  // of event loops iterations.
  mOpenPromise->UseDirectTaskDispatch(__func__);
  return mOpenPromise;
}

auto DocumentLoadListener::OpenDocument(
    nsDocShellLoadState* aLoadState, uint32_t aCacheKey,
    const Maybe<uint64_t>& aChannelId, const TimeStamp& aAsyncOpenTime,
    nsDOMNavigationTiming* aTiming, Maybe<dom::ClientInfo>&& aInfo,
    Maybe<bool> aUriModified, Maybe<bool> aIsXFOError, base::ProcessId aPid,
    nsresult* aRv) -> RefPtr<OpenPromise> {
  LOG(("DocumentLoadListener [%p] OpenDocument [uri=%s]", this,
       aLoadState->URI()->GetSpecOrDefault().get()));

  MOZ_ASSERT(mIsDocumentLoad);

  RefPtr<CanonicalBrowsingContext> browsingContext =
      GetDocumentBrowsingContext();

  // If this is a top-level load, then rebuild the LoadInfo from scratch,
  // since the goal is to be able to initiate loads in the parent, where the
  // content process won't have provided us with an existing one.
  RefPtr<LoadInfo> loadInfo =
      CreateDocumentLoadInfo(browsingContext, aLoadState);

  nsLoadFlags loadFlags = aLoadState->CalculateChannelLoadFlags(
      browsingContext, std::move(aUriModified), std::move(aIsXFOError));

  return Open(aLoadState, loadInfo, loadFlags, aCacheKey, aChannelId,
              aAsyncOpenTime, aTiming, std::move(aInfo), false, aPid, aRv);
}

auto DocumentLoadListener::OpenObject(
    nsDocShellLoadState* aLoadState, uint32_t aCacheKey,
    const Maybe<uint64_t>& aChannelId, const TimeStamp& aAsyncOpenTime,
    nsDOMNavigationTiming* aTiming, Maybe<dom::ClientInfo>&& aInfo,
    uint64_t aInnerWindowId, nsLoadFlags aLoadFlags,
    nsContentPolicyType aContentPolicyType, bool aUrgentStart,
    base::ProcessId aPid, ObjectUpgradeHandler* aObjectUpgradeHandler,
    nsresult* aRv) -> RefPtr<OpenPromise> {
  LOG(("DocumentLoadListener [%p] OpenObject [uri=%s]", this,
       aLoadState->URI()->GetSpecOrDefault().get()));

  MOZ_ASSERT(!mIsDocumentLoad);

  auto sandboxFlags = GetLoadingBrowsingContext()->GetSandboxFlags();

  RefPtr<LoadInfo> loadInfo = CreateObjectLoadInfo(
      aLoadState, aInnerWindowId, aContentPolicyType, sandboxFlags);

  mObjectUpgradeHandler = aObjectUpgradeHandler;

  return Open(aLoadState, loadInfo, aLoadFlags, aCacheKey, aChannelId,
              aAsyncOpenTime, aTiming, std::move(aInfo), aUrgentStart, aPid,
              aRv);
}

auto DocumentLoadListener::OpenInParent(nsDocShellLoadState* aLoadState,
                                        bool aSupportsRedirectToRealChannel)
    -> RefPtr<OpenPromise> {
  MOZ_ASSERT(mIsDocumentLoad);

  // We currently only support passing nullptr for aLoadInfo for
  // top level browsing contexts.
  auto* browsingContext = GetDocumentBrowsingContext();
  if (!browsingContext->IsTopContent() ||
      !browsingContext->GetContentParent()) {
    LOG(("DocumentLoadListener::OpenInParent failed because of subdoc"));
    return nullptr;
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
      return nullptr;
    }
  }

  // Clone because this mutates the load flags in the load state, which
  // breaks nsDocShells expectations of being able to do it.
  RefPtr<nsDocShellLoadState> loadState = new nsDocShellLoadState(*aLoadState);
  loadState->CalculateLoadURIFlags();

  RefPtr<nsDOMNavigationTiming> timing = new nsDOMNavigationTiming(nullptr);
  timing->NotifyNavigationStart(
      browsingContext->IsActive()
          ? nsDOMNavigationTiming::DocShellState::eActive
          : nsDOMNavigationTiming::DocShellState::eInactive);

  const mozilla::dom::LoadingSessionHistoryInfo* loadingInfo =
      loadState->GetLoadingSessionHistoryInfo();

  uint32_t cacheKey = 0;
  auto loadType = aLoadState->LoadType();
  if (loadType == LOAD_HISTORY || loadType == LOAD_RELOAD_NORMAL ||
      loadType == LOAD_RELOAD_CHARSET_CHANGE ||
      loadType == LOAD_RELOAD_CHARSET_CHANGE_BYPASS_CACHE ||
      loadType == LOAD_RELOAD_CHARSET_CHANGE_BYPASS_PROXY_AND_CACHE) {
    if (loadingInfo) {
      cacheKey = loadingInfo->mInfo.GetCacheKey();
    }
  }

  // Loads start in the content process might have exposed a channel id to
  // observers, so we need to preserve the value in the parent. That can't have
  // happened here, so Nothing() is fine.
  Maybe<uint64_t> channelId = Nothing();

  // Initial client info is only relevant for subdocument loads, which we're
  // not supporting yet.
  Maybe<dom::ClientInfo> initialClientInfo;

  mSupportsRedirectToRealChannel = aSupportsRedirectToRealChannel;

  // This is a top-level load, so rebuild the LoadInfo from scratch,
  // since in the parent the
  // content process won't have provided us with an existing one.
  RefPtr<LoadInfo> loadInfo =
      CreateDocumentLoadInfo(browsingContext, aLoadState);

  nsLoadFlags loadFlags = loadState->CalculateChannelLoadFlags(
      browsingContext,
      Some(loadingInfo && loadingInfo->mInfo.GetURIWasModified()), Nothing());

  nsresult rv;
  return Open(loadState, loadInfo, loadFlags, cacheKey, channelId,
              TimeStamp::Now(), timing, std::move(initialClientInfo), false,
              browsingContext->GetContentParent()->OtherPid(), &rv);
}

void DocumentLoadListener::FireStateChange(uint32_t aStateFlags,
                                           nsresult aStatus) {
  nsCOMPtr<nsIChannel> request = GetChannel();
  nsCOMPtr<nsIWebProgress> webProgress =
      new RemoteWebProgress(GetLoadType(), true, true);

  RefPtr<BrowsingContextWebProgress> loadingWebProgress =
      WebProgressForBrowsingContext(GetLoadingBrowsingContext());

  if (loadingWebProgress) {
    NS_DispatchToMainThread(
        NS_NewRunnableFunction("DocumentLoadListener::FireStateChange", [=]() {
          loadingWebProgress->OnStateChange(webProgress, request, aStateFlags,
                                            aStatus);
        }));
  }
}

static void SetNavigating(CanonicalBrowsingContext* aBrowsingContext,
                          bool aNavigating) {
  nsCOMPtr<nsIBrowser> browser;
  if (RefPtr<Element> currentElement = aBrowsingContext->GetEmbedderElement()) {
    browser = currentElement->AsBrowser();
  }

  if (!browser) {
    return;
  }

  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "DocumentLoadListener::SetNavigating",
      [browser, aNavigating]() { browser->SetIsNavigating(aNavigating); }));
}

/* static */ bool DocumentLoadListener::LoadInParent(
    CanonicalBrowsingContext* aBrowsingContext, nsDocShellLoadState* aLoadState,
    bool aSetNavigating) {
  SetNavigating(aBrowsingContext, aSetNavigating);

  RefPtr<DocumentLoadListener> load =
      new DocumentLoadListener(aBrowsingContext, true);
  RefPtr<DocumentLoadListener::OpenPromise> promise = load->OpenInParent(
      aLoadState, /* aSupportsRedirectToRealChannel */ false);
  if (!promise) {
    SetNavigating(aBrowsingContext, false);
    return false;
  }

  // We passed false for aSupportsRedirectToRealChannel, so we should always
  // take the process switching path, and reject this promise.
  promise->Then(
      GetCurrentSerialEventTarget(), __func__,
      [load](DocumentLoadListener::OpenPromise::ResolveOrRejectValue&& aValue) {
        MOZ_ASSERT(aValue.IsReject());
        DocumentLoadListener::OpenPromiseFailedType& rejectValue =
            aValue.RejectValue();
        if (!rejectValue.mSwitchedProcess) {
          // If we're not switching the load to a new process, then it is
          // finished (and failed), and we should fire a state change to notify
          // observers. Normally the docshell would fire this, and it would get
          // filtered out by BrowserParent if needed.
          load->FireStateChange(nsIWebProgressListener::STATE_STOP |
                                    nsIWebProgressListener::STATE_IS_WINDOW |
                                    nsIWebProgressListener::STATE_IS_NETWORK,
                                rejectValue.mStatus);
        }
      });

  load->FireStateChange(nsIWebProgressListener::STATE_START |
                            nsIWebProgressListener::STATE_IS_DOCUMENT |
                            nsIWebProgressListener::STATE_IS_REQUEST |
                            nsIWebProgressListener::STATE_IS_WINDOW |
                            nsIWebProgressListener::STATE_IS_NETWORK,
                        NS_OK);
  SetNavigating(aBrowsingContext, false);
  return true;
}

/* static */
bool DocumentLoadListener::SpeculativeLoadInParent(
    dom::CanonicalBrowsingContext* aBrowsingContext,
    nsDocShellLoadState* aLoadState) {
  LOG(("DocumentLoadListener::OpenFromParent"));

  RefPtr<DocumentLoadListener> listener =
      new DocumentLoadListener(aBrowsingContext, true);

  auto promise = listener->OpenInParent(aLoadState, true);
  if (promise) {
    // Create an entry in the redirect channel registrar to
    // allocate an identifier for this load.
    nsCOMPtr<nsIRedirectChannelRegistrar> registrar =
        RedirectChannelRegistrar::GetOrCreate();
    uint64_t loadIdentifier = aLoadState->GetLoadIdentifier();
    DebugOnly<nsresult> rv =
        registrar->RegisterChannel(nullptr, loadIdentifier);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    // Register listener (as an nsIParentChannel) under our new identifier.
    rv = registrar->LinkChannels(loadIdentifier, listener, nullptr);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }
  return !!promise;
}

void DocumentLoadListener::CleanupParentLoadAttempt(uint64_t aLoadIdent) {
  nsCOMPtr<nsIRedirectChannelRegistrar> registrar =
      RedirectChannelRegistrar::GetOrCreate();

  nsCOMPtr<nsIParentChannel> parentChannel;
  registrar->GetParentChannel(aLoadIdent, getter_AddRefs(parentChannel));
  RefPtr<DocumentLoadListener> loadListener = do_QueryObject(parentChannel);

  if (loadListener) {
    // If the load listener is still registered, then we must have failed
    // to connect DocumentChannel into it. Better cancel it!
    loadListener->NotifyDocumentChannelFailed();
  }

  registrar->DeregisterChannels(aLoadIdent);
}

auto DocumentLoadListener::ClaimParentLoad(DocumentLoadListener** aListener,
                                           uint64_t aLoadIdent,
                                           Maybe<uint64_t> aChannelId)
    -> RefPtr<OpenPromise> {
  nsCOMPtr<nsIRedirectChannelRegistrar> registrar =
      RedirectChannelRegistrar::GetOrCreate();

  nsCOMPtr<nsIParentChannel> parentChannel;
  registrar->GetParentChannel(aLoadIdent, getter_AddRefs(parentChannel));
  RefPtr<DocumentLoadListener> loadListener = do_QueryObject(parentChannel);
  registrar->DeregisterChannels(aLoadIdent);

  if (!loadListener) {
    // The parent went away unexpectedly.
    *aListener = nullptr;
    return nullptr;
  }

  loadListener->mDocumentChannelId = aChannelId;

  MOZ_DIAGNOSTIC_ASSERT(loadListener->mOpenPromise);
  loadListener.forget(aListener);

  return (*aListener)->mOpenPromise;
}

void DocumentLoadListener::NotifyDocumentChannelFailed() {
  LOG(("DocumentLoadListener NotifyDocumentChannelFailed [this=%p]", this));
  // There's been no calls to ClaimParentLoad, and so no listeners have been
  // attached to mOpenPromise yet. As such we can run Then() on it.
  mOpenPromise->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [](DocumentLoadListener::OpenPromiseSucceededType&& aResolveValue) {
        aResolveValue.mPromise->Resolve(NS_BINDING_ABORTED, __func__);
      },
      []() {});

  Cancel(NS_BINDING_ABORTED);
}

void DocumentLoadListener::Disconnect() {
  LOG(("DocumentLoadListener Disconnect [this=%p]", this));
  // The nsHttpChannel may have a reference to this parent, release it
  // to avoid circular references.
  RefPtr<nsHttpChannel> httpChannelImpl = do_QueryObject(mChannel);
  if (httpChannelImpl) {
    httpChannelImpl->SetWarningReporter(nullptr);
  }

  if (auto* ctx = GetDocumentBrowsingContext()) {
    ctx->EndDocumentLoad(mDoingProcessSwitch);
  }
}

void DocumentLoadListener::Cancel(const nsresult& aStatusCode) {
  LOG(
      ("DocumentLoadListener Cancel [this=%p, "
       "aStatusCode=%" PRIx32 " ]",
       this, static_cast<uint32_t>(aStatusCode)));
  if (mOpenPromiseResolved) {
    return;
  }
  if (mChannel) {
    mChannel->Cancel(aStatusCode);
  }

  DisconnectListeners(aStatusCode, aStatusCode);
}

void DocumentLoadListener::DisconnectListeners(nsresult aStatus,
                                               nsresult aLoadGroupStatus,
                                               bool aSwitchedProcess) {
  LOG(
      ("DocumentLoadListener DisconnectListener [this=%p, "
       "aStatus=%" PRIx32 " aLoadGroupStatus=%" PRIx32 " ]",
       this, static_cast<uint32_t>(aStatus),
       static_cast<uint32_t>(aLoadGroupStatus)));

  RejectOpenPromise(aStatus, aLoadGroupStatus, aSwitchedProcess, __func__);

  Disconnect();

  if (!aSwitchedProcess) {
    // If we're not going to send anything else to the content process, and
    // we haven't yet consumed a stream filter promise, then we're never going
    // to. If we're disconnecting the old content process due to a proces
    // switch, then we can rely on FinishReplacementChannelSetup being called
    // (even if the switch failed), so we clear at that point instead.
    // TODO: This might be because we retargeted the stream to the download
    // handler or similar. Do we need to attach a stream filter to that?
    mStreamFilterRequests.Clear();
  }
}

void DocumentLoadListener::RedirectToRealChannelFinished(nsresult aRv) {
  LOG(
      ("DocumentLoadListener RedirectToRealChannelFinished [this=%p, "
       "aRv=%" PRIx32 " ]",
       this, static_cast<uint32_t>(aRv)));
  if (NS_FAILED(aRv)) {
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

  auto endDocumentLoad = MakeScopeExit([&]() {
    if (auto* ctx = GetDocumentBrowsingContext()) {
      ctx->EndDocumentLoad(false);
    }
  });
  mStreamFilterRequests.Clear();

  nsCOMPtr<nsIRedirectChannelRegistrar> registrar =
      RedirectChannelRegistrar::GetOrCreate();
  MOZ_ASSERT(registrar);

  nsCOMPtr<nsIParentChannel> redirectChannel;
  nsresult rv = registrar->GetParentChannel(mRedirectChannelId,
                                            getter_AddRefs(redirectChannel));
  if (NS_FAILED(rv) || !redirectChannel) {
    aResult = NS_ERROR_FAILURE;
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
    return;
  }

  MOZ_ASSERT(
      !SameCOMIdentity(redirectChannel, static_cast<nsIParentChannel*>(this)));

  redirectChannel->SetParentListener(mParentChannelListener);

  ApplyPendingFunctions(redirectChannel);

  if (!ResumeSuspendedChannel(redirectChannel)) {
    nsCOMPtr<nsILoadGroup> loadGroup;
    mChannel->GetLoadGroup(getter_AddRefs(loadGroup));
    if (loadGroup) {
      // We added ourselves to the load group, but attempting
      // to resume has notified us that the channel is already
      // finished. Better remove ourselves from the loadgroup
      // again. The only time the channel will be in a loadgroup
      // is if we're connected to the parent process.
      nsresult status = NS_OK;
      mChannel->GetStatus(&status);
      loadGroup->RemoveRequest(mChannel, nullptr, status);
    }
  }
}

void DocumentLoadListener::ApplyPendingFunctions(
    nsIParentChannel* aChannel) const {
  // We stored the values from all nsIParentChannel functions called since we
  // couldn't handle them. Copy them across to the real channel since it
  // should know what to do.

  nsCOMPtr<nsIParentChannel> parentChannel = aChannel;
  for (const auto& variant : mIParentChannelFunctions) {
    variant.match(
        [parentChannel](const nsIHttpChannel::FlashPluginState& aState) {
          parentChannel->NotifyFlashPluginStateChanged(aState);
        },
        [parentChannel](const ClassifierMatchedInfoParams& aParams) {
          parentChannel->SetClassifierMatchedInfo(
              aParams.mList, aParams.mProvider, aParams.mFullHash);
        },
        [parentChannel](const ClassifierMatchedTrackingInfoParams& aParams) {
          parentChannel->SetClassifierMatchedTrackingInfo(aParams.mLists,
                                                          aParams.mFullHashes);
        },
        [parentChannel](const ClassificationFlagsParams& aParams) {
          parentChannel->NotifyClassificationFlags(aParams.mClassificationFlags,
                                                   aParams.mIsThirdParty);
        });
  }

  RefPtr<HttpChannelSecurityWarningReporter> reporter;
  if (RefPtr<HttpChannelParent> httpParent = do_QueryObject(aChannel)) {
    reporter = httpParent;
  } else if (RefPtr<nsHttpChannel> httpChannel = do_QueryObject(aChannel)) {
    reporter = httpChannel->GetWarningReporter();
  }
  if (reporter) {
    for (const auto& variant : mSecurityWarningFunctions) {
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

  if (auto* ctx = GetDocumentBrowsingContext()) {
    ctx->EndDocumentLoad(mDoingProcessSwitch);
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

  aArgs.loadIdentifier() = mLoadIdentifier;

  // I previously used HttpBaseChannel::CloneLoadInfoForRedirect, but that
  // clears the principal to inherit, which fails tests (probably because this
  // 'redirect' is usually just an implementation detail). It's also http
  // only, and mChannel can be anything that we redirected to.
  nsCOMPtr<nsILoadInfo> channelLoadInfo = mChannel->LoadInfo();
  nsCOMPtr<nsIPrincipal> principalToInherit;
  channelLoadInfo->GetPrincipalToInherit(getter_AddRefs(principalToInherit));

  const RefPtr<nsHttpChannel> baseChannel = do_QueryObject(mChannel);

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
        new nsRedirectHistoryEntry(uriPrincipal, nullptr, ""_ns);

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
  aArgs.properties() = do_QueryObject(mChannel);
  aArgs.srcdocData() = mSrcdocData;
  aArgs.baseUri() = mBaseURI;
  aArgs.loadStateExternalLoadFlags() = mLoadStateExternalLoadFlags;
  aArgs.loadStateInternalLoadFlags() = mLoadStateInternalLoadFlags;
  aArgs.loadStateLoadType() = mLoadStateLoadType;
  aArgs.originalUriString() = mOriginalUriString;
  if (mLoadingSessionHistoryInfo) {
    aArgs.loadingSessionHistoryInfo().emplace(*mLoadingSessionHistoryInfo);
  }
}

static bool IsLargeAllocationLoad(CanonicalBrowsingContext* aBrowsingContext,
                                  nsIChannel* aChannel) {
  if (!StaticPrefs::dom_largeAllocationHeader_enabled() ||
      aBrowsingContext->UseRemoteSubframes()) {
    return false;
  }

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel);
  if (!httpChannel) {
    return false;
  }

  nsAutoCString ignoredHeaderValue;
  nsresult rv =
      httpChannel->GetResponseHeader("Large-Allocation"_ns, ignoredHeaderValue);
  if (NS_FAILED(rv)) {
    return false;
  }

  // On all platforms other than win32, LargeAllocation is disabled by default,
  // and has to be force-enabled using `dom.largeAllocation.forceEnable`.
#if defined(XP_WIN) && defined(_X86_)
  return true;
#else
  return StaticPrefs::dom_largeAllocation_forceEnable();
#endif
}

static bool ContextCanProcessSwitch(CanonicalBrowsingContext* aBrowsingContext,
                                    WindowGlobalParent* aParentWindow) {
  if (NS_WARN_IF(!aBrowsingContext)) {
    LOG(("Process Switch Abort: no browsing context"));
    return false;
  }
  if (!aBrowsingContext->IsContent()) {
    LOG(("Process Switch Abort: non-content browsing context"));
    return false;
  }

  if (aParentWindow && !aBrowsingContext->UseRemoteSubframes()) {
    LOG(("Process Switch Abort: remote subframes disabled"));
    return false;
  }

  if (aParentWindow && aParentWindow->IsInProcess()) {
    LOG(("Process Switch Abort: Subframe with in-process parent"));
    return false;
  }

  // Determine what process switching behaviour is being requested by the root
  // <browser> element.
  Element* browserElement = aBrowsingContext->Top()->GetEmbedderElement();
  if (!browserElement) {
    LOG(("Process Switch Abort: cannot get embedder element"));
    return false;
  }
  nsCOMPtr<nsIBrowser> browser = browserElement->AsBrowser();
  if (!browser) {
    LOG(("Process Switch Abort: not loaded within nsIBrowser"));
    return false;
  }

  nsIBrowser::ProcessBehavior processBehavior =
      nsIBrowser::PROCESS_BEHAVIOR_DISABLED;
  nsresult rv = browser->GetProcessSwitchBehavior(&processBehavior);
  if (NS_FAILED(rv)) {
    MOZ_ASSERT_UNREACHABLE(
        "nsIBrowser::GetProcessSwitchBehavior shouldn't fail");
    LOG(("Process Switch Abort: failed to get process switch behavior"));
    return false;
  }

  // Check if the process switch we're considering is disabled by the
  // <browser>'s process behavior.
  if (processBehavior == nsIBrowser::PROCESS_BEHAVIOR_DISABLED) {
    LOG(("Process Switch Abort: switch disabled by <browser>"));
    return false;
  }
  if (!aParentWindow &&
      processBehavior == nsIBrowser::PROCESS_BEHAVIOR_SUBFRAME_ONLY) {
    LOG(("Process Switch Abort: toplevel switch disabled by <browser>"));
    return false;
  }

  return true;
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

  // If we're doing an <object>/<embed> load, we may be doing a document load at
  // this point. We never need to do a process switch for a non-document
  // <object> or <embed> load.
  if (!mIsDocumentLoad && !mChannel->IsDocument()) {
    LOG(("Process Switch Abort: non-document load"));
    return false;
  }

  // Get the loading BrowsingContext. This may not be the context which will be
  // switching processes in the case of an <object> or <embed> element, as we
  // don't create the final context until after process selection.
  //
  // - /!\ WARNING /!\ -
  // Don't use `browsingContext->IsTop()` in this method! It will behave
  // incorrectly for non-document loads such as `<object>` or `<embed>`.
  // Instead, check whether or not `parentWindow` is null.
  RefPtr<CanonicalBrowsingContext> browsingContext =
      GetLoadingBrowsingContext();
  RefPtr<WindowGlobalParent> parentWindow = GetParentWindowContext();
  if (!ContextCanProcessSwitch(browsingContext, parentWindow)) {
    return false;
  }

  // Get the final principal, used to select which process to load into.
  nsCOMPtr<nsIPrincipal> resultPrincipal;
  nsresult rv = nsContentUtils::GetSecurityManager()->GetChannelResultPrincipal(
      mChannel, getter_AddRefs(resultPrincipal));
  if (NS_FAILED(rv)) {
    LOG(("Process Switch Abort: failed to get channel result principal"));
    return false;
  }

  nsAutoCString currentRemoteType(NOT_REMOTE_TYPE);
  if (RefPtr<ContentParent> contentParent =
          browsingContext->GetContentParent()) {
    currentRemoteType = contentParent->GetRemoteType();
  }
  MOZ_ASSERT_IF(currentRemoteType.IsEmpty(), !OtherPid());

  // Determine what type of content process this load should finish in.
  nsAutoCString preferredRemoteType(currentRemoteType);
  RemotenessChangeOptions options;

  // If we're in a preloaded browser, force browsing context replacement to
  // ensure the current process is re-selected.
  {
    Element* browserElement = browsingContext->Top()->GetEmbedderElement();

    nsAutoString isPreloadBrowserStr;
    if (browserElement->GetAttr(kNameSpaceID_None, nsGkAtoms::preloadedState,
                                isPreloadBrowserStr) &&
        isPreloadBrowserStr.EqualsLiteral("consumed")) {
      nsCOMPtr<nsIURI> originalURI;
      if (NS_SUCCEEDED(mChannel->GetOriginalURI(getter_AddRefs(originalURI))) &&
          !originalURI->GetSpecOrDefault().EqualsLiteral("about:newtab")) {
        LOG(("Process Switch: leaving preloaded browser"));
        options.mReplaceBrowsingContext = true;
        browserElement->UnsetAttr(kNameSpaceID_None, nsGkAtoms::preloadedState,
                                  true);
      }
    }
  }

  // Update the preferred final process for our load based on the
  // Cross-Origin-Opener-Policy and Cross-Origin-Embedder-Policy headers.
  {
    bool isCOOPSwitch = HasCrossOriginOpenerPolicyMismatch();
    options.mReplaceBrowsingContext |= isCOOPSwitch;

    // Determine our COOP status, which will be used to determine our preferred
    // remote type.
    nsILoadInfo::CrossOriginOpenerPolicy coop =
        nsILoadInfo::OPENER_POLICY_UNSAFE_NONE;
    if (parentWindow) {
      coop = browsingContext->Top()->GetOpenerPolicy();
    } else if (nsCOMPtr<nsIHttpChannelInternal> httpChannel =
                   do_QueryInterface(mChannel)) {
      MOZ_ALWAYS_SUCCEEDS(httpChannel->GetCrossOriginOpenerPolicy(&coop));
    }

    if (coop ==
        nsILoadInfo::OPENER_POLICY_SAME_ORIGIN_EMBEDDER_POLICY_REQUIRE_CORP) {
      // We want documents with SAME_ORIGIN_EMBEDDER_POLICY_REQUIRE_CORP COOP
      // policy to be loaded in a separate process in which we can enable
      // high-resolution timers.
      nsAutoCString siteOrigin;
      resultPrincipal->GetSiteOrigin(siteOrigin);
      preferredRemoteType = WITH_COOP_COEP_REMOTE_TYPE_PREFIX;
      preferredRemoteType.Append(siteOrigin);
    } else if (isCOOPSwitch) {
      // If we're doing a COOP switch, we do not need any affinity to the
      // current remote type. Clear it back to the default value.
      preferredRemoteType = DEFAULT_REMOTE_TYPE;
    }
  }

  // If we're performing a large allocation load, override the remote type
  // with `LARGE_ALLOCATION_REMOTE_TYPE` to move it into an exclusive content
  // process. If we're already in one, and don't otherwise we force ourselves
  // out of that content process.
  if (!parentWindow && browsingContext->Group()->Toplevels().Length() == 1) {
    if (IsLargeAllocationLoad(browsingContext, mChannel)) {
      preferredRemoteType = LARGE_ALLOCATION_REMOTE_TYPE;
      options.mReplaceBrowsingContext = true;
    } else if (preferredRemoteType == LARGE_ALLOCATION_REMOTE_TYPE) {
      preferredRemoteType = DEFAULT_REMOTE_TYPE;
      options.mReplaceBrowsingContext = true;
    }
  }

  // Put toplevel BrowsingContexts which load within the extension process into
  // a specific BrowsingContextGroup.
  if (auto* addonPolicy = BasePrincipal::Cast(resultPrincipal)->AddonPolicy()) {
    if (!parentWindow) {
      // Toplevel extension BrowsingContexts must be loaded in the extension
      // browsing context group, within the extension content process.
      if (ExtensionPolicyService::GetSingleton().UseRemoteExtensions()) {
        preferredRemoteType = EXTENSION_REMOTE_TYPE;
      } else {
        preferredRemoteType = NOT_REMOTE_TYPE;
      }

      if (browsingContext->Group()->Id() !=
          addonPolicy->GetBrowsingContextGroupId()) {
        options.mReplaceBrowsingContext = true;
        options.mSpecificGroupId = addonPolicy->GetBrowsingContextGroupId();
      }
    } else {
      // As a temporary measure, extension iframes must be loaded within the
      // same process as their parent document.
      preferredRemoteType = parentWindow->GetRemoteType();
    }
  }

  LOG(
      ("DocumentLoadListener GetRemoteTypeForPrincipal "
       "[this=%p, contentParent=%s, preferredRemoteType=%s]",
       this, currentRemoteType.get(), preferredRemoteType.get()));

  nsCOMPtr<nsIE10SUtils> e10sUtils =
      do_ImportModule("resource://gre/modules/E10SUtils.jsm", "E10SUtils");
  if (!e10sUtils) {
    LOG(("Process Switch Abort: Could not import E10SUtils"));
    return false;
  }

  // Get information about the current document loaded in our BrowsingContext.
  nsCOMPtr<nsIPrincipal> currentPrincipal;
  RefPtr<WindowGlobalParent> wgp = browsingContext->GetCurrentWindowGlobal();
  if (wgp) {
    currentPrincipal = wgp->DocumentPrincipal();
  }

  rv = e10sUtils->GetRemoteTypeForPrincipal(
      resultPrincipal, mChannelCreationURI, browsingContext->UseRemoteTabs(),
      browsingContext->UseRemoteSubframes(), preferredRemoteType,
      currentPrincipal, parentWindow, options.mRemoteType);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG(("Process Switch Abort: getRemoteTypeForPrincipal threw an exception"));
    return false;
  }

  // If the final decision is to switch from an 'extension' remote type to any
  // other remote type, ensure the browsing context is replaced so that we leave
  // the extension-specific BrowsingContextGroup.
  if (!parentWindow && currentRemoteType != options.mRemoteType &&
      currentRemoteType == EXTENSION_REMOTE_TYPE) {
    options.mReplaceBrowsingContext = true;
  }

  if (mozilla::BFCacheInParent() && nsSHistory::GetMaxTotalViewers() > 0 &&
      !parentWindow && !browsingContext->HadOriginalOpener() &&
      browsingContext->Group()->Toplevels().Length() == 1 &&
      !options.mRemoteType.IsEmpty() &&
      browsingContext->GetHasLoadedNonInitialDocument() &&
      (mLoadStateLoadType == LOAD_NORMAL ||
       mLoadStateLoadType == LOAD_HISTORY || mLoadStateLoadType == LOAD_LINK ||
       mLoadStateLoadType == LOAD_STOP_CONTENT ||
       mLoadStateLoadType == LOAD_STOP_CONTENT_AND_REPLACE)) {
    options.mReplaceBrowsingContext = true;
    options.mTryUseBFCache = true;
  }

  LOG(("GetRemoteTypeForPrincipal -> current:%s remoteType:%s",
       currentRemoteType.get(), options.mRemoteType.get()));

  // Check if a process switch is needed.
  if (currentRemoteType == options.mRemoteType &&
      !options.mReplaceBrowsingContext) {
    LOG(("Process Switch Abort: type (%s) is compatible",
         options.mRemoteType.get()));
    return false;
  }

  if (NS_WARN_IF(parentWindow && options.mRemoteType.IsEmpty())) {
    LOG(("Process Switch Abort: non-remote target process for subframe"));
    return false;
  }

  *aWillSwitchToRemote = !options.mRemoteType.IsEmpty();

  // If we're doing a document load, we can immediately perform a process
  // switch.
  if (mIsDocumentLoad) {
    if (options.mTryUseBFCache && wgp) {
      if (RefPtr<BrowserParent> browserParent = wgp->GetBrowserParent()) {
        nsTArray<RefPtr<PContentParent::CanSavePresentationPromise>>
            canSavePromises;
        browsingContext->Group()->EachParent([&](ContentParent* aParent) {
          RefPtr<PContentParent::CanSavePresentationPromise> canSave =
              aParent->SendCanSavePresentation(browsingContext,
                                               mDocumentChannelId);
          canSavePromises.AppendElement(canSave);
        });

        PContentParent::CanSavePresentationPromise::All(
            GetCurrentSerialEventTarget(), canSavePromises)
            ->Then(
                GetMainThreadSerialEventTarget(), __func__,
                [self = RefPtr{this}, browsingContext,
                 options](const nsTArray<bool> aCanSaves) mutable {
                  bool canSave = !aCanSaves.Contains(false);
                  MOZ_LOG(gSHIPBFCacheLog, LogLevel::Debug,
                          ("DocumentLoadListener::MaybeTriggerProcessSwitch "
                           "saving presentation=%i",
                           canSave));
                  options.mTryUseBFCache = canSave;
                  self->TriggerProcessSwitch(browsingContext, options);
                },
                [self = RefPtr{this}, browsingContext,
                 options](ipc::ResponseRejectReason) mutable {
                  MOZ_LOG(gSHIPBFCacheLog, LogLevel::Debug,
                          ("DocumentLoadListener::MaybeTriggerProcessSwitch "
                           "error in trying to save presentation"));
                  options.mTryUseBFCache = false;
                  self->TriggerProcessSwitch(browsingContext, options);
                });
        return true;
      }
    }

    options.mTryUseBFCache = false;
    TriggerProcessSwitch(browsingContext, options);
    return true;
  }

  // We're not doing a document load, which means we must be performing an
  // object load. We need a BrowsingContext to perform the switch in, so will
  // trigger an upgrade.
  if (!mObjectUpgradeHandler) {
    LOG(("Process Switch Abort: no object upgrade handler"));
    return false;
  }

  if (!StaticPrefs::fission_remoteObjectEmbed()) {
    LOG(("Process Switch Abort: remote <object>/<embed> disabled"));
    return false;
  }

  mObjectUpgradeHandler->UpgradeObjectLoad()->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [self = RefPtr{this}, options,
       wgp](const RefPtr<CanonicalBrowsingContext>& aBrowsingContext) mutable {
        if (aBrowsingContext->IsDiscarded() ||
            wgp != aBrowsingContext->GetParentWindowContext()) {
          LOG(
              ("Process Switch: Got invalid BrowsingContext from object "
               "upgrade!"));
          self->RedirectToRealChannelFinished(NS_ERROR_FAILURE);
          return;
        }

        LOG(("Process Switch: Upgraded Object to Document Load"));
        self->TriggerProcessSwitch(aBrowsingContext, options);
      },
      [self = RefPtr{this}](nsresult aStatusCode) {
        MOZ_ASSERT(NS_FAILED(aStatusCode), "Status should be error");
        self->RedirectToRealChannelFinished(aStatusCode);
      });
  return true;
}

void DocumentLoadListener::TriggerProcessSwitch(
    CanonicalBrowsingContext* aContext,
    const RemotenessChangeOptions& aOptions) {
  nsAutoCString currentRemoteType(NOT_REMOTE_TYPE);
  if (RefPtr<ContentParent> contentParent = aContext->GetContentParent()) {
    currentRemoteType = contentParent->GetRemoteType();
  }
  MOZ_ASSERT_IF(currentRemoteType.IsEmpty(), !OtherPid());

  LOG(("Process Switch: Changing Remoteness from '%s' to '%s'",
       currentRemoteType.get(), aOptions.mRemoteType.get()));

  // We're now committing to a process switch, so we can disconnect from
  // the listeners in the old process.
  mDoingProcessSwitch = true;

  DisconnectListeners(NS_BINDING_ABORTED, NS_BINDING_ABORTED, true);

  LOG(("Process Switch: Calling ChangeRemoteness"));
  aContext->ChangeRemoteness(aOptions, mLoadIdentifier)
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [self = RefPtr{this}](BrowserParent* aBrowserParent) {
            MOZ_ASSERT(self->mChannel,
                       "Something went wrong, channel got cancelled");
            self->TriggerRedirectToRealChannel(Some(
                aBrowserParent ? aBrowserParent->Manager()->ChildID() : 0));
          },
          [self = RefPtr{this}](nsresult aStatusCode) {
            MOZ_ASSERT(NS_FAILED(aStatusCode), "Status should be error");
            self->RedirectToRealChannelFinished(aStatusCode);
          });
}

RefPtr<PDocumentChannelParent::RedirectToRealChannelPromise>
DocumentLoadListener::RedirectToParentProcess(uint32_t aRedirectFlags,
                                              uint32_t aLoadFlags) {
  // This is largely the same as ContentChild::RecvCrossProcessRedirect,
  // except without needing to deserialize or create an nsIChildChannel.

  RefPtr<nsDocShellLoadState> loadState;
  nsDocShellLoadState::CreateFromPendingChannel(
      mChannel, mLoadIdentifier, mRedirectChannelId, getter_AddRefs(loadState));

  loadState->SetLoadFlags(mLoadStateExternalLoadFlags);
  loadState->SetInternalLoadFlags(mLoadStateInternalLoadFlags);
  loadState->SetLoadType(mLoadStateLoadType);
  if (mLoadingSessionHistoryInfo) {
    loadState->SetLoadingSessionHistoryInfo(*mLoadingSessionHistoryInfo);
  }

  // This is poorly named now.
  RefPtr<ChildProcessChannelListener> processListener =
      ChildProcessChannelListener::GetSingleton();

  auto promise =
      MakeRefPtr<PDocumentChannelParent::RedirectToRealChannelPromise::Private>(
          __func__);
  promise->UseDirectTaskDispatch(__func__);
  auto resolve = [promise](nsresult aResult) {
    promise->Resolve(aResult, __func__);
  };

  nsTArray<ipc::Endpoint<extensions::PStreamFilterParent>> endpoints;
  processListener->OnChannelReady(loadState, mLoadIdentifier,
                                  std::move(endpoints), mTiming,
                                  std::move(resolve));

  return promise;
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

  if (mIsDocumentLoad) {
    // TODO(djg): Add the last URI visit to history if success. Is there a
    // better place to handle this? Need access to the updated aLoadFlags.
    nsresult status = NS_OK;
    mChannel->GetStatus(&status);
    bool updateGHistory =
        nsDocShell::ShouldUpdateGlobalHistory(mLoadStateLoadType);
    if (NS_SUCCEEDED(status) && updateGHistory &&
        !net::ChannelIsPost(mChannel)) {
      AddURIVisit(mChannel, aLoadFlags);
    }
  }

  // Register the new channel and obtain id for it
  nsCOMPtr<nsIRedirectChannelRegistrar> registrar =
      RedirectChannelRegistrar::GetOrCreate();
  MOZ_ASSERT(registrar);
  nsCOMPtr<nsIChannel> chan = mChannel;
  if (nsCOMPtr<nsIViewSourceChannel> vsc = do_QueryInterface(chan)) {
    chan = vsc->GetInnerChannel();
  }
  mRedirectChannelId = nsContentUtils::GenerateLoadIdentifier();
  MOZ_ALWAYS_SUCCEEDS(registrar->RegisterChannel(chan, mRedirectChannelId));

  if (aDestinationProcess) {
    if (!*aDestinationProcess) {
      MOZ_ASSERT(aStreamFilterEndpoints.IsEmpty());
      return RedirectToParentProcess(aRedirectFlags, aLoadFlags);
    }
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

  if (mOpenPromiseResolved) {
    LOG(
        ("DocumentLoadListener RedirectToRealChannel [this=%p] "
         "promise already resolved. Aborting.",
         this));
    // The promise has already been resolved or aborted, so we have no way to
    // return a promise again to the listener which would cancel the operation.
    // Reject the promise immediately.
    return PDocumentChannelParent::RedirectToRealChannelPromise::
        CreateAndResolve(NS_BINDING_ABORTED, __func__);
  }

  // This promise will be passed on the promise listener which will
  // resolve this promise for us.
  auto promise =
      MakeRefPtr<PDocumentChannelParent::RedirectToRealChannelPromise::Private>(
          __func__);
  mOpenPromise->Resolve(
      OpenPromiseSucceededType({std::move(aStreamFilterEndpoints),
                                aRedirectFlags, aLoadFlags, promise}),
      __func__);

  // There is no way we could come back here if the promise had been resolved
  // previously. But for clarity and to avoid all doubt, we set this boolean to
  // true.
  mOpenPromiseResolved = true;

  return promise;
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
      if (*aDestinationProcess) {
        dom::ContentParent* cp =
            dom::ContentProcessManager::GetSingleton()->GetContentProcessById(
                ContentParentId(*aDestinationProcess));
        if (cp) {
          pid = cp->OtherPid();
        }
      } else {
        pid = 0;
      }
    }

    for (StreamFilterRequest& request : mStreamFilterRequests) {
      if (!pid) {
        request.mPromise->Reject(false, __func__);
        request.mPromise = nullptr;
        continue;
      }
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
  if (mIsDocumentLoad || aDestinationProcess) {
    newLoadFlags |= nsIChannel::LOAD_DOCUMENT_URI;
  }
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
          GetCurrentSerialEventTarget(), __func__,
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

void DocumentLoadListener::MaybeReportBlockedByURLClassifier(nsresult aStatus) {
  auto* browsingContext = GetDocumentBrowsingContext();
  if (!browsingContext || browsingContext->IsTop() ||
      !StaticPrefs::privacy_trackingprotection_testing_report_blocked_node()) {
    return;
  }

  if (!UrlClassifierFeatureFactory::IsClassifierBlockingErrorCode(aStatus)) {
    return;
  }

  RefPtr<WindowGlobalParent> parent = browsingContext->GetParentWindowContext();
  if (parent) {
    Unused << parent->SendAddBlockedFrameNodeByClassifier(browsingContext);
  }
}

bool DocumentLoadListener::DocShellWillDisplayContent(nsresult aStatus) {
  if (NS_SUCCEEDED(aStatus)) {
    return true;
  }

  // Always return errored loads to the <object> or <embed> element's process,
  // as load errors will not be rendered as documents.
  if (!mIsDocumentLoad) {
    return false;
  }

  // nsDocShell attempts urifixup on some failure types,
  // but also of those also display an error page if we don't
  // succeed with fixup, so we don't need to check for it
  // here.

  auto* loadingContext = GetLoadingBrowsingContext();

  bool isInitialDocument = true;
  if (WindowGlobalParent* currentWindow =
          loadingContext->GetCurrentWindowGlobal()) {
    isInitialDocument = currentWindow->IsInitialDocument();
  }

  nsresult rv = nsDocShell::FilterStatusForErrorPage(
      aStatus, mChannel, mLoadStateLoadType, loadingContext->IsTop(),
      loadingContext->GetUseErrorPages(), isInitialDocument, nullptr);

  // If filtering returned a failure code, then an error page will
  // be display for that code, so return true;
  return NS_FAILED(rv);
}

bool DocumentLoadListener::MaybeHandleLoadErrorWithURIFixup(nsresult aStatus) {
  auto* bc = GetDocumentBrowsingContext();
  if (!bc) {
    return false;
  }

  nsCOMPtr<nsIInputStream> newPostData;
  nsCOMPtr<nsIURI> newURI = nsDocShell::AttemptURIFixup(
      mChannel, aStatus, mOriginalUriString, mLoadStateLoadType, bc->IsTop(),
      mLoadStateInternalLoadFlags &
          nsDocShell::INTERNAL_LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP,
      bc->UsePrivateBrowsing(), true, getter_AddRefs(newPostData));
  if (!newURI) {
    return false;
  }

  // If we got a new URI, then we should initiate a load with that.
  // Notify the listeners that this load is complete (with a code that
  // won't trigger an error page), and then start the new one.
  DisconnectListeners(NS_BINDING_ABORTED, NS_BINDING_ABORTED);

  RefPtr<nsDocShellLoadState> loadState = new nsDocShellLoadState(newURI);
  nsCOMPtr<nsILoadInfo> loadInfo = mChannel->LoadInfo();

  nsCOMPtr<nsIContentSecurityPolicy> cspToInherit = loadInfo->GetCspToInherit();
  loadState->SetCsp(cspToInherit);

  nsCOMPtr<nsIPrincipal> triggeringPrincipal = loadInfo->TriggeringPrincipal();
  loadState->SetTriggeringPrincipal(triggeringPrincipal);

  loadState->SetPostDataStream(newPostData);

  bc->LoadURI(loadState, false);
  return true;
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

  // Enforce CSP frame-ancestors and x-frame-options checks which
  // might cancel the channel.
  nsContentSecurityUtils::PerformCSPFrameAncestorAndXFOCheck(mChannel);

  // HTTPS-Only Mode tries to upgrade connections to https. Once loading
  // is in progress we set that flag so that timeout counter measures
  // do not kick in.
  if (httpChannel) {
    nsCOMPtr<nsILoadInfo> loadInfo = httpChannel->LoadInfo();
    bool isPrivateWin = loadInfo->GetOriginAttributes().mPrivateBrowsingId > 0;
    if (nsHTTPSOnlyUtils::IsHttpsOnlyModeEnabled(isPrivateWin)) {
      uint32_t httpsOnlyStatus = loadInfo->GetHttpsOnlyStatus();
      httpsOnlyStatus |= nsILoadInfo::HTTPS_ONLY_TOP_LEVEL_LOAD_IN_PROGRESS;
      loadInfo->SetHttpsOnlyStatus(httpsOnlyStatus);
    }
  }

  auto* loadingContext = GetLoadingBrowsingContext();
  if (!loadingContext || loadingContext->IsDiscarded()) {
    DisconnectListeners(NS_ERROR_UNEXPECTED, NS_ERROR_UNEXPECTED);
    return NS_ERROR_UNEXPECTED;
  }

  // Generally we want to switch to a real channel even if the request failed,
  // since the listener might want to access protocol-specific data (like http
  // response headers) in its error handling.
  // An exception to this is when nsExtProtocolChannel handled the request and
  // returned NS_ERROR_NO_CONTENT, since creating a real one in the content
  // process will attempt to handle the URI a second time.
  nsresult status = NS_OK;
  aRequest->GetStatus(&status);
  if (status == NS_ERROR_NO_CONTENT) {
    DisconnectListeners(status, status);
    return NS_OK;
  }

  // If this was a failed load and we want to try fixing the uri, then
  // this will initiate a new load (and disconnect this one), and we don't
  // need to do anything else.
  if (MaybeHandleLoadErrorWithURIFixup(status)) {
    return NS_OK;
  }

  mStreamListenerFunctions.AppendElement(StreamListenerFunction{
      VariantIndex<0>{}, OnStartRequestParams{aRequest}});

  if (mOpenPromiseResolved || mInitiatedRedirectToRealChannel) {
    // I we have already resolved the promise, there's no point to continue
    // attempting a process switch or redirecting to the real channel.
    // We can also have multiple calls to OnStartRequest when dealing with
    // multi-part content, but only want to redirect once.
    return NS_OK;
  }

  mChannel->Suspend();

  mInitiatedRedirectToRealChannel = true;

  MaybeReportBlockedByURLClassifier(status);

  // Determine if a new process needs to be spawned. If it does, this will
  // trigger a cross process switch, and we should hold off on redirecting to
  // the real channel.
  // If the channel has failed, and the docshell isn't going to display an
  // error page for that failure, then don't allow process switching, since
  // we just want to keep our existing document.
  bool willBeRemote = false;
  if (!DocShellWillDisplayContent(status) ||
      !MaybeTriggerProcessSwitch(&willBeRemote)) {
    if (!mSupportsRedirectToRealChannel) {
      // If the existing process is right for this load, but the bridge doesn't
      // support redirects, then we need to do it manually, by faking a process
      // switch.
      mDoingProcessSwitch = true;

      // If we're not going to process switch, then we must have an existing
      // window global, right?
      MOZ_ASSERT(loadingContext->GetCurrentWindowGlobal());

      RefPtr<BrowserParent> browserParent =
          loadingContext->GetCurrentWindowGlobal()->GetBrowserParent();

      // XXX(anny) This is currently a dead code path because parent-controlled
      // DC pref is off. When we enable the pref, we might get extra STATE_START
      // progress events

      // Notify the docshell that it should load using the newly connected
      // channel
      browserParent->ResumeLoad(mLoadIdentifier);

      // Use the current process ID to run the 'process switch' path and connect
      // the channel into the current process.
      TriggerRedirectToRealChannel(Some(loadingContext->OwnerProcessId()));
    } else {
      TriggerRedirectToRealChannel(Nothing());
    }

    // If we're not switching, then check if we're currently remote.
    if (loadingContext->GetContentParent()) {
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
    DisconnectListeners(NS_BINDING_RETARGETED, NS_OK);
    return NS_OK;
  }
  mStreamListenerFunctions.AppendElement(StreamListenerFunction{
      VariantIndex<3>{}, OnAfterLastPartParams{aStatus}});
  mIsFinished = true;
  return NS_OK;
}

NS_IMETHODIMP
DocumentLoadListener::GetInterface(const nsIID& aIID, void** result) {
  RefPtr<CanonicalBrowsingContext> browsingContext =
      GetLoadingBrowsingContext();
  if (aIID.Equals(NS_GET_IID(nsILoadContext)) && browsingContext) {
    browsingContext.forget(result);
    return NS_OK;
  }

  return QueryInterface(aIID, result);
}

////////////////////////////////////////////////////////////////////////////////
// nsIParentChannel
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
DocumentLoadListener::SetParentListener(
    mozilla::net::ParentChannelListener* listener) {
  // We don't need this (do we?)
  return NS_OK;
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
  MOZ_ASSERT_UNREACHABLE("This method is unused");
  return NS_OK;
}

NS_IMETHODIMP
DocumentLoadListener::GetRemoteType(nsACString& aRemoteType) {
  // FIXME: The remote type here should be pulled from the remote process used
  // to create this DLL, not from the current `browsingContext`.
  RefPtr<CanonicalBrowsingContext> browsingContext =
      GetDocumentBrowsingContext();
  if (!browsingContext) {
    return NS_ERROR_UNEXPECTED;
  }

  ErrorResult error;
  browsingContext->GetCurrentRemoteType(aRemoteType, error);
  if (error.Failed()) {
    aRemoteType = NOT_REMOTE_TYPE;
  }
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIChannelEventSink
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
DocumentLoadListener::AsyncOnChannelRedirect(
    nsIChannel* aOldChannel, nsIChannel* aNewChannel, uint32_t aFlags,
    nsIAsyncVerifyRedirectCallback* aCallback) {
  LOG(("DocumentLoadListener::AsyncOnChannelRedirect [this=%p flags=%" PRIu32
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

  // If HTTPS-Only mode is enabled, we need to check whether the exception-flag
  // needs to be removed or set, by asking the PermissionManager.
  nsHTTPSOnlyUtils::TestSitePermissionAndPotentiallyAddExemption(mChannel);

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

  if (GetDocumentBrowsingContext()) {
    if (mLoadingSessionHistoryInfo) {
      mLoadingSessionHistoryInfo =
          GetDocumentBrowsingContext()
              ->ReplaceLoadingSessionHistoryEntryForLoad(
                  mLoadingSessionHistoryInfo.get(), aNewChannel);
    }
    if (!net::ChannelIsPost(aOldChannel)) {
      AddURIVisit(aOldChannel, 0);

      nsCOMPtr<nsIURI> oldURI;
      aOldChannel->GetURI(getter_AddRefs(oldURI));
      nsDocShell::SaveLastVisit(aNewChannel, oldURI, aFlags);
    }
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

  RefPtr<MozPromise<bool, bool, false>> promise;
  RefPtr<CanonicalBrowsingContext> bc =
      mParentChannelListener->GetBrowsingContext();
  nsCOMPtr<nsIWidget> widget =
      bc ? bc->GetParentProcessWidgetContaining() : nullptr;
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
        GetCurrentSerialEventTarget(), __func__,
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

NS_IMETHODIMP DocumentLoadListener::OnProgress(nsIRequest* aRequest,
                                               int64_t aProgress,
                                               int64_t aProgressMax) {
  return NS_OK;
}

NS_IMETHODIMP DocumentLoadListener::OnStatus(nsIRequest* aRequest,
                                             nsresult aStatus,
                                             const char16_t* aStatusArg) {
  nsCOMPtr<nsIChannel> channel = mChannel;
  nsCOMPtr<nsIWebProgress> webProgress =
      new RemoteWebProgress(mLoadStateLoadType, true, true);

  RefPtr<BrowsingContextWebProgress> topWebProgress =
      WebProgressForBrowsingContext(GetTopBrowsingContext());
  const nsString message(aStatusArg);

  if (topWebProgress) {
    NS_DispatchToMainThread(
        NS_NewRunnableFunction("DocumentLoadListener::OnStatus", [=]() {
          topWebProgress->OnStatusChange(webProgress, channel, aStatus,
                                         message.get());
        }));
  }
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla

#undef LOG
