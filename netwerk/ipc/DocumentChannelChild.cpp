/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocumentChannelChild.h"

#include "mozilla/dom/Document.h"
#include "mozilla/dom/RemoteType.h"
#include "mozilla/extensions/StreamFilterParent.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/net/HttpBaseChannel.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticPrefs_fission.h"
#include "nsHashPropertyBag.h"
#include "nsIHttpChannelInternal.h"
#include "nsIObjectLoadingContent.h"
#include "nsIXULRuntime.h"
#include "nsIWritablePropertyBag.h"
#include "nsFrameLoader.h"
#include "nsFrameLoaderOwner.h"
#include "nsQueryObject.h"
#include "nsDocShellLoadState.h"

using namespace mozilla::dom;
using namespace mozilla::ipc;

extern mozilla::LazyLogModule gDocumentChannelLog;
#define LOG(fmt) MOZ_LOG(gDocumentChannelLog, mozilla::LogLevel::Verbose, fmt)

namespace mozilla {
namespace net {

//-----------------------------------------------------------------------------
// DocumentChannelChild::nsISupports

NS_INTERFACE_MAP_BEGIN(DocumentChannelChild)
  NS_INTERFACE_MAP_ENTRY(nsIAsyncVerifyRedirectCallback)
NS_INTERFACE_MAP_END_INHERITING(DocumentChannel)

NS_IMPL_ADDREF_INHERITED(DocumentChannelChild, DocumentChannel)
NS_IMPL_RELEASE_INHERITED(DocumentChannelChild, DocumentChannel)

DocumentChannelChild::DocumentChannelChild(nsDocShellLoadState* aLoadState,
                                           net::LoadInfo* aLoadInfo,
                                           nsLoadFlags aLoadFlags,
                                           uint32_t aCacheKey,
                                           bool aUriModified,
                                           bool aIsEmbeddingBlockedError)
    : DocumentChannel(aLoadState, aLoadInfo, aLoadFlags, aCacheKey,
                      aUriModified, aIsEmbeddingBlockedError) {
  mLoadingContext = nullptr;
  LOG(("DocumentChannelChild ctor [this=%p, uri=%s]", this,
       aLoadState->URI()->GetSpecOrDefault().get()));
}

DocumentChannelChild::~DocumentChannelChild() {
  LOG(("DocumentChannelChild dtor [this=%p]", this));
}

NS_IMETHODIMP
DocumentChannelChild::AsyncOpen(nsIStreamListener* aListener) {
  nsresult rv = NS_OK;

  nsCOMPtr<nsIStreamListener> listener = aListener;

  NS_ENSURE_TRUE(gNeckoChild, NS_ERROR_FAILURE);
  NS_ENSURE_ARG_POINTER(listener);
  NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);
  NS_ENSURE_TRUE(!mWasOpened, NS_ERROR_ALREADY_OPENED);

  // Port checked in parent, but duplicate here so we can return with error
  // immediately, as we've done since before e10s.
  rv = NS_CheckPortSafety(mURI);
  NS_ENSURE_SUCCESS(rv, rv);

  bool isNotDownload = mLoadState->FileName().IsVoid();

  // If not a download, add ourselves to the load group
  if (isNotDownload && mLoadGroup) {
    // During this call, we can re-enter back into the DocumentChannelChild to
    // call SetNavigationTiming.
    mLoadGroup->AddRequest(this, nullptr);
  }

  if (mCanceled) {
    // We may have been canceled already, either by on-modify-request
    // listeners or by load group observers; in that case, don't create IPDL
    // connection. See nsHttpChannel::AsyncOpen().
    return mStatus;
  }

  gHttpHandler->OnOpeningDocumentRequest(this);

  RefPtr<nsDocShell> docShell = GetDocShell();
  if (!docShell) {
    return NS_ERROR_FAILURE;
  }

  // `loadingContext` is the BC that is initiating the resource load.
  // For normal subdocument loads, the BC is the one that the subdoc will load
  // into. For <object>/<embed> it's the embedder doc's BC.
  RefPtr<BrowsingContext> loadingContext = docShell->GetBrowsingContext();
  if (!loadingContext || loadingContext->IsDiscarded()) {
    return NS_ERROR_FAILURE;
  }
  mLoadingContext = loadingContext;

  Maybe<IPCClientInfo> ipcClientInfo;
  if (mInitialClientInfo.isSome()) {
    ipcClientInfo.emplace(mInitialClientInfo.ref().ToIPC());
  }

  DocumentChannelElementCreationArgs ipcElementCreationArgs;
  switch (mLoadInfo->GetExternalContentPolicyType()) {
    case ExtContentPolicy::TYPE_DOCUMENT:
    case ExtContentPolicy::TYPE_SUBDOCUMENT: {
      DocumentCreationArgs docArgs;
      docArgs.uriModified() = mUriModified;
      docArgs.isEmbeddingBlockedError() = mIsEmbeddingBlockedError;

      ipcElementCreationArgs = docArgs;
      break;
    }

    case ExtContentPolicy::TYPE_OBJECT: {
      ObjectCreationArgs objectArgs;
      objectArgs.embedderInnerWindowId() = InnerWindowIDForExtantDoc(docShell);
      objectArgs.loadFlags() = mLoadFlags;
      objectArgs.contentPolicyType() = mLoadInfo->InternalContentPolicyType();
      objectArgs.isUrgentStart() = UserActivation::IsHandlingUserInput();

      ipcElementCreationArgs = objectArgs;
      break;
    }

    default:
      MOZ_ASSERT_UNREACHABLE("unsupported content policy type");
      return NS_ERROR_FAILURE;
  }

  switch (mLoadInfo->GetExternalContentPolicyType()) {
    case ExtContentPolicy::TYPE_DOCUMENT:
    case ExtContentPolicy::TYPE_SUBDOCUMENT:
      MOZ_ALWAYS_SUCCEEDS(loadingContext->SetCurrentLoadIdentifier(
          Some(mLoadState->GetLoadIdentifier())));
      break;

    default:
      break;
  }

  mLoadState->AssertProcessCouldTriggerLoadIfSystem();

  DocumentChannelCreationArgs args(
      mozilla::WrapNotNull(mLoadState), TimeStamp::Now(), mChannelId, mCacheKey,
      mTiming, ipcClientInfo, ipcElementCreationArgs,
      loadingContext->GetParentInitiatedNavigationEpoch());

  gNeckoChild->SendPDocumentChannelConstructor(this, loadingContext, args);

  mIsPending = true;
  mWasOpened = true;
  mListener = listener;

  return NS_OK;
}

IPCResult DocumentChannelChild::RecvFailedAsyncOpen(
    const nsresult& aStatusCode) {
  if (aStatusCode == NS_ERROR_RECURSIVE_DOCUMENT_LOAD) {
    // This exists so that we are able to fire an error event
    // for when there are too many recursive iframe or object loads.
    // This is an incomplete solution, because right now we don't have a unified
    // way of firing error events due to errors in document channel.
    // This should be fixed in bug 1629201.
    MOZ_DIAGNOSTIC_ASSERT(mLoadingContext);
    if (RefPtr<Element> embedder = mLoadingContext->GetEmbedderElement()) {
      if (RefPtr<nsFrameLoaderOwner> flo = do_QueryObject(embedder)) {
        if (RefPtr<nsFrameLoader> fl = flo->GetFrameLoader()) {
          fl->FireErrorEvent();
        }
      }
    }
  }
  ShutdownListeners(aStatusCode);
  return IPC_OK();
}

IPCResult DocumentChannelChild::RecvDisconnectChildListeners(
    const nsresult& aStatus, const nsresult& aLoadGroupStatus,
    bool aContinueNavigating) {
  // If this disconnect is not due to a process switch, perform the disconnect
  // immediately.
  if (!aContinueNavigating) {
    DisconnectChildListeners(aStatus, aLoadGroupStatus);
    return IPC_OK();
  }

  // Otherwise, the disconnect will occur later using some other mechanism,
  // depending on what's happening to the loading DocShell. If this is a
  // toplevel navigation, and this BrowsingContext enters the BFCache, we will
  // cancel this channel when the PageHide event is firing, whereas if it does
  // not enter BFCache (e.g. due to being an object, subframe or non-bfcached
  // toplevel navigation), we will cancel this channel when the DocShell is
  // destroyed.
  nsDocShell* shell = GetDocShell();
  if (mLoadInfo->GetExternalContentPolicyType() ==
          ExtContentPolicy::TYPE_DOCUMENT &&
      shell) {
    MOZ_ASSERT(shell->GetBrowsingContext()->IsTop());
    if (mozilla::SessionHistoryInParent() &&
        shell->GetBrowsingContext()->IsInBFCache()) {
      DisconnectChildListeners(aStatus, aLoadGroupStatus);
    } else {
      // Tell the DocShell which channel to cancel if it enters the BFCache.
      shell->SetChannelToDisconnectOnPageHide(mChannelId);
    }
  }

  return IPC_OK();
}

IPCResult DocumentChannelChild::RecvRedirectToRealChannel(
    RedirectToRealChannelArgs&& aArgs,
    nsTArray<Endpoint<extensions::PStreamFilterParent>>&& aEndpoints,
    RedirectToRealChannelResolver&& aResolve) {
  LOG(("DocumentChannelChild RecvRedirectToRealChannel [this=%p, uri=%s]", this,
       aArgs.uri()->GetSpecOrDefault().get()));

  // The document that created the cspToInherit.
  // This is used when deserializing LoadInfo from the parent
  // process, since we can't serialize Documents directly.
  // TODO: For a fission OOP iframe this will be unavailable,
  // as will the loadingContext computed in LoadInfoArgsToLoadInfo.
  // Figure out if we need these for cross-origin subdocs.
  RefPtr<dom::Document> cspToInheritLoadingDocument;
  nsCOMPtr<nsIContentSecurityPolicy> policy = mLoadState->Csp();
  if (policy) {
    nsWeakPtr ctx =
        static_cast<nsCSPContext*>(policy.get())->GetLoadingContext();
    cspToInheritLoadingDocument = do_QueryReferent(ctx);
  }
  nsCOMPtr<nsILoadInfo> loadInfo;
  MOZ_ALWAYS_SUCCEEDS(LoadInfoArgsToLoadInfo(aArgs.loadInfo(), NOT_REMOTE_TYPE,
                                             cspToInheritLoadingDocument,
                                             getter_AddRefs(loadInfo)));

  mRedirectResolver = std::move(aResolve);

  nsCOMPtr<nsIChannel> newChannel;
  MOZ_ASSERT((aArgs.loadStateInternalLoadFlags() &
              nsDocShell::InternalLoad::INTERNAL_LOAD_FLAGS_IS_SRCDOC) ||
             aArgs.srcdocData().IsVoid());
  nsresult rv = nsDocShell::CreateRealChannelForDocument(
      getter_AddRefs(newChannel), aArgs.uri(), loadInfo, nullptr,
      aArgs.newLoadFlags(), aArgs.srcdocData(), aArgs.baseUri());
  if (newChannel) {
    newChannel->SetLoadGroup(mLoadGroup);
  }

  if (RefPtr<HttpBaseChannel> httpChannel = do_QueryObject(newChannel)) {
    httpChannel->SetEarlyHints(std::move(aArgs.earlyHints()));
    httpChannel->SetEarlyHintLinkType(aArgs.earlyHintLinkType());
  }

  // This is used to report any errors back to the parent by calling
  // CrossProcessRedirectFinished.
  auto scopeExit = MakeScopeExit([&]() {
    mRedirectResolver(rv);
    mRedirectResolver = nullptr;
  });

  if (NS_FAILED(rv)) {
    return IPC_OK();
  }

  if (nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(newChannel)) {
    rv = httpChannel->SetChannelId(aArgs.channelId());
  }
  if (NS_FAILED(rv)) {
    return IPC_OK();
  }

  rv = newChannel->SetOriginalURI(aArgs.originalURI());
  if (NS_FAILED(rv)) {
    return IPC_OK();
  }

  if (nsCOMPtr<nsIHttpChannelInternal> httpChannelInternal =
          do_QueryInterface(newChannel)) {
    rv = httpChannelInternal->SetRedirectMode(aArgs.redirectMode());
  }
  if (NS_FAILED(rv)) {
    return IPC_OK();
  }

  newChannel->SetNotificationCallbacks(mCallbacks);

  if (aArgs.init()) {
    HttpBaseChannel::ReplacementChannelConfig config(*aArgs.init());
    HttpBaseChannel::ConfigureReplacementChannel(
        newChannel, config,
        HttpBaseChannel::ReplacementReason::DocumentChannel);
  }

  if (aArgs.contentDisposition()) {
    newChannel->SetContentDisposition(*aArgs.contentDisposition());
  }

  if (aArgs.contentDispositionFilename()) {
    newChannel->SetContentDispositionFilename(
        *aArgs.contentDispositionFilename());
  }

  nsDocShell* docShell = GetDocShell();
  if (docShell && aArgs.loadingSessionHistoryInfo().isSome()) {
    docShell->SetLoadingSessionHistoryInfo(
        aArgs.loadingSessionHistoryInfo().ref());
  }

  // transfer any properties. This appears to be entirely a content-side
  // interface and isn't copied across to the parent. Copying the values
  // for this from this into the new actor will work, since the parent
  // won't have the right details anyway.
  // TODO: What about the process switch equivalent
  // (ContentChild::RecvCrossProcessRedirect)? In that case there is no local
  // existing actor in the destination process... We really need all information
  // to go up to the parent, and then come down to the new child actor.
  if (nsCOMPtr<nsIWritablePropertyBag> bag = do_QueryInterface(newChannel)) {
    nsHashPropertyBag::CopyFrom(bag, aArgs.properties());
  }

  // connect parent.
  nsCOMPtr<nsIChildChannel> childChannel = do_QueryInterface(newChannel);
  if (childChannel) {
    rv = childChannel->ConnectParent(
        aArgs.registrarId());  // creates parent channel
    if (NS_FAILED(rv)) {
      return IPC_OK();
    }
  }
  mRedirectChannel = newChannel;
  mStreamFilterEndpoints = std::move(aEndpoints);

  rv = gHttpHandler->AsyncOnChannelRedirect(this, newChannel,
                                            aArgs.redirectFlags(),
                                            GetMainThreadSerialEventTarget());

  if (NS_SUCCEEDED(rv)) {
    scopeExit.release();
  }

  // scopeExit will call CrossProcessRedirectFinished(rv) here
  return IPC_OK();
}

IPCResult DocumentChannelChild::RecvUpgradeObjectLoad(
    UpgradeObjectLoadResolver&& aResolve) {
  // We're doing a load for an <object> or <embed> element if we got here.
  MOZ_ASSERT(mLoadFlags & nsIRequest::LOAD_HTML_OBJECT_DATA,
             "Should have LOAD_HTML_OBJECT_DATA set");
  MOZ_ASSERT(!(mLoadFlags & nsIChannel::LOAD_DOCUMENT_URI),
             "Shouldn't be a LOAD_DOCUMENT_URI load yet");
  MOZ_ASSERT(mLoadInfo->GetExternalContentPolicyType() ==
                 ExtContentPolicy::TYPE_OBJECT,
             "Should have the TYPE_OBJECT content policy type");

  // If our load has already failed, or been cancelled, abort this attempt to
  // upgade the load.
  if (NS_FAILED(mStatus)) {
    aResolve(nullptr);
    return IPC_OK();
  }

  nsCOMPtr<nsIObjectLoadingContent> loadingContent;
  NS_QueryNotificationCallbacks(this, loadingContent);
  if (!loadingContent) {
    return IPC_FAIL(this, "Channel is not for ObjectLoadingContent!");
  }

  // We're upgrading to a document channel now. Add the LOAD_DOCUMENT_URI flag
  // after-the-fact.
  mLoadFlags |= nsIChannel::LOAD_DOCUMENT_URI;

  RefPtr<BrowsingContext> browsingContext;
  nsresult rv = loadingContent->UpgradeLoadToDocument(
      this, getter_AddRefs(browsingContext));
  if (NS_FAILED(rv) || !browsingContext) {
    // Oops! Looks like something went wrong, so let's bail out.
    mLoadFlags &= ~nsIChannel::LOAD_DOCUMENT_URI;
    aResolve(nullptr);
    return IPC_OK();
  }

  aResolve(browsingContext);
  return IPC_OK();
}

NS_IMETHODIMP
DocumentChannelChild::OnRedirectVerifyCallback(nsresult aStatusCode) {
  LOG(
      ("DocumentChannelChild OnRedirectVerifyCallback [this=%p, "
       "aRv=0x%08" PRIx32 " ]",
       this, static_cast<uint32_t>(aStatusCode)));
  nsCOMPtr<nsIChannel> redirectChannel = std::move(mRedirectChannel);
  RedirectToRealChannelResolver redirectResolver = std::move(mRedirectResolver);

  // If we've already shut down, then just notify the parent that
  // we're done.
  if (NS_FAILED(mStatus)) {
    redirectChannel->SetNotificationCallbacks(nullptr);
    redirectResolver(aStatusCode);
    return NS_OK;
  }

  nsresult rv = aStatusCode;
  if (NS_SUCCEEDED(rv)) {
    if (nsCOMPtr<nsIChildChannel> childChannel =
            do_QueryInterface(redirectChannel)) {
      rv = childChannel->CompleteRedirectSetup(mListener);
    } else {
      rv = redirectChannel->AsyncOpen(mListener);
    }
  } else {
    redirectChannel->SetNotificationCallbacks(nullptr);
  }

  for (auto& endpoint : mStreamFilterEndpoints) {
    extensions::StreamFilterParent::Attach(redirectChannel,
                                           std::move(endpoint));
  }

  redirectResolver(rv);

  if (NS_FAILED(rv)) {
    ShutdownListeners(rv);
    return NS_OK;
  }

  if (mLoadGroup) {
    mLoadGroup->RemoveRequest(this, nullptr, NS_BINDING_REDIRECTED);
  }
  mCallbacks = nullptr;
  mListener = nullptr;

  // This calls NeckoChild::DeallocPDocumentChannel(), which deletes |this| if
  // IPDL holds the last reference.  Don't rely on |this| existing after here!
  if (CanSend()) {
    Send__delete__(this);
  }

  return NS_OK;
}

NS_IMETHODIMP
DocumentChannelChild::Cancel(nsresult aStatusCode) {
  return CancelWithReason(aStatusCode, "DocumentChannelChild::Cancel"_ns);
}

NS_IMETHODIMP DocumentChannelChild::CancelWithReason(
    nsresult aStatusCode, const nsACString& aReason) {
  if (mCanceled) {
    return NS_OK;
  }

  mCanceled = true;
  if (CanSend()) {
    SendCancel(aStatusCode, aReason);
  }

  ShutdownListeners(aStatusCode);

  return NS_OK;
}

}  // namespace net
}  // namespace mozilla

#undef LOG
