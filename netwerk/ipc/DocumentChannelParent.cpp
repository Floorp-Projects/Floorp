/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocumentChannelParent.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/MozPromiseInlines.h"  // For MozPromise::FromDomPromise
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/ClientChannelHelper.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentProcessManager.h"
#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/net/HttpChannelParent.h"
#include "mozilla/net/RedirectChannelRegistrar.h"
#include "nsDocShell.h"
#include "nsDocShellLoadState.h"
#include "nsHttpChannel.h"
#include "nsIHttpProtocolHandler.h"
#include "nsISecureBrowserUI.h"
#include "nsRedirectHistoryEntry.h"
#include "nsSerializationHelper.h"

using namespace mozilla::dom;

namespace mozilla {
namespace net {

NS_IMPL_ADDREF(DocumentChannelParent)
NS_IMPL_RELEASE(DocumentChannelParent)

NS_INTERFACE_MAP_BEGIN(DocumentChannelParent)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsIParentChannel)
  NS_INTERFACE_MAP_ENTRY(nsIAsyncVerifyRedirectReadyCallback)
  NS_INTERFACE_MAP_ENTRY(nsIChannelEventSink)
  NS_INTERFACE_MAP_ENTRY(nsIProcessSwitchRequestor)
  NS_INTERFACE_MAP_ENTRY_CONCRETE(DocumentChannelParent)
NS_INTERFACE_MAP_END

DocumentChannelParent::DocumentChannelParent(
    const PBrowserOrId& iframeEmbedding, nsILoadContext* aLoadContext,
    PBOverrideStatus aOverrideStatus)
    : mLoadContext(aLoadContext), mPBOverride(aOverrideStatus) {
  if (iframeEmbedding.type() == PBrowserOrId::TPBrowserParent) {
    mBrowserParent =
        static_cast<dom::BrowserParent*>(iframeEmbedding.get_PBrowserParent());
  }
}

bool DocumentChannelParent::Init(const DocumentChannelCreationArgs& aArgs) {
  RefPtr<nsDocShellLoadState> loadState =
      new nsDocShellLoadState(aArgs.loadState());

  RefPtr<LoadInfo> loadInfo;
  nsresult rv = mozilla::ipc::LoadInfoArgsToLoadInfo(Some(aArgs.loadInfo()),
                                                     getter_AddRefs(loadInfo));
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  mListener = new ParentChannelListener(this);

  bool result = nsDocShell::CreateChannelForLoadState(
      loadState, loadInfo, mListener, nullptr,
      aArgs.initiatorType().ptrOr(nullptr), aArgs.loadFlags(), aArgs.loadType(),
      aArgs.cacheKey(), aArgs.isActive(), aArgs.isTopLevelDoc(), rv,
      getter_AddRefs(mChannel));
  if (!result) {
    return SendFailedAsyncOpen(rv);
  }

  nsDocShell::ConfigureChannel(mChannel, loadState,
                               aArgs.initiatorType().ptrOr(nullptr),
                               aArgs.loadType(), aArgs.cacheKey());

  // Computation of the top window uses the docshell tree, so only
  // works in the source process. We compute it manually and override
  // it so that it gets the right value.
  // This probably isn't fission compatible.
  RefPtr<HttpBaseChannel> httpBaseChannel = do_QueryObject(mChannel, &rv);
  if (httpBaseChannel) {
    nsCOMPtr<nsIURI> topWindowURI = DeserializeURI(aArgs.topWindowURI());
    rv = httpBaseChannel->SetTopWindowURI(topWindowURI);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    if (aArgs.contentBlockingAllowListPrincipal()) {
      nsCOMPtr<nsIPrincipal> contentBlockingAllowListPrincipal =
          PrincipalInfoToPrincipal(*aArgs.contentBlockingAllowListPrincipal());
      httpBaseChannel->SetContentBlockingAllowListPrincipal(
          contentBlockingAllowListPrincipal);
    }

    // The content process docshell can specify a custom override
    // for the user agent value. If we had one, then add it to
    // the header now.
    if (!aArgs.customUserAgent().IsEmpty()) {
      NS_ConvertUTF16toUTF8 utf8CustomUserAgent(aArgs.customUserAgent());
      rv = httpBaseChannel->SetRequestHeader(NS_LITERAL_CSTRING("User-Agent"),
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
    Unused << identChannel->SetChannelId(aArgs.channelId());
  }

  RefPtr<nsHttpChannel> httpChannelImpl = do_QueryObject(mChannel);
  if (httpChannelImpl) {
    httpChannelImpl->SetWarningReporter(this);
  }

  // Setup a ClientChannelHelper to watch for redirects, and copy
  // across any serviceworker related data between channels as needed.
  AddClientChannelHelperInParent(mChannel, GetMainThreadSerialEventTarget());

  rv = mChannel->AsyncOpen(mListener);
  if (NS_FAILED(rv)) {
    return SendFailedAsyncOpen(rv);
  }

  mChannelCreationURI = loadState->URI();

  return true;
}

void DocumentChannelParent::ActorDestroy(ActorDestroyReason why) {
  // The nsHttpChannel may have a reference to this parent, release it
  // to avoid circular references.
  RefPtr<nsHttpChannel> httpChannelImpl = do_QueryObject(mChannel);
  if (httpChannelImpl) {
    httpChannelImpl->SetWarningReporter(nullptr);
  }
}

void DocumentChannelParent::CancelChildForProcessSwitch() {
  MOZ_ASSERT(!mDoingProcessSwitch, "Already in the middle of switching?");
  MOZ_ASSERT(NS_IsMainThread());

  mDoingProcessSwitch = true;
  if (CanSend()) {
    Unused << SendCancelForProcessSwitch();
  }
}

bool DocumentChannelParent::RecvCancel(const nsresult& aStatusCode) {
  if (mDoingProcessSwitch) {
    return IPC_OK();
  }

  if (mChannel) {
    mChannel->Cancel(aStatusCode);
  }
  return true;
}

bool DocumentChannelParent::RecvSuspend() {
  if (mChannel) {
    mChannel->Suspend();
  }
  return true;
}

bool DocumentChannelParent::RecvResume() {
  if (mChannel) {
    mChannel->Resume();
  }
  return true;
}

void DocumentChannelParent::RedirectToRealChannelFinished(nsresult aRv) {
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
DocumentChannelParent::ReadyToVerify(nsresult aResultCode) {
  FinishReplacementChannelSetup(NS_SUCCEEDED(aResultCode));
  return NS_OK;
}

void DocumentChannelParent::FinishReplacementChannelSetup(bool aSucceeded) {
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
      nsCOMPtr<nsIChannel> newChannel;
      rv = registrar->GetRegisteredChannel(mRedirectChannelId,
                                           getter_AddRefs(newChannel));
      MOZ_ASSERT(newChannel, "Already registered channel not found");

      if (NS_SUCCEEDED(rv)) {
        newChannel->Cancel(NS_BINDING_ABORTED);
      }
    }
    // Release all previously registered channels, they are no longer needed to be
    // kept in the registrar from this moment.
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
    if (mSuspendedChannel) {
      mChannel->Resume();
    }
    return;
  }

  MOZ_ASSERT(
      !SameCOMIdentity(redirectChannel, static_cast<nsIParentChannel*>(this)));

  Delete();
  if (!mStopRequestValue) {
    mListener->SetListenerAfterRedirect(redirectChannel);
  }
  redirectChannel->SetParentListener(mListener);

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

  if (mSuspendedChannel) {
    nsTArray<OnDataAvailableRequest> pendingRequests =
        std::move(mPendingRequests);
    MOZ_ASSERT(mPendingRequests.IsEmpty());

    nsCOMPtr<nsHttpChannel> httpChannel = do_QueryInterface(mChannel);
    if (httpChannel) {
      httpChannel->SetApplyConversion(mOldApplyConversion);
    }
    rv = redirectChannel->OnStartRequest(mChannel);
    if (NS_FAILED(rv)) {
      mChannel->Cancel(rv);
    }

    // If we failed to suspend the channel, then we might have received
    // some messages while the redirected was being handled.
    // Manually send them on now.
    if (NS_SUCCEEDED(rv) &&
        (!mStopRequestValue || NS_SUCCEEDED(*mStopRequestValue))) {
      for (auto& request : pendingRequests) {
        nsCOMPtr<nsIInputStream> stringStream;
        rv = NS_NewByteInputStream(
            getter_AddRefs(stringStream),
            Span<const char>(request.data.get(), request.count),
            NS_ASSIGNMENT_DEPEND);
        if (NS_SUCCEEDED(rv)) {
          rv = redirectChannel->OnDataAvailable(mChannel, stringStream,
                                                request.offset, request.count);
        }
        if (NS_FAILED(rv)) {
          mChannel->Cancel(rv);
          mStopRequestValue = Some(rv);
          break;
        }
      }
    }

    if (mStopRequestValue) {
      redirectChannel->OnStopRequest(mChannel, *mStopRequestValue);
    }

    mChannel->Resume();
  }
}

void DocumentChannelParent::TriggerCrossProcessSwitch() {
  MOZ_ASSERT(mRedirectContentProcessIdPromise);
  CancelChildForProcessSwitch();
  RefPtr<DocumentChannelParent> self = this;
  mRedirectContentProcessIdPromise->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [self, this](uint64_t aCpId) {
        MOZ_ASSERT(mChannel, "Something went wrong, channel got cancelled");
        TriggerRedirectToRealChannel(mChannel, Some(aCpId),
                                     mCrossProcessRedirectIdentifier);
      },
      [self](nsresult aStatusCode) {
        MOZ_ASSERT(NS_FAILED(aStatusCode), "Status should be error");
        self->RedirectToRealChannelFinished(aStatusCode);
      });
}

void DocumentChannelParent::TriggerRedirectToRealChannel(
    nsIChannel* aChannel, const Maybe<uint64_t>& aDestinationProcess,
    uint64_t aIdentifier) {
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

  // Use the original URI of the current channel, as this is what
  // we'll use to construct the channel in the content process.
  nsCOMPtr<nsIURI> uri = mChannelCreationURI;
  if (!uri) {
    aChannel->GetOriginalURI(getter_AddRefs(uri));
  }

  // I previously used HttpBaseChannel::CloneLoadInfoForRedirect, but that
  // clears the principal to inherit, which fails tests (probably because this
  // 'redirect' is usually just an implementation detail). It's also http only,
  // and aChannel can be anything that we redirected to.
  nsCOMPtr<nsILoadInfo> channelLoadInfo;
  aChannel->GetLoadInfo(getter_AddRefs(channelLoadInfo));

  nsCOMPtr<nsIPrincipal> principalToInherit;
  channelLoadInfo->GetPrincipalToInherit(getter_AddRefs(principalToInherit));

  RefPtr<nsHttpChannel> baseChannel = do_QueryObject(aChannel);
  nsCOMPtr<nsILoadInfo> redirectLoadInfo;
  if (baseChannel) {
    redirectLoadInfo = baseChannel->CloneLoadInfoForRedirect(
        uri, nsIChannelEventSink::REDIRECT_INTERNAL);
    redirectLoadInfo->SetResultPrincipalURI(uri);

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
    sm->GetChannelURIPrincipal(aChannel, getter_AddRefs(uriPrincipal));

    nsCOMPtr<nsIRedirectHistoryEntry> entry =
        new nsRedirectHistoryEntry(uriPrincipal, nullptr, EmptyCString());

    redirectLoadInfo->AppendRedirectHistoryEntry(entry, true);
  }

  if (!aDestinationProcess) {
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
  nsresult rv = registrar->RegisterChannel(aChannel, &mRedirectChannelId);
  NS_ENSURE_SUCCESS_VOID(rv);

  Maybe<LoadInfoArgs> loadInfoArgs;
  MOZ_ALWAYS_SUCCEEDS(
      ipc::LoadInfoToLoadInfoArgs(redirectLoadInfo, &loadInfoArgs));

  uint32_t newLoadFlags = nsIRequest::LOAD_NORMAL;
  MOZ_ALWAYS_SUCCEEDS(aChannel->GetLoadFlags(&newLoadFlags));
  if (!aDestinationProcess) {
    newLoadFlags |= nsIChannel::LOAD_REPLACE;
  }

  nsCOMPtr<nsIURI> originalURI;
  aChannel->GetOriginalURI(getter_AddRefs(originalURI));

  uint64_t channelId = 0;
  // aChannel can be a nsHttpChannel as well as InterceptedHttpChannel so we
  // can't use baseChannel here.
  if (nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aChannel)) {
    MOZ_ALWAYS_SUCCEEDS(httpChannel->GetChannelId(&channelId));
  }

  uint32_t redirectMode = nsIHttpChannelInternal::REDIRECT_MODE_FOLLOW;
  nsCOMPtr<nsIHttpChannelInternal> httpChannelInternal =
      do_QueryInterface(aChannel);
  if (httpChannelInternal) {
    MOZ_ALWAYS_SUCCEEDS(httpChannelInternal->GetRedirectMode(&redirectMode));
  }

  // If we didn't have any redirects, then we pass the REDIRECT_INTERNAL flag
  // for this channel switch so that it isn't recorded in session history etc.
  // If there were redirect(s), then we want this switch to be recorded as a
  // real one, since we have a new URI.
  uint32_t redirectFlags = 0;
  if (mRedirects.IsEmpty()) {
    redirectFlags = nsIChannelEventSink::REDIRECT_INTERNAL;
  }

  Maybe<ReplacementChannelConfigInit> config;

  if (baseChannel) {
    uint32_t loadFlags = 0;
    if (!aDestinationProcess) {
      loadFlags |= nsIChannel::LOAD_REPLACE;
    }

    config =
        Some(baseChannel
                 ->CloneReplacementChannelConfig(true, redirectFlags, loadFlags)
                 .Serialize());
  }

  Maybe<uint32_t> contentDisposition;
  uint32_t contentDispositionTemp;
  rv = aChannel->GetContentDisposition(&contentDispositionTemp);
  if (NS_SUCCEEDED(rv)) {
    contentDisposition = Some(contentDispositionTemp);
  }

  Maybe<nsString> contentDispositionFilename;
  nsString contentDispositionFilenameTemp;
  rv = aChannel->GetContentDispositionFilename(contentDispositionFilenameTemp);
  if (NS_SUCCEEDED(rv)) {
    contentDispositionFilename = Some(contentDispositionFilenameTemp);
  }

  RefPtr<DocumentChannelParent> self = this;
  if (aDestinationProcess) {
    dom::ContentParent* cp =
        dom::ContentProcessManager::GetSingleton()->GetContentProcessById(
            ContentParentId{*aDestinationProcess});
    if (!cp) {
      return;
    }

    MOZ_ASSERT(config);

    cp->SendCrossProcessRedirect(mRedirectChannelId, uri, *config, loadInfoArgs,
                                 channelId, originalURI, aIdentifier,
                                 redirectMode)
        ->Then(
            GetCurrentThreadSerialEventTarget(), __func__,
            [self](Tuple<nsresult, Maybe<LoadInfoArgs>>&& aResponse) {
              if (NS_SUCCEEDED(Get<0>(aResponse))) {
                nsCOMPtr<nsILoadInfo> newLoadInfo;
                MOZ_ALWAYS_SUCCEEDS(LoadInfoArgsToLoadInfo(
                    Get<1>(aResponse), getter_AddRefs(newLoadInfo)));

                if (newLoadInfo) {
                  self->mChannel->SetLoadInfo(newLoadInfo);
                }
              }
              self->RedirectToRealChannelFinished(Get<0>(aResponse));
            },
            [self](const mozilla::ipc::ResponseRejectReason) {
              self->RedirectToRealChannelFinished(NS_ERROR_FAILURE);
            });
  } else {
    SendRedirectToRealChannel(mRedirectChannelId, uri, newLoadFlags, config,
                              loadInfoArgs, mRedirects, channelId, originalURI,
                              redirectMode, redirectFlags, contentDisposition,
                              contentDispositionFilename)
        ->Then(
            GetCurrentThreadSerialEventTarget(), __func__,
            [self](nsresult aRv) { self->RedirectToRealChannelFinished(aRv); },
            [self](const mozilla::ipc::ResponseRejectReason) {
              self->RedirectToRealChannelFinished(NS_ERROR_FAILURE);
            });
  }
}

NS_IMETHODIMP
DocumentChannelParent::OnStartRequest(nsIRequest* aRequest) {
  nsCOMPtr<nsHttpChannel> channel = do_QueryInterface(aRequest);
  mChannel = do_QueryInterface(aRequest);
  MOZ_DIAGNOSTIC_ASSERT(mChannel);

  // If this is a download, then redirect entirely within the parent.
  // TODO, see bug 1574372.

  if (!CanSend()) {
    return NS_ERROR_UNEXPECTED;
  }

  // Once we initiate a process switch, we ask the child to notify the listeners
  // that we have completed. If the switch promise then gets rejected we also
  // cancel the parent, which results in this being called. We don't need
  // to forward it on though, since the child side is already completed.
  if (mDoingProcessSwitch) {
    return NS_OK;
  }

  mChannel->Suspend();
  mSuspendedChannel = true;

  // The caller of this OnStartRequest will install a conversion
  // helper after we return if we haven't disabled conversion. Normally
  // HttpChannelParent::OnStartRequest would disable conversion, but we're
  // defering calling that until later. Manually disable it now to prevent the
  // converter from being installed (since we want the child to do it), and
  // also save the value so that when we do call
  // HttpChannelParent::OnStartRequest, we can have the value as it originally
  // was.
  if (channel) {
    Unused << channel->GetApplyConversion(&mOldApplyConversion);
    channel->SetApplyConversion(false);

    // notify "http-on-may-change-process" observers which is typically
    // SessionStore.jsm. This will determine if a new process needs to be
    // spawned and if so SwitchProcessTo() will be called which will set a
    // ContentProcessIdPromise.
    gHttpHandler->OnMayChangeProcess(this);
    if (mRedirectContentProcessIdPromise) {
      TriggerCrossProcessSwitch();
      return NS_OK;
    }
  }

  TriggerRedirectToRealChannel(mChannel);

  return NS_OK;
}

NS_IMETHODIMP
DocumentChannelParent::OnStopRequest(nsIRequest* aRequest,
                                     nsresult aStatusCode) {
  mStopRequestValue = Some(aStatusCode);

  return NS_OK;
}

NS_IMETHODIMP
DocumentChannelParent::OnDataAvailable(nsIRequest* aRequest,
                                       nsIInputStream* aInputStream,
                                       uint64_t aOffset, uint32_t aCount) {
  // This isn't supposed to happen, since we suspended the channel, but
  // sometimes Suspend just doesn't work. This can happen when we're routing
  // through nsUnknownDecoder to sniff the content type, and it doesn't handle
  // being suspended. Let's just store the data and manually forward it to our
  // redirected channel when it's ready.
  nsCString data;
  nsresult rv = NS_ReadInputStreamToString(aInputStream, data, aCount);
  NS_ENSURE_SUCCESS(rv, rv);

  mPendingRequests.AppendElement(
      OnDataAvailableRequest({data, aOffset, aCount}));

  return NS_OK;
}

NS_IMETHODIMP
DocumentChannelParent::SetParentListener(
    mozilla::net::ParentChannelListener* listener) {
  // We don't need this (do we?)
  return NS_OK;
}

NS_IMETHODIMP
DocumentChannelParent::GetInterface(const nsIID& aIID, void** result) {
  // Only support nsILoadContext if child channel's callbacks did too
  if (aIID.Equals(NS_GET_IID(nsILoadContext)) && mLoadContext) {
    nsCOMPtr<nsILoadContext> copy = mLoadContext;
    copy.forget(result);
    return NS_OK;
  }

  if (aIID.Equals(NS_GET_IID(nsIAuthPromptProvider)) ||
      aIID.Equals(NS_GET_IID(nsISecureBrowserUI)) ||
      aIID.Equals(NS_GET_IID(nsIRemoteTab))) {
    if (mBrowserParent) {
      return mBrowserParent->QueryInterface(aIID, result);
    }
  }

  nsresult rv = QueryInterface(aIID, result);
  return rv;
}

// Rather than forwarding all these nsIParentChannel functions to the child, we
// cache a list of them, and then ask the 'real' channel to forward them for us
// after it's created.
NS_IMETHODIMP
DocumentChannelParent::NotifyChannelClassifierProtectionDisabled(
    uint32_t aAcceptedReason) {
  if (CanSend()) {
    Unused << SendNotifyChannelClassifierProtectionDisabled(aAcceptedReason);
  }
  return NS_OK;
}

NS_IMETHODIMP
DocumentChannelParent::NotifyCookieAllowed() {
  if (CanSend()) {
    Unused << SendNotifyCookieAllowed();
  }
  return NS_OK;
}

NS_IMETHODIMP
DocumentChannelParent::NotifyCookieBlocked(uint32_t aRejectedReason) {
  if (CanSend()) {
    Unused << SendNotifyCookieBlocked(aRejectedReason);
  }
  return NS_OK;
}

NS_IMETHODIMP
DocumentChannelParent::NotifyFlashPluginStateChanged(
    nsIHttpChannel::FlashPluginState aState) {
  mIParentChannelFunctions.AppendElement(
      IParentChannelFunction{VariantIndex<0>{}, aState});
  return NS_OK;
}

NS_IMETHODIMP
DocumentChannelParent::SetClassifierMatchedInfo(const nsACString& aList,
                                                const nsACString& aProvider,
                                                const nsACString& aFullHash) {
  ClassifierMatchedInfoParams params;
  params.mList = aList;
  params.mProvider = aProvider;
  params.mFullHash = aFullHash;

  if (CanSend()) {
    Unused << SendSetClassifierMatchedInfo(params.mList, params.mProvider,
                                           params.mFullHash);
  }

  mIParentChannelFunctions.AppendElement(
      IParentChannelFunction{VariantIndex<1>{}, std::move(params)});
  return NS_OK;
}

NS_IMETHODIMP
DocumentChannelParent::SetClassifierMatchedTrackingInfo(
    const nsACString& aLists, const nsACString& aFullHash) {
  ClassifierMatchedTrackingInfoParams params;
  params.mLists = aLists;
  params.mFullHashes = aFullHash;

  if (CanSend()) {
    Unused << SendSetClassifierMatchedTrackingInfo(params.mLists,
                                                   params.mFullHashes);
  }

  mIParentChannelFunctions.AppendElement(
      IParentChannelFunction{VariantIndex<2>{}, std::move(params)});
  return NS_OK;
}

NS_IMETHODIMP
DocumentChannelParent::NotifyClassificationFlags(uint32_t aClassificationFlags,
                                                 bool aIsThirdParty) {
  mIParentChannelFunctions.AppendElement(IParentChannelFunction{
      VariantIndex<3>{},
      ClassificationFlagsParams{aClassificationFlags, aIsThirdParty}});
  return NS_OK;
}

NS_IMETHODIMP
DocumentChannelParent::Delete() {
  // TODO - not sure we need it, but should delete the child or call on the
  // child to release the parent.
  if (!CanSend()) {
    return NS_ERROR_UNEXPECTED;
  }
  Unused << SendDeleteSelf();
  return NS_OK;
}

NS_IMETHODIMP
DocumentChannelParent::AsyncOnChannelRedirect(
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

  // We don't need to confirm internal redirects or record any
  // history for them, so just immediately verify and return.
  if (aFlags & nsIChannelEventSink::REDIRECT_INTERNAL) {
    aCallback->OnRedirectVerifyCallback(NS_OK);
    return NS_OK;
  } else {
    nsCOMPtr<nsIURI> oldURI;
    aOldChannel->GetURI(getter_AddRefs(oldURI));
    uint32_t responseStatus = 0;
    bool isPost = false;
    if (nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aOldChannel)) {
      Unused << httpChannel->GetResponseStatus(&responseStatus);
      nsAutoCString method;
      Unused << httpChannel->GetRequestMethod(method);
      isPost = method.EqualsLiteral("POST");
    }
    mRedirects.AppendElement(
        DocumentChannelRedirect{oldURI, aFlags, responseStatus, isPost});
  }

  if (!CanSend()) {
    return NS_BINDING_ABORTED;
  }

  // Currently the CSP code expects to run in the content
  // process so that it can send events. Send a message to
  // our content process to ask CSP if we should allow this
  // redirect, and wait for confirmation.
  nsCOMPtr<nsIURI> newUri;
  nsresult rv = aNewChannel->GetURI(getter_AddRefs(newUri));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIAsyncVerifyRedirectCallback> callback(aCallback);
  nsCOMPtr<nsIChannel> oldChannel(aOldChannel);
  SendConfirmRedirect(newUri)->Then(
      GetCurrentThreadSerialEventTarget(), __func__,
      [callback, oldChannel](nsresult aRv) {
        if (NS_FAILED(aRv)) {
          oldChannel->Cancel(NS_ERROR_DOM_BAD_URI);
        }
        callback->OnRedirectVerifyCallback(aRv);
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
// DocumentChannelParent::nsIProcessSwitchRequestor
//-----------------------------------------------------------------------------

NS_IMETHODIMP DocumentChannelParent::GetChannel(nsIChannel** aChannel) {
  MOZ_ASSERT(mChannel);
  nsCOMPtr<nsIChannel> channel(mChannel);
  channel.forget(aChannel);
  return NS_OK;
}

NS_IMETHODIMP DocumentChannelParent::SwitchProcessTo(
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
DocumentChannelParent::HasCrossOriginOpenerPolicyMismatch(bool* aMismatch) {
  MOZ_ASSERT(aMismatch);

  if (!aMismatch) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsHttpChannel> channel = do_QueryInterface(mChannel);
  if (!channel) {
    // Not an nsHttpChannel assume it's okay to switch.
    *aMismatch = false;
    return NS_OK;
  }

  return channel->HasCrossOriginOpenerPolicyMismatch(aMismatch);
}

}  // namespace net
}  // namespace mozilla
