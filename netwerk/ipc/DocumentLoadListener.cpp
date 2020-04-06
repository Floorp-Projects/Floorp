/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocumentLoadListener.h"
#include "mozilla/AntiTrackingUtils.h"
#include "mozilla/ContentBlockingAllowList.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/MozPromiseInlines.h"  // For MozPromise::FromDomPromise
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/ClientChannelHelper.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentProcessManager.h"
#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/dom/ServiceWorkerManager.h"
#include "mozilla/net/CookieJarSettings.h"
#include "mozilla/net/HttpChannelParent.h"
#include "mozilla/net/RedirectChannelRegistrar.h"
#include "nsDocShell.h"
#include "nsDocShellLoadState.h"
#include "nsHttpChannel.h"
#include "nsISecureBrowserUI.h"
#include "nsRedirectHistoryEntry.h"
#include "nsSerializationHelper.h"
#include "nsIPrompt.h"
#include "nsIWindowWatcher.h"
#include "nsIURIContentListener.h"
#include "nsWebNavigationInfo.h"
#include "nsURILoader.h"
#include "nsIStreamConverterService.h"
#include "nsExternalHelperAppService.h"
#include "nsCExternalHandlerService.h"
#include "nsMimeTypes.h"
#include "nsIViewSourceChannel.h"
#include "nsIOService.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/StaticPrefs_security.h"
#include "nsIBrowser.h"

mozilla::LazyLogModule gDocumentChannelLog("DocumentChannel");
#define LOG(fmt) MOZ_LOG(gDocumentChannelLog, mozilla::LogLevel::Verbose, fmt)

using namespace mozilla::dom;

namespace mozilla {
namespace net {

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
    rv = streamConvService->ConvertedType(mContentType, str);
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
    CanonicalBrowsingContext* aBrowsingContext, nsILoadContext* aLoadContext,
    ADocumentChannelBridge* aBridge)
    : mLoadContext(aLoadContext) {
  LOG(("DocumentLoadListener ctor [this=%p]", this));
  mParentChannelListener = new ParentChannelListener(
      this, aBrowsingContext, aLoadContext->UsePrivateBrowsing());
  mDocumentChannelBridge = aBridge;
}

DocumentLoadListener::~DocumentLoadListener() {
  LOG(("DocumentLoadListener dtor [this=%p]", this));
}

already_AddRefed<LoadInfo> DocumentLoadListener::CreateLoadInfo(
    CanonicalBrowsingContext* aBrowsingContext, nsDocShellLoadState* aLoadState,
    uint64_t aOuterWindowId) {
  OriginAttributes attrs;
  mLoadContext->GetOriginAttributes(attrs);

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

  RefPtr<LoadInfo> loadInfo =
      new LoadInfo(aBrowsingContext, aLoadState->TriggeringPrincipal(), attrs,
                   aOuterWindowId, securityFlags, sandboxFlags);
  return loadInfo.forget();
}

already_AddRefed<WindowGlobalParent> GetParentEmbedderWindowGlobal(
    CanonicalBrowsingContext* aBrowsingContext) {
  RefPtr<WindowGlobalParent> parent =
      aBrowsingContext->GetEmbedderWindowGlobal();
  if (parent && parent->BrowsingContext() == aBrowsingContext->GetParent()) {
    return parent.forget();
  }
  return nullptr;
}

// parent-process implementation of
// nsGlobalWindowOuter::GetTopExcludingExtensionAccessibleContentFrames
already_AddRefed<WindowGlobalParent>
GetTopWindowExcludingExtensionAccessibleContentFrames(
    CanonicalBrowsingContext* aBrowsingContext, nsIURI* aURIBeingLoaded) {
  CanonicalBrowsingContext* bc = aBrowsingContext;
  RefPtr<WindowGlobalParent> prev;
  while (RefPtr<WindowGlobalParent> parent =
             GetParentEmbedderWindowGlobal(bc)) {
    CanonicalBrowsingContext* parentBC = parent->BrowsingContext();

    nsIPrincipal* parentPrincipal = parent->DocumentPrincipal();
    nsIURI* uri = prev ? prev->GetDocumentURI() : aURIBeingLoaded;

    // If the new parent has permission to load the current page, we're
    // at a moz-extension:// frame which has a host permission that allows
    // it to load the document that we've loaded.  In that case, stop at
    // this frame and consider it the top-level frame.
    if (uri &&
        BasePrincipal::Cast(parentPrincipal)->AddonAllowsLoad(uri, true)) {
      break;
    }

    bc = parentBC;
    prev = parent;
  }
  if (!prev) {
    prev = bc->GetCurrentWindowGlobal();
  }
  return prev.forget();
}

bool DocumentLoadListener::Open(
    nsDocShellLoadState* aLoadState, class LoadInfo* aLoadInfo,
    nsLoadFlags aLoadFlags, uint32_t aCacheKey, const uint64_t& aChannelId,
    const TimeStamp& aAsyncOpenTime, nsDOMNavigationTiming* aTiming,
    Maybe<ClientInfo>&& aInfo, uint64_t aOuterWindowId, nsresult* aRv) {
  LOG(("DocumentLoadListener Open [this=%p, uri=%s]", this,
       aLoadState->URI()->GetSpecOrDefault().get()));
  RefPtr<CanonicalBrowsingContext> browsingContext =
      mParentChannelListener->GetBrowsingContext();

  OriginAttributes attrs;
  mLoadContext->GetOriginAttributes(attrs);

  // If this is a top-level load, then rebuild the LoadInfo from scratch,
  // since the goal is to be able to initiate loads in the parent, where the
  // content process won't have provided us with an existing one.
  // TODO: Handle TYPE_SUBDOCUMENT LoadInfo construction, and stop passing
  // aLoadInfo across IPC.
  RefPtr<LoadInfo> loadInfo = aLoadInfo;
  if (!browsingContext->GetParent()) {
    // If we're a top level load, then we should have not got an existing
    // LoadInfo, or if we did, it should be TYPE_DOCUMENT.
    MOZ_ASSERT(!aLoadInfo || aLoadInfo->InternalContentPolicyType() ==
                                 nsIContentPolicy::TYPE_DOCUMENT);
    loadInfo = CreateLoadInfo(browsingContext, aLoadState, aOuterWindowId);
  }

  if (!nsDocShell::CreateAndConfigureRealChannelForLoadState(
          browsingContext, aLoadState, loadInfo, mParentChannelListener,
          nullptr, attrs, aLoadFlags, aCacheKey, *aRv,
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

      // Update the IsOnContentBlockingAllowList flag in the CookieJarSettings
      // if this is a top level loading. For sub-document loading, this flag
      // would inherit from the parent.
      nsCOMPtr<nsICookieJarSettings> cookieJarSettings;
      Unused << loadInfo->GetCookieJarSettings(
          getter_AddRefs(cookieJarSettings));
      net::CookieJarSettings::Cast(cookieJarSettings)
          ->UpdateIsOnContentBlockingAllowList(mChannel);
    } else if (RefPtr<WindowGlobalParent> topWindow =
                   GetTopWindowExcludingExtensionAccessibleContentFrames(
                       browsingContext, uriBeingLoaded)) {
      nsCOMPtr<nsIPrincipal> topWindowPrincipal =
          topWindow->DocumentPrincipal();
      if (topWindowPrincipal && !topWindowPrincipal->GetIsNullPrincipal()) {
        topWindowPrincipal->GetURI(getter_AddRefs(topWindowURI));
      }
    }
    httpBaseChannel->SetTopWindowURI(topWindowURI);
  }

  Unused << loadInfo->SetHasStoragePermission(
      AntiTrackingUtils::HasStoragePermissionInParent(mChannel));

  nsCOMPtr<nsIIdentChannel> identChannel = do_QueryInterface(mChannel);
  if (identChannel) {
    Unused << identChannel->SetChannelId(aChannelId);
  }

  RefPtr<nsHttpChannel> httpChannelImpl = do_QueryObject(mChannel);
  if (httpChannelImpl) {
    httpChannelImpl->SetWarningReporter(this);
  }

  nsCOMPtr<nsITimedChannel> timedChannel = do_QueryInterface(mChannel);
  if (timedChannel) {
    timedChannel->SetAsyncOpen(aAsyncOpenTime);
  }

  // nsViewSourceChannel normally replaces the nsIRequest passed to
  // OnStart/StopRequest with itself. We don't need this, and instead
  // we want the original request so that we get different ones for
  // each part of a multipart channel.
  if (nsCOMPtr<nsIViewSourceChannel> viewSourceChannel =
          do_QueryInterface(mChannel)) {
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

  *aRv = mChannel->AsyncOpen(openInfo);
  if (NS_FAILED(*aRv)) {
    mParentChannelListener = nullptr;
    return false;
  }

  mChannelCreationURI = aLoadState->URI();
  mLoadStateLoadFlags = aLoadState->LoadFlags();
  mLoadStateLoadType = aLoadState->LoadType();
  mTiming = aTiming;
  mSrcdocData = aLoadState->SrcdocData();
  mBaseURI = aLoadState->BaseURI();
  return true;
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
}

void DocumentLoadListener::Cancel(const nsresult& aStatusCode) {
  LOG(
      ("DocumentLoadListener Cancel [this=%p, "
       "aStatusCode=%" PRIx32 " ]",
       this, static_cast<uint32_t>(aStatusCode)));
  if (mChannel && !mDoingProcessSwitch) {
    mChannel->Cancel(aStatusCode);
  }
}

void DocumentLoadListener::DisconnectChildListeners(nsresult aStatus,
                                                    nsresult aLoadGroupStatus) {
  LOG(
      ("DocumentLoadListener DisconnectChildListener [this=%p, "
       "aStatus=%" PRIx32 " aLoadGroupStatus=%" PRIx32 " ]",
       this, static_cast<uint32_t>(aStatus),
       static_cast<uint32_t>(aLoadGroupStatus)));
  if (mDocumentChannelBridge) {
    mDocumentChannelBridge->DisconnectChildListeners(aStatus, aLoadGroupStatus);
  }
  DocumentChannelBridgeDisconnected();
}

void DocumentLoadListener::RedirectToRealChannelFinished(nsresult aRv) {
  LOG(
      ("DocumentLoadListener RedirectToRealChannelFinished [this=%p, "
       "aRv=%" PRIx32 " ]",
       this, static_cast<uint32_t>(aRv)));
  if (NS_FAILED(aRv)) {
    FinishReplacementChannelSetup(false);
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
    FinishReplacementChannelSetup(false);
    return;
  }

  nsCOMPtr<nsIParentRedirectingChannel> redirectingParent =
      do_QueryInterface(redirectParentChannel);
  if (!redirectingParent) {
    // Continue verification procedure if redirecting to non-Http protocol
    FinishReplacementChannelSetup(true);
    return;
  }

  // Ask redirected channel if verification can proceed.
  // ReadyToVerify will be invoked when redirected channel is ready.
  redirectingParent->ContinueVerification(this);
  return;
}

NS_IMETHODIMP
DocumentLoadListener::ReadyToVerify(nsresult aResultCode) {
  FinishReplacementChannelSetup(NS_SUCCEEDED(aResultCode));
  return NS_OK;
}

void DocumentLoadListener::FinishReplacementChannelSetup(bool aSucceeded) {
  LOG(
      ("DocumentLoadListener FinishReplacementChannelSetup [this=%p, "
       "aSucceeded=%d]",
       this, aSucceeded));
  nsresult rv;

  if (mDoingProcessSwitch) {
    DisconnectChildListeners(NS_BINDING_ABORTED, NS_BINDING_ABORTED);
  }

  nsCOMPtr<nsIParentChannel> redirectChannel;
  if (mRedirectChannelId) {
    nsCOMPtr<nsIRedirectChannelRegistrar> registrar =
        RedirectChannelRegistrar::GetOrCreate();
    MOZ_ASSERT(registrar);

    rv = registrar->GetParentChannel(mRedirectChannelId,
                                     getter_AddRefs(redirectChannel));
    if (NS_FAILED(rv) || !redirectChannel) {
      // Redirect might get canceled before we got AsyncOnChannelRedirect
      nsCOMPtr<nsIChannel> newChannel;
      rv = registrar->GetRegisteredChannel(mRedirectChannelId,
                                           getter_AddRefs(newChannel));
      MOZ_ASSERT(newChannel, "Already registered channel not found");

      if (NS_SUCCEEDED(rv)) {
        newChannel->Cancel(NS_BINDING_ABORTED);
      }
    }
    // Release all previously registered channels, they are no longer needed to
    // be kept in the registrar from this moment.
    registrar->DeregisterChannels(mRedirectChannelId);

    mRedirectChannelId = 0;
  }

  if (!redirectChannel) {
    aSucceeded = false;
  }

  if (!aSucceeded) {
    if (redirectChannel) {
      redirectChannel->Delete();
    }
    mChannel->Resume();
    return;
  }

  MOZ_ASSERT(
      !SameCOMIdentity(redirectChannel, static_cast<nsIParentChannel*>(this)));

  Delete();
  if (!mIsFinished) {
    mParentChannelListener->SetListenerAfterRedirect(redirectChannel);
  }
  redirectChannel->SetParentListener(mParentChannelListener);

  // We stored the values from all nsIParentChannel functions called since we
  // couldn't handle them. Copy them across to the real channel since it should
  // know what to do.
  for (auto& variant : mIParentChannelFunctions) {
    variant.match(
        [redirectChannel](const nsIHttpChannel::FlashPluginState& aState) {
          redirectChannel->NotifyFlashPluginStateChanged(aState);
        },
        [redirectChannel](const ClassifierMatchedInfoParams& aParams) {
          redirectChannel->SetClassifierMatchedInfo(
              aParams.mList, aParams.mProvider, aParams.mFullHash);
        },
        [redirectChannel](const ClassifierMatchedTrackingInfoParams& aParams) {
          redirectChannel->SetClassifierMatchedTrackingInfo(
              aParams.mLists, aParams.mFullHashes);
        },
        [redirectChannel](const ClassificationFlagsParams& aParams) {
          redirectChannel->NotifyClassificationFlags(
              aParams.mClassificationFlags, aParams.mIsThirdParty);
        });
  }

  RefPtr<HttpChannelParent> httpParent = do_QueryObject(redirectChannel);
  if (httpParent) {
    RefPtr<HttpChannelSecurityWarningReporter> reporter = httpParent;
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

  ResumeSuspendedChannel(redirectChannel);
}

void DocumentLoadListener::ResumeSuspendedChannel(
    nsIStreamListener* aListener) {
  LOG(("DocumentLoadListener ResumeSuspendedChannel [this=%p]", this));
  RefPtr<nsHttpChannel> httpChannel = do_QueryObject(mChannel);
  if (httpChannel) {
    httpChannel->SetApplyConversion(mOldApplyConversion);
  }

  // If we failed to suspend the channel, then we might have received
  // some messages while the redirected was being handled.
  // Manually send them on now.
  nsTArray<StreamListenerFunction> streamListenerFunctions =
      std::move(mStreamListenerFunctions);
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
}

void DocumentLoadListener::SerializeRedirectData(
    RedirectToRealChannelArgs& aArgs, bool aIsCrossProcess,
    uint32_t aRedirectFlags, uint32_t aLoadFlags) {
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

  RefPtr<nsHttpChannel> baseChannel = do_QueryObject(mChannel);
  nsCOMPtr<nsILoadInfo> redirectLoadInfo;
  if (baseChannel) {
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

  // Register the new channel and obtain id for it
  nsCOMPtr<nsIRedirectChannelRegistrar> registrar =
      RedirectChannelRegistrar::GetOrCreate();
  MOZ_ASSERT(registrar);
  nsresult rv = registrar->RegisterChannel(mChannel, &mRedirectChannelId);
  NS_ENSURE_SUCCESS_VOID(rv);
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
                 .Serialize());
  }

  uint32_t contentDispositionTemp;
  rv = mChannel->GetContentDisposition(&contentDispositionTemp);
  if (NS_SUCCEEDED(rv)) {
    aArgs.contentDisposition() = Some(contentDispositionTemp);
  }

  nsString contentDispositionFilenameTemp;
  rv = mChannel->GetContentDispositionFilename(contentDispositionFilenameTemp);
  if (NS_SUCCEEDED(rv)) {
    aArgs.contentDispositionFilename() = Some(contentDispositionFilenameTemp);
  }

  aArgs.newLoadFlags() = aLoadFlags;
  aArgs.redirectFlags() = aRedirectFlags;
  aArgs.redirects() = mRedirects;
  aArgs.redirectIdentifier() = mCrossProcessRedirectIdentifier;
  aArgs.properties() = do_QueryObject(mChannel);
  nsCOMPtr<nsIURI> previousURI;
  uint32_t previousFlags = 0;
  nsDocShell::ExtractLastVisit(mChannel, getter_AddRefs(previousURI),
                               &previousFlags);
  aArgs.lastVisitInfo() = LastVisitInfo{previousURI, previousFlags};
  aArgs.srcdocData() = mSrcdocData;
  aArgs.baseUri() = mBaseURI;
  aArgs.loadStateLoadFlags() = mLoadStateLoadFlags;
  aArgs.loadStateLoadType() = mLoadStateLoadType;
}

bool DocumentLoadListener::MaybeTriggerProcessSwitch() {
  MOZ_DIAGNOSTIC_ASSERT(!mDoingProcessSwitch,
                        "Already in the middle of switching?");
  MOZ_DIAGNOSTIC_ASSERT(mChannel);
  MOZ_DIAGNOSTIC_ASSERT(mParentChannelListener);

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

  // We currently can't switch processes for toplevel loads unless they're
  // loaded within a browser tab. We also enforce this for non-toplevel tabs, as
  // otherwise cross-origin subframes could get out of sync.
  // FIXME: Ideally we won't do this in the future.
  Element* browserElement = browsingContext->Top()->GetEmbedderElement();
  if (!browserElement) {
    LOG(("Process Switch Abort: cannot get browser element"));
    return false;
  }
  nsCOMPtr<nsIBrowser> browser = browserElement->AsBrowser();
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

  // Get information about the current document loaded in our BrowsingContext.
  nsCOMPtr<nsIPrincipal> currentPrincipal;
  if (RefPtr<WindowGlobalParent> wgp =
          browsingContext->GetCurrentWindowGlobal()) {
    currentPrincipal = wgp->DocumentPrincipal();
  }
  RefPtr<ContentParent> currentProcess = browsingContext->GetContentParent();
  if (!currentProcess) {
    LOG(("Process Switch Abort: frame currently not remote"));
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

  if (resultPrincipal->IsSystemPrincipal()) {
    LOG(("Process Switch Abort: cannot switch process for system principal"));
    return false;
  }

  // Determine our COOP status, which will be used to determine our preferred
  // remote type.
  bool isCOOPSwitch = HasCrossOriginOpenerPolicyMismatch();
  nsILoadInfo::CrossOriginOpenerPolicy coop =
      nsILoadInfo::OPENER_POLICY_UNSAFE_NONE;
  if (RefPtr<nsHttpChannel> httpChannel = do_QueryObject(mChannel)) {
    MOZ_ALWAYS_SUCCEEDS(httpChannel->GetCrossOriginOpenerPolicy(&coop));
  }

  nsAutoString preferredRemoteType(currentProcess->GetRemoteType());
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
  MOZ_DIAGNOSTIC_ASSERT(!preferredRemoteType.IsEmpty(),
                        "Unexpected empty remote type!");

  LOG(
      ("DocumentLoadListener GetRemoteTypeForPrincipal "
       "[this=%p, currentProcess=%s, preferredRemoteType=%s]",
       this, NS_ConvertUTF16toUTF8(currentProcess->GetRemoteType()).get(),
       NS_ConvertUTF16toUTF8(preferredRemoteType).get()));

  nsAutoString remoteType;
  rv = browser->GetRemoteTypeForPrincipal(
      resultPrincipal, browsingContext->UseRemoteTabs(),
      browsingContext->UseRemoteSubframes(), preferredRemoteType,
      currentPrincipal, browsingContext->GetParent(), remoteType);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG(("Process Switch Abort: getRemoteTypeForPrincipal threw an exception"));
    return false;
  }

  // Check if a process switch is needed.
  if (currentProcess->GetRemoteType() == remoteType && !isCOOPSwitch) {
    LOG(("Process Switch Abort: type (%s) is compatible",
         NS_ConvertUTF16toUTF8(remoteType).get()));
    return false;
  }
  if (NS_WARN_IF(remoteType.IsEmpty())) {
    LOG(("Process Switch Abort: non-remote target process"));
    return false;
  }

  LOG(("Process Switch: Changing Remoteness from '%s' to '%s'",
       NS_ConvertUTF16toUTF8(currentProcess->GetRemoteType()).get(),
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

RefPtr<PDocumentChannelParent::RedirectToRealChannelPromise>
DocumentLoadListener::RedirectToRealChannel(
    uint32_t aRedirectFlags, uint32_t aLoadFlags,
    const Maybe<uint64_t>& aDestinationProcess) {
  LOG(("DocumentLoadListener RedirectToRealChannel [this=%p]", this));
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
                          aLoadFlags);
    if (mTiming) {
      mTiming->Anonymize(args.uri());
      args.timing() = Some(std::move(mTiming));
    }

    return cp->SendCrossProcessRedirect(args);
  }
  MOZ_ASSERT(mDocumentChannelBridge);
  return mDocumentChannelBridge->RedirectToRealChannel(aRedirectFlags,
                                                       aLoadFlags);
}

void DocumentLoadListener::TriggerRedirectToRealChannel(
    const Maybe<uint64_t>& aDestinationProcess) {
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

  // If we didn't have any redirects, then we pass the REDIRECT_INTERNAL flag
  // for this channel switch so that it isn't recorded in session history etc.
  // If there were redirect(s), then we want this switch to be recorded as a
  // real one, since we have a new URI.
  uint32_t redirectFlags = 0;
  if (mRedirects.IsEmpty()) {
    redirectFlags = nsIChannelEventSink::REDIRECT_INTERNAL;
  }

  uint32_t newLoadFlags = nsIRequest::LOAD_NORMAL;
  MOZ_ALWAYS_SUCCEEDS(mChannel->GetLoadFlags(&newLoadFlags));
  if (!aDestinationProcess) {
    newLoadFlags |= nsIChannel::LOAD_REPLACE;
  }

  RefPtr<DocumentLoadListener> self = this;
  RedirectToRealChannel(redirectFlags, newLoadFlags, aDestinationProcess)
      ->Then(
          GetCurrentThreadSerialEventTarget(), __func__,
          [self](const nsresult& aResponse) {
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

  // If this is a download, then redirect entirely within the parent.
  // TODO, see bug 1574372.

  if (!mDocumentChannelBridge) {
    return NS_ERROR_UNEXPECTED;
  }

  // Once we initiate a process switch, we ask the child to notify the
  // listeners that we have completed. If the switch promise then gets
  // rejected we also cancel the parent, which results in this being called.
  // We don't need to forward it on though, since the child side is already
  // completed.
  if (mDoingProcessSwitch) {
    return NS_OK;
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
    mDocumentChannelBridge->DisconnectChildListeners(status, status);
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
    httpChannel->SetApplyConversion(false);
  }

  // Determine if a new process needs to be spawned. If it does, this will
  // trigger a cross process switch, and we should hold off on redirecting to
  // the real channel.
  if (status != NS_BINDING_ABORTED && MaybeTriggerProcessSwitch()) {
    return NS_OK;
  }

  TriggerRedirectToRealChannel();

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
  // Only support nsILoadContext if child channel's callbacks did too
  if (aIID.Equals(NS_GET_IID(nsILoadContext)) && mLoadContext) {
    nsCOMPtr<nsILoadContext> copy = mLoadContext;
    copy.forget(result);
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
  // We generally don't want to notify the content process about redirects,
  // so just update our channel and tell the callback that we're good to go.
  mChannel = aNewChannel;

  // We need the original URI of the current channel to use to open the real
  // channel in the content process. Unfortunately we overwrite the original
  // uri of the new channel with the original pre-redirect URI, so grab
  // a copy of it now.
  aNewChannel->GetOriginalURI(getter_AddRefs(mChannelCreationURI));

  // Since we're redirecting away from aOldChannel, we should check if it
  // had a COOP mismatch, since we want the final result for this to
  // include the state of all channels we redirected through.
  RefPtr<nsHttpChannel> httpChannel = do_QueryObject(aOldChannel);
  if (httpChannel) {
    mHasCrossOriginOpenerPolicyMismatch |=
        httpChannel->HasCrossOriginOpenerPolicyMismatch();
  }

  // We don't need to confirm internal redirects or record any
  // history for them, so just immediately verify and return.
  if (aFlags & nsIChannelEventSink::REDIRECT_INTERNAL) {
    aCallback->OnRedirectVerifyCallback(NS_OK);
    return NS_OK;
  } else {
    nsCOMPtr<nsIURI> oldURI;
    aOldChannel->GetURI(getter_AddRefs(oldURI));
    uint32_t responseStatus = 0;
    if (nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aOldChannel)) {
      Unused << httpChannel->GetResponseStatus(&responseStatus);
    }
    mRedirects.AppendElement(DocumentChannelRedirect{
        oldURI, aFlags, responseStatus, net::ChannelIsPost(aOldChannel)});
  }

  if (!mDocumentChannelBridge) {
    return NS_BINDING_ABORTED;
  }

  // Clear out our nsIParentChannel functions, since a normal parent
  // channel would actually redirect and not have those values on the new one.
  // We expect the URI classifier to run on the redirected channel with
  // the new URI and set these again.
  mIParentChannelFunctions.Clear();

  nsCOMPtr<nsILoadInfo> loadInfo = aOldChannel->LoadInfo();

  nsCOMPtr<nsIURI> originalUri;
  nsresult rv = aOldChannel->GetOriginalURI(getter_AddRefs(originalUri));
  if (NS_FAILED(rv)) {
    aOldChannel->Cancel(NS_ERROR_DOM_BAD_URI);
    return rv;
  }

  nsCOMPtr<nsIURI> newUri;
  rv = aNewChannel->GetURI(getter_AddRefs(newUri));
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<ADocumentChannelBridge> bridge = mDocumentChannelBridge;
  auto callback =
      [bridge, loadInfo](
          nsCSPContext* aContext, mozilla::dom::Element* aTriggeringElement,
          nsICSPEventListener* aCSPEventListener, nsIURI* aBlockedURI,
          nsCSPContext::BlockedContentSource aBlockedContentSource,
          nsIURI* aOriginalURI, const nsAString& aViolatedDirective,
          uint32_t aViolatedPolicyIndex, const nsAString& aObserverSubject,
          const nsAString& aSourceFile, const nsAString& aScriptSample,
          uint32_t aLineNum, uint32_t aColumnNum) -> nsresult {
    MOZ_ASSERT(!aTriggeringElement);
    MOZ_ASSERT(!aCSPEventListener);
    MOZ_ASSERT(aSourceFile.IsVoid() || aSourceFile.IsEmpty());
    MOZ_ASSERT(aScriptSample.IsVoid() || aScriptSample.IsEmpty());
    nsCOMPtr<nsIContentSecurityPolicy> cspToInherit =
        loadInfo->GetCspToInherit();

    // The CSPContext normally contains the loading Document (used
    // for targeting events), but this gets lost when serializing across
    // IPDL. We need to know which CSPContext we're serializing, so that
    // we can find the right loading Document on the content process
    // side.
    bool isCspToInherit = (aContext == cspToInherit);
    bridge->CSPViolation(aContext, isCspToInherit, aBlockedURI,
                         aBlockedContentSource, aOriginalURI,
                         aViolatedDirective, aViolatedPolicyIndex,
                         aObserverSubject);
    return NS_OK;
  };

  Maybe<nsresult> cancelCode;
  rv = CSPService::ConsultCSPForRedirect(callback, originalUri, newUri,
                                         loadInfo, cancelCode);

  if (cancelCode) {
    aOldChannel->Cancel(*cancelCode);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  aCallback->OnRedirectVerifyCallback(NS_OK);
  return NS_OK;
}

// This method returns the cached result of running the Cross-Origin-Opener
// policy compare algorithm by calling ComputeCrossOriginOpenerPolicyMismatch
bool DocumentLoadListener::HasCrossOriginOpenerPolicyMismatch() {
  // If we found a COOP mismatch on an earlier channel and then
  // redirected away from that, we should use that result.
  if (mHasCrossOriginOpenerPolicyMismatch) {
    return true;
  }

  RefPtr<nsHttpChannel> httpChannel = do_QueryObject(mChannel);
  if (!httpChannel) {
    // Not an nsHttpChannel assume it's okay to switch.
    return false;
  }

  return httpChannel->HasCrossOriginOpenerPolicyMismatch();
}

}  // namespace net
}  // namespace mozilla

#undef LOG
