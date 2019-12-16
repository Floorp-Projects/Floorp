/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocumentLoadListener.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/MozPromiseInlines.h"  // For MozPromise::FromDomPromise
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/ClientChannelHelper.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentProcessManager.h"
#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/dom/ServiceWorkerManager.h"
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

mozilla::LazyLogModule gDocumentChannelLog("DocumentChannel");
#define LOG(fmt) MOZ_LOG(gDocumentChannelLog, mozilla::LogLevel::Verbose, fmt)

using namespace mozilla::dom;

namespace mozilla {
namespace net {

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
  NS_INTERFACE_MAP_ENTRY(nsIProcessSwitchRequestor)
  NS_INTERFACE_MAP_ENTRY(nsIMultiPartChannelListener)
  NS_INTERFACE_MAP_ENTRY_CONCRETE(DocumentLoadListener)
NS_INTERFACE_MAP_END

DocumentLoadListener::DocumentLoadListener(const PBrowserOrId& aIframeEmbedding,
                                           nsILoadContext* aLoadContext,
                                           PBOverrideStatus aOverrideStatus,
                                           ADocumentChannelBridge* aBridge)
    : mLoadContext(aLoadContext), mPBOverride(aOverrideStatus) {
  LOG(("DocumentLoadListener ctor [this=%p]", this));
  RefPtr<dom::BrowserParent> parent;
  if (aIframeEmbedding.type() == PBrowserOrId::TPBrowserParent) {
    parent =
        static_cast<dom::BrowserParent*>(aIframeEmbedding.get_PBrowserParent());
  }
  mParentChannelListener = new ParentChannelListener(this, parent);
  mDocumentChannelBridge = aBridge;
}

DocumentLoadListener::~DocumentLoadListener() {
  LOG(("DocumentLoadListener dtor [this=%p]", this));
}

bool DocumentLoadListener::Open(
    nsDocShellLoadState* aLoadState, class LoadInfo* aLoadInfo,
    const nsString* aInitiatorType, nsLoadFlags aLoadFlags, uint32_t aLoadType,
    uint32_t aCacheKey, bool aIsActive, bool aIsTopLevelDoc,
    bool aHasNonEmptySandboxingFlags, const Maybe<URIParams>& aTopWindowURI,
    const Maybe<PrincipalInfo>& aContentBlockingAllowListPrincipal,
    const nsString& aCustomUserAgent, const uint64_t& aChannelId,
    const TimeStamp& aAsyncOpenTime, nsresult* aRv) {
  LOG(("DocumentLoadListener Open [this=%p, uri=%s]", this,
       aLoadState->URI()->GetSpecOrDefault().get()));
  if (!nsDocShell::CreateChannelForLoadState(
          aLoadState, aLoadInfo, mParentChannelListener, nullptr,
          aInitiatorType, aLoadFlags, aLoadType, aCacheKey, aIsActive,
          aIsTopLevelDoc, aHasNonEmptySandboxingFlags, *aRv,
          getter_AddRefs(mChannel))) {
    mParentChannelListener = nullptr;
    return false;
  }

  nsDocShell::ConfigureChannel(mChannel, aLoadState, aInitiatorType, aLoadType,
                               aCacheKey, aHasNonEmptySandboxingFlags);

  // Computation of the top window uses the docshell tree, so only
  // works in the source process. We compute it manually and override
  // it so that it gets the right value.
  // This probably isn't fission compatible.
  RefPtr<HttpBaseChannel> httpBaseChannel = do_QueryObject(mChannel, aRv);
  if (httpBaseChannel) {
    nsCOMPtr<nsIURI> topWindowURI = DeserializeURI(aTopWindowURI);
    httpBaseChannel->SetTopWindowURI(topWindowURI);

    if (aContentBlockingAllowListPrincipal) {
      nsCOMPtr<nsIPrincipal> contentBlockingAllowListPrincipal =
          PrincipalInfoToPrincipal(*aContentBlockingAllowListPrincipal);
      httpBaseChannel->SetContentBlockingAllowListPrincipal(
          contentBlockingAllowListPrincipal);
    }

    // The content process docshell can specify a custom override
    // for the user agent value. If we had one, then add it to
    // the header now.
    if (!aCustomUserAgent.IsEmpty()) {
      NS_ConvertUTF16toUTF8 utf8CustomUserAgent(aCustomUserAgent);
      *aRv = httpBaseChannel->SetRequestHeader(NS_LITERAL_CSTRING("User-Agent"),
                                               utf8CustomUserAgent, false);
    }
  }

  nsCOMPtr<nsIPrivateBrowsingChannel> privateChannel =
      do_QueryInterface(mChannel);
  if (mPBOverride != kPBOverride_Unset) {
    privateChannel->SetPrivate(mPBOverride == kPBOverride_Private);
  }

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

  // Setup a ClientChannelHelper to watch for redirects, and copy
  // across any serviceworker related data between channels as needed.
  AddClientChannelHelperInParent(mChannel, GetMainThreadSerialEventTarget());

  *aRv = mChannel->AsyncOpen(mParentChannelListener);
  if (NS_FAILED(*aRv)) {
    mParentChannelListener = nullptr;
    return false;
  }

  mChannelCreationURI = aLoadState->URI();
  mLoadStateLoadFlags = aLoadState->LoadFlags();
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
  if (mChannel && !mDoingProcessSwitch) {
    mChannel->Cancel(aStatusCode);
  }
}

void DocumentLoadListener::Suspend() {
  if (mChannel && !mDoingProcessSwitch) {
    mChannel->Suspend();
  }
}

void DocumentLoadListener::Resume() {
  if (mChannel && !mDoingProcessSwitch) {
    mChannel->Resume();
  }
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
  nsresult rv;

  if (mDoingProcessSwitch && mDocumentChannelBridge) {
    mDocumentChannelBridge->DisconnectChildListeners(NS_BINDING_ABORTED);
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
        },
        [redirectChannel](
            const NotifyChannelClassifierProtectionDisabledParams& aParams) {
          redirectChannel->NotifyChannelClassifierProtectionDisabled(
              aParams.mAcceptedReason);
        },
        [redirectChannel](const NotifyCookieAllowedParams&) {
          redirectChannel->NotifyCookieAllowed();
        },
        [redirectChannel](const NotifyCookieBlockedParams& aParams) {
          redirectChannel->NotifyCookieBlocked(aParams.mRejectedReason);
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

  if (!aIsCrossProcess) {
    // When we're switching into a new process, the destination
    // docshell will create a new initial client source which conflicts
    // with this. We should probably use this one, but need to update
    // docshell to understand that.
    const Maybe<ClientInfo>& reservedClientInfo =
        channelLoadInfo->GetReservedClientInfo();
    if (reservedClientInfo) {
      redirectLoadInfo->SetReservedClientInfo(*reservedClientInfo);
    }
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
    uint32_t loadFlags = 0;
    if (!aIsCrossProcess) {
      loadFlags |= nsIChannel::LOAD_REPLACE;
    }

    aArgs.init() = Some(
        baseChannel
            ->CloneReplacementChannelConfig(
                true, aRedirectFlags,
                HttpBaseChannel::ReplacementReason::DocumentChannel, loadFlags)
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
}

void DocumentLoadListener::TriggerCrossProcessSwitch() {
  MOZ_ASSERT(mRedirectContentProcessIdPromise);
  MOZ_ASSERT(!mDoingProcessSwitch, "Already in the middle of switching?");
  MOZ_ASSERT(NS_IsMainThread());

  LOG(("DocumentLoadListener TriggerCrossProcessSwitch [this=%p]", this));

  mDoingProcessSwitch = true;

  RefPtr<DocumentLoadListener> self = this;
  mRedirectContentProcessIdPromise->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [self, this](uint64_t aCpId) {
        MOZ_ASSERT(mChannel, "Something went wrong, channel got cancelled");
        TriggerRedirectToRealChannel(Some(aCpId));
      },
      [self](nsresult aStatusCode) {
        MOZ_ASSERT(NS_FAILED(aStatusCode), "Status should be error");
        self->RedirectToRealChannelFinished(aStatusCode);
      });
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
          [self](Tuple<nsresult, Maybe<LoadInfoArgs>>&& aResponse) {
            if (NS_SUCCEEDED(Get<0>(aResponse))) {
              nsCOMPtr<nsILoadInfo> newLoadInfo;
              MOZ_ALWAYS_SUCCEEDS(LoadInfoArgsToLoadInfo(
                  Get<1>(aResponse), getter_AddRefs(newLoadInfo)));
              if (newLoadInfo) {
                // Since the old reservedClientInfo might be controlled by a
                // ServiceWorker when opening the channel, we need to update
                // the controlled ClientInfo in ServiceWorkerManager to make
                // sure the same origin subresource load can be intercepted by
                // the ServiceWorker.
                nsCOMPtr<nsILoadInfo> oldLoadInfo;
                self->mChannel->GetLoadInfo(getter_AddRefs(oldLoadInfo));
                MOZ_ASSERT(oldLoadInfo);
                Maybe<ClientInfo> oldClientInfo =
                    oldLoadInfo->GetReservedClientInfo();
                Maybe<ServiceWorkerDescriptor> oldController =
                    oldLoadInfo->GetController();
                Maybe<ClientInfo> newClientInfo =
                    newLoadInfo->GetReservedClientInfo();
                Maybe<ServiceWorkerDescriptor> newController =
                    newLoadInfo->GetController();
                if (oldClientInfo.isSome() && newClientInfo.isSome() &&
                    newController.isSome() && oldController.isSome() &&
                    newController.ref() == oldController.ref()) {
                  RefPtr<ServiceWorkerManager> swMgr =
                      ServiceWorkerManager::GetInstance();
                  MOZ_ASSERT(swMgr);
                  swMgr->UpdateControlledClient(oldClientInfo.ref(),
                                                newClientInfo.ref(),
                                                newController.ref());
                }
                self->mChannel->SetLoadInfo(newLoadInfo);
              }
            }
            self->RedirectToRealChannelFinished(Get<0>(aResponse));
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
    mDocumentChannelBridge->DisconnectChildListeners(NS_ERROR_NO_CONTENT);
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

  // notify "channel-on-may-change-process" observers which is typically
  // SessionStore.jsm. This will determine if a new process needs to be
  // spawned and if so SwitchProcessTo() will be called which will set a
  // ContentProcessIdPromise.
  nsCOMPtr<nsIObserverService> obsService = services::GetObserverService();
  obsService->NotifyObservers(ToSupports(this), "channel-on-may-change-process",
                              nullptr);

  if (mRedirectContentProcessIdPromise) {
    TriggerCrossProcessSwitch();
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
DocumentLoadListener::NotifyChannelClassifierProtectionDisabled(
    uint32_t aAcceptedReason) {
  mIParentChannelFunctions.AppendElement(IParentChannelFunction{
      VariantIndex<4>{},
      NotifyChannelClassifierProtectionDisabledParams{aAcceptedReason}});
  return NS_OK;
}

NS_IMETHODIMP
DocumentLoadListener::NotifyCookieAllowed() {
  mIParentChannelFunctions.AppendElement(
      IParentChannelFunction{VariantIndex<5>{}, NotifyCookieAllowedParams()});
  return NS_OK;
}

NS_IMETHODIMP
DocumentLoadListener::NotifyCookieBlocked(uint32_t aRejectedReason) {
  mIParentChannelFunctions.AppendElement(IParentChannelFunction{
      VariantIndex<6>{}, NotifyCookieBlockedParams{aRejectedReason}});
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
    bool mismatch = false;
    MOZ_ALWAYS_SUCCEEDS(
        httpChannel->HasCrossOriginOpenerPolicyMismatch(&mismatch));
    mHasCrossOriginOpenerPolicyMismatch |= mismatch;
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

  // Currently the CSP code expects to run in the content
  // process so that it can send events. Send a message to
  // our content process to ask CSP if we should allow this
  // redirect, and wait for confirmation.
  nsCOMPtr<nsILoadInfo> loadInfo = aOldChannel->LoadInfo();
  Maybe<LoadInfoArgs> loadInfoArgs;
  MOZ_ALWAYS_SUCCEEDS(ipc::LoadInfoToLoadInfoArgs(loadInfo, &loadInfoArgs));
  MOZ_ASSERT(loadInfoArgs.isSome());

  nsCOMPtr<nsIURI> newUri;
  nsresult rv = aNewChannel->GetURI(getter_AddRefs(newUri));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIAsyncVerifyRedirectCallback> callback(aCallback);
  nsCOMPtr<nsIChannel> oldChannel(aOldChannel);
  mDocumentChannelBridge->ConfirmRedirect(*loadInfoArgs, newUri)
      ->Then(
          GetCurrentThreadSerialEventTarget(), __func__,
          [callback,
           oldChannel](const Tuple<nsresult, Maybe<nsresult>>& aResult) {
            if (Get<1>(aResult)) {
              oldChannel->Cancel(*Get<1>(aResult));
            }
            callback->OnRedirectVerifyCallback(Get<0>(aResult));
          },
          [callback, oldChannel](const mozilla::ipc::ResponseRejectReason) {
            oldChannel->Cancel(NS_ERROR_DOM_BAD_URI);
            callback->OnRedirectVerifyCallback(NS_BINDING_ABORTED);
          });

  // Clear out our nsIParentChannel functions, since a normal parent
  // channel would actually redirect and not have those values on the new one.
  // We expect the URI classifier to run on the redirected channel with
  // the new URI and set these again.
  mIParentChannelFunctions.Clear();
  return NS_OK;
}

//-----------------------------------------------------------------------------
// DocumentLoadListener::nsIProcessSwitchRequestor
//-----------------------------------------------------------------------------

NS_IMETHODIMP DocumentLoadListener::GetChannel(nsIChannel** aChannel) {
  MOZ_ASSERT(mChannel);
  nsCOMPtr<nsIChannel> channel(mChannel);
  channel.forget(aChannel);
  return NS_OK;
}

NS_IMETHODIMP DocumentLoadListener::SwitchProcessTo(
    dom::Promise* aContentProcessIdPromise, uint64_t aIdentifier) {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG(aContentProcessIdPromise);

  mRedirectContentProcessIdPromise =
      ContentProcessIdPromise::FromDomPromise(aContentProcessIdPromise);
  mCrossProcessRedirectIdentifier = aIdentifier;
  return NS_OK;
}

// This method returns the cached result of running the Cross-Origin-Opener
// policy compare algorithm by calling ComputeCrossOriginOpenerPolicyMismatch
NS_IMETHODIMP
DocumentLoadListener::HasCrossOriginOpenerPolicyMismatch(bool* aMismatch) {
  MOZ_ASSERT(aMismatch);

  if (!aMismatch) {
    return NS_ERROR_INVALID_ARG;
  }

  // If we found a COOP mismatch on an earlier channel and then
  // redirected away from that, we should use that result.
  if (mHasCrossOriginOpenerPolicyMismatch) {
    *aMismatch = true;
    return NS_OK;
  }

  RefPtr<nsHttpChannel> httpChannel = do_QueryObject(mChannel);
  if (!httpChannel) {
    // Not an nsHttpChannel assume it's okay to switch.
    *aMismatch = false;
    return NS_OK;
  }

  return httpChannel->HasCrossOriginOpenerPolicyMismatch(aMismatch);
}

NS_IMETHODIMP
DocumentLoadListener::GetCrossOriginOpenerPolicy(
    nsILoadInfo::CrossOriginOpenerPolicy* aPolicy) {
  MOZ_ASSERT(aPolicy);
  if (!aPolicy) {
    return NS_ERROR_INVALID_ARG;
  }

  RefPtr<nsHttpChannel> httpChannel = do_QueryObject(mChannel);
  if (!httpChannel) {
    *aPolicy = nsILoadInfo::OPENER_POLICY_NULL;
    return NS_OK;
  }

  return httpChannel->GetCrossOriginOpenerPolicy(aPolicy);
}

}  // namespace net
}  // namespace mozilla

#undef LOG
