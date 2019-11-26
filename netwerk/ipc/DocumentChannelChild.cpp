/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocumentChannelChild.h"

#include "SerializedLoadContext.h"
#include "mozIThirdPartyUtil.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/nsCSPContext.h"
#include "mozilla/extensions/StreamFilterParent.h"
#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/net/HttpChannelChild.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/net/UrlClassifierCommon.h"
#include "nsContentSecurityManager.h"
#include "nsDocShellLoadState.h"
#include "nsHttpHandler.h"
#include "nsQueryObject.h"
#include "nsSerializationHelper.h"
#include "nsStreamListenerWrapper.h"
#include "nsStringStream.h"
#include "nsURLHelper.h"

using namespace mozilla::dom;
using namespace mozilla::ipc;

namespace mozilla {
namespace net {

//-----------------------------------------------------------------------------
// DocumentChannelChild::nsISupports

NS_IMPL_ADDREF(DocumentChannelChild)
NS_IMPL_RELEASE(DocumentChannelChild)

NS_INTERFACE_MAP_BEGIN(DocumentChannelChild)
  if (mWasOpened && aIID == NS_GET_IID(nsIHttpChannel)) {
    // DocumentChannelChild generally is doing an http connection
    // internally, but doesn't implement the interface. Everything
    // before AsyncOpen should be duplicated in the parent process
    // on the real http channel, but anything trying to QI to nsIHttpChannel
    // after that will be failing and get confused.
    NS_WARNING(
        "Trying to request nsIHttpChannel from DocumentChannelChild, this is "
        "likely broken");
  }
  NS_INTERFACE_MAP_ENTRY(nsIRequest)
  NS_INTERFACE_MAP_ENTRY(nsIChannel)
  NS_INTERFACE_MAP_ENTRY(nsITraceableChannel)
  NS_INTERFACE_MAP_ENTRY(nsIAsyncVerifyRedirectCallback)
  NS_INTERFACE_MAP_ENTRY_CONCRETE(DocumentChannelChild)
NS_INTERFACE_MAP_END_INHERITING(nsHashPropertyBag)

DocumentChannelChild::DocumentChannelChild(
    nsDocShellLoadState* aLoadState, net::LoadInfo* aLoadInfo,
    const nsString* aInitiatorType, nsLoadFlags aLoadFlags, uint32_t aLoadType,
    uint32_t aCacheKey, bool aIsActive, bool aIsTopLevelDoc,
    bool aHasNonEmptySandboxingFlags)
    : mAsyncOpenTime(TimeStamp::Now()),
      mLoadState(aLoadState),
      mInitiatorType(aInitiatorType ? Some(*aInitiatorType) : Nothing()),
      mLoadType(aLoadType),
      mCacheKey(aCacheKey),
      mIsActive(aIsActive),
      mIsTopLevelDoc(aIsTopLevelDoc),
      mHasNonEmptySandboxingFlags(aHasNonEmptySandboxingFlags),
      mLoadFlags(aLoadFlags),
      mURI(aLoadState->URI()),
      mLoadInfo(aLoadInfo) {
  RefPtr<nsHttpHandler> handler = nsHttpHandler::GetInstance();
  uint64_t channelId;
  Unused << handler->NewChannelId(channelId);
  mChannelId = channelId;
}

NS_IMETHODIMP
DocumentChannelChild::AsyncOpen(nsIStreamListener* aListener) {
  nsresult rv = NS_OK;

  nsCOMPtr<nsIStreamListener> listener = aListener;
  rv = nsContentSecurityManager::doContentSecurityCheck(this, listener);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ENSURE_TRUE(gNeckoChild, NS_ERROR_FAILURE);
  NS_ENSURE_ARG_POINTER(listener);
  NS_ENSURE_TRUE(!mIsPending, NS_ERROR_IN_PROGRESS);
  NS_ENSURE_TRUE(!mWasOpened, NS_ERROR_ALREADY_OPENED);

  // Port checked in parent, but duplicate here so we can return with error
  // immediately, as we've done since before e10s.
  rv = NS_CheckPortSafety(mURI);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> topWindowURI;
  nsCOMPtr<nsIURI> uriBeingLoaded =
      AntiTrackingCommon::MaybeGetDocumentURIBeingLoaded(this);
  nsCOMPtr<nsIPrincipal> contentBlockingAllowListPrincipal;

  nsCOMPtr<mozIThirdPartyUtil> util = services::GetThirdPartyUtil();
  if (util) {
    nsCOMPtr<mozIDOMWindowProxy> win;
    rv =
        util->GetTopWindowForChannel(this, uriBeingLoaded, getter_AddRefs(win));
    if (NS_SUCCEEDED(rv)) {
      util->GetURIFromWindow(win, getter_AddRefs(topWindowURI));

      Unused << util->GetContentBlockingAllowListPrincipalFromWindow(
          win, uriBeingLoaded,
          getter_AddRefs(contentBlockingAllowListPrincipal));
    }
  }

  // add ourselves to the load group.
  if (mLoadGroup) {
    mLoadGroup->AddRequest(this, nullptr);
  }

  if (mCanceled) {
    // We may have been canceled already, either by on-modify-request
    // listeners or by load group observers; in that case, don't create IPDL
    // connection. See nsHttpChannel::AsyncOpen().
    return mStatus;
  }

  gHttpHandler->OnOpeningDocumentRequest(this);

  DocumentChannelCreationArgs args;

  SerializeURI(topWindowURI, args.topWindowURI());
  args.loadState() = mLoadState->Serialize();
  Maybe<LoadInfoArgs> maybeArgs;
  rv = LoadInfoToLoadInfoArgs(mLoadInfo, &maybeArgs);
  NS_ENSURE_SUCCESS(rv, rv);
  MOZ_DIAGNOSTIC_ASSERT(maybeArgs);

  if (contentBlockingAllowListPrincipal) {
    PrincipalInfo principalInfo;
    rv = PrincipalToPrincipalInfo(contentBlockingAllowListPrincipal,
                                  &principalInfo);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    args.contentBlockingAllowListPrincipal() = Some(principalInfo);
  }

  args.loadInfo() = *maybeArgs;
  args.loadFlags() = mLoadFlags;
  args.initiatorType() = mInitiatorType;
  args.loadType() = mLoadType;
  args.cacheKey() = mCacheKey;
  args.isActive() = mIsActive;
  args.isTopLevelDoc() = mIsTopLevelDoc;
  args.hasNonEmptySandboxingFlags() = mHasNonEmptySandboxingFlags;
  args.channelId() = mChannelId;
  args.asyncOpenTime() = mAsyncOpenTime;

  nsCOMPtr<nsILoadContext> loadContext;
  NS_QueryNotificationCallbacks(this, loadContext);
  if (loadContext) {
    nsCOMPtr<mozIDOMWindowProxy> domWindow;
    loadContext->GetAssociatedWindow(getter_AddRefs(domWindow));
    if (domWindow) {
      auto* pDomWindow = nsPIDOMWindowOuter::From(domWindow);
      nsIDocShell* docshell = pDomWindow->GetDocShell();
      if (docshell) {
        docshell->GetCustomUserAgent(args.customUserAgent());
      }
    }
  }

  nsCOMPtr<nsIBrowserChild> iBrowserChild;
  NS_QueryNotificationCallbacks(mCallbacks, mLoadGroup,
                                NS_GET_TEMPLATE_IID(nsIBrowserChild),
                                getter_AddRefs(iBrowserChild));
  BrowserChild* browserChild = static_cast<BrowserChild*>(iBrowserChild.get());
  if (MissingRequiredBrowserChild(browserChild, "ftp")) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  // TODO: What happens if the caller has called other methods on the
  // nsIChannel after the ctor, but before this?

  gNeckoChild->SendPDocumentChannelConstructor(
      this, browserChild, IPC::SerializedLoadContext(this), args);

  mIsPending = true;
  mWasOpened = true;
  mListener = listener;

  return NS_OK;
}

IPCResult DocumentChannelChild::RecvFailedAsyncOpen(
    const nsresult& aStatusCode) {
  ShutdownListeners(aStatusCode);
  return IPC_OK();
}

void DocumentChannelChild::ShutdownListeners(nsresult aStatusCode) {
  mStatus = aStatusCode;

  nsCOMPtr<nsIStreamListener> l = mListener;
  if (l) {
    l->OnStartRequest(this);
  }

  mIsPending = false;

  l = mListener;  // it might have changed!
  if (l) {
    l->OnStopRequest(this, aStatusCode);
  }
  mListener = nullptr;
  mCallbacks = nullptr;

  if (mLoadGroup) {
    mLoadGroup->RemoveRequest(this, nullptr, aStatusCode);
    mLoadGroup = nullptr;
  }

  if (CanSend()) {
    Send__delete__(this);
  }
}

IPCResult DocumentChannelChild::RecvDisconnectChildListeners(
    const nsresult& aStatus) {
  MOZ_ASSERT(NS_FAILED(aStatus));
  // Make sure we remove from the load group before
  // setting mStatus, as existing tests expect the
  // status to be successful when we disconnect.
  if (mLoadGroup) {
    mLoadGroup->RemoveRequest(this, nullptr, aStatus);
    mLoadGroup = nullptr;
  }

  ShutdownListeners(aStatus);
  return IPC_OK();
}

IPCResult DocumentChannelChild::RecvDeleteSelf() {
  // This calls NeckoChild::DeallocPGenericChannel(), which deletes |this| if
  // IPDL holds the last reference.  Don't rely on |this| existing after here!
  Send__delete__(this);
  return IPC_OK();
}

IPCResult DocumentChannelChild::RecvRedirectToRealChannel(
    const RedirectToRealChannelArgs& aArgs,
    RedirectToRealChannelResolver&& aResolve) {
  RefPtr<dom::Document> loadingDocument;
  mLoadInfo->GetLoadingDocument(getter_AddRefs(loadingDocument));

  RefPtr<dom::Document> cspToInheritLoadingDocument;
  nsCOMPtr<nsIContentSecurityPolicy> policy = mLoadInfo->GetCspToInherit();
  if (policy) {
    nsWeakPtr ctx =
        static_cast<nsCSPContext*>(policy.get())->GetLoadingContext();
    cspToInheritLoadingDocument = do_QueryReferent(ctx);
  }
  nsCOMPtr<nsILoadInfo> loadInfo;
  MOZ_ALWAYS_SUCCEEDS(LoadInfoArgsToLoadInfo(aArgs.loadInfo(), loadingDocument,
                                             cspToInheritLoadingDocument,
                                             getter_AddRefs(loadInfo)));

  mRedirects = std::move(aArgs.redirects());
  mRedirectResolver = std::move(aResolve);

  nsCOMPtr<nsIChannel> newChannel;
  nsresult rv =
      NS_NewChannelInternal(getter_AddRefs(newChannel), aArgs.uri(), loadInfo,
                            nullptr,     // PerformanceStorage
                            mLoadGroup,  // aLoadGroup
                            nullptr,     // aCallbacks
                            aArgs.newLoadFlags());

  RefPtr<HttpChannelChild> httpChild = do_QueryObject(newChannel);
  RefPtr<nsIChildChannel> childChannel = do_QueryObject(newChannel);
  if (NS_FAILED(rv)) {
    MOZ_DIAGNOSTIC_ASSERT(false, "NS_NewChannelInternal failed");
    return IPC_OK();
  }

  // This is used to report any errors back to the parent by calling
  // CrossProcessRedirectFinished.
  auto scopeExit = MakeScopeExit([&]() {
    Maybe<LoadInfoArgs> dummy;
    mRedirectResolver(
        Tuple<const nsresult&, const Maybe<LoadInfoArgs>&>(rv, dummy));
    mRedirectResolver = nullptr;
  });

  if (httpChild) {
    rv = httpChild->SetChannelId(aArgs.channelId());
  }
  if (NS_FAILED(rv)) {
    return IPC_OK();
  }

  rv = newChannel->SetOriginalURI(aArgs.originalURI());
  if (NS_FAILED(rv)) {
    return IPC_OK();
  }

  if (httpChild) {
    rv = httpChild->SetRedirectMode(aArgs.redirectMode());
  }
  if (NS_FAILED(rv)) {
    return IPC_OK();
  }

  newChannel->SetNotificationCallbacks(mCallbacks);

  if (aArgs.init()) {
    HttpBaseChannel::ReplacementChannelConfig config(*aArgs.init());
    HttpBaseChannel::ConfigureReplacementChannel(
        newChannel, config,
        HttpBaseChannel::ConfigureReason::DocumentChannelReplacement);
  }

  if (aArgs.contentDisposition()) {
    newChannel->SetContentDisposition(*aArgs.contentDisposition());
  }

  if (aArgs.contentDispositionFilename()) {
    newChannel->SetContentDispositionFilename(
        *aArgs.contentDispositionFilename());
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
  // We still need to copy the property bag to the DCC as the nsDocShell no
  // longer set the properties on it during ConfigureChannel. This will go away
  // once the nsDocShell no longer relies on the DCC to determine the history
  // data and is instead provided that information from the parent.
  nsHashPropertyBag::CopyFrom(this, aArgs.properties());

  // connect parent.
  if (childChannel) {
    rv = childChannel->ConnectParent(
        aArgs.registrarId());  // creates parent channel
    if (NS_FAILED(rv)) {
      return IPC_OK();
    }
  }
  mRedirectChannel = newChannel;

  rv = gHttpHandler->AsyncOnChannelRedirect(
      this, newChannel, aArgs.redirectFlags(), GetMainThreadEventTarget());

  if (NS_SUCCEEDED(rv)) {
    scopeExit.release();
  }

  // scopeExit will call CrossProcessRedirectFinished(rv) here
  return IPC_OK();
}

NS_IMETHODIMP
DocumentChannelChild::OnRedirectVerifyCallback(nsresult aStatusCode) {
  nsCOMPtr<nsIChannel> redirectChannel = std::move(mRedirectChannel);
  RedirectToRealChannelResolver redirectResolver = std::move(mRedirectResolver);

  // If we've already shut down, then just notify the parent that
  // we're done.
  if (NS_FAILED(mStatus)) {
    redirectChannel->SetNotificationCallbacks(nullptr);
    Maybe<LoadInfoArgs> dummy;
    redirectResolver(
        Tuple<const nsresult&, const Maybe<LoadInfoArgs>&>(aStatusCode, dummy));
    return NS_OK;
  }

  nsresult rv = aStatusCode;
  if (NS_SUCCEEDED(rv)) {
    if (nsCOMPtr<nsIChildChannel> childChannel =
            do_QueryInterface(redirectChannel)) {
      rv = childChannel->CompleteRedirectSetup(mListener, nullptr);
    } else {
      rv = redirectChannel->AsyncOpen(mListener);
    }
  } else {
    redirectChannel->SetNotificationCallbacks(nullptr);
  }

  Maybe<LoadInfoArgs> dummy;
  redirectResolver(
      Tuple<const nsresult&, const Maybe<LoadInfoArgs>&>(rv, dummy));

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

IPCResult DocumentChannelChild::RecvConfirmRedirect(
    const LoadInfoArgs& aLoadInfo, nsIURI* aNewUri,
    ConfirmRedirectResolver&& aResolve) {
  // This is effectively the same as AsyncOnChannelRedirect, except since we're
  // not propagating the redirect into this process, we don't have an nsIChannel
  // for the redirection and we have to do the checks manually.
  // This just checks CSP thus far, hopefully there's not much else needed.
  RefPtr<dom::Document> loadingDocument;
  mLoadInfo->GetLoadingDocument(getter_AddRefs(loadingDocument));
  RefPtr<dom::Document> cspToInheritLoadingDocument;
  nsCOMPtr<nsIContentSecurityPolicy> policy = mLoadInfo->GetCspToInherit();
  if (policy) {
    nsWeakPtr ctx =
        static_cast<nsCSPContext*>(policy.get())->GetLoadingContext();
    cspToInheritLoadingDocument = do_QueryReferent(ctx);
  }
  nsCOMPtr<nsILoadInfo> loadInfo;
  MOZ_ALWAYS_SUCCEEDS(LoadInfoArgsToLoadInfo(Some(aLoadInfo), loadingDocument,
                                             cspToInheritLoadingDocument,
                                             getter_AddRefs(loadInfo)));

  nsCOMPtr<nsIURI> originalUri;
  GetOriginalURI(getter_AddRefs(originalUri));
  Maybe<nsresult> cancelCode;
  nsresult rv = CSPService::ConsultCSPForRedirect(originalUri, aNewUri,
                                                  mLoadInfo, cancelCode);
  aResolve(Tuple<const nsresult&, const Maybe<nsresult>&>(rv, cancelCode));
  return IPC_OK();
}

mozilla::ipc::IPCResult DocumentChannelChild::RecvAttachStreamFilter(
    Endpoint<extensions::PStreamFilterParent>&& aEndpoint) {
  extensions::StreamFilterParent::Attach(this, std::move(aEndpoint));
  return IPC_OK();
}

//-----------------------------------------------------------------------------
// DocumentChannelChild::nsITraceableChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
DocumentChannelChild::SetNewListener(nsIStreamListener* aListener,
                                     nsIStreamListener** _retval) {
  NS_ENSURE_ARG_POINTER(aListener);

  nsCOMPtr<nsIStreamListener> wrapper = new nsStreamListenerWrapper(mListener);

  wrapper.forget(_retval);
  mListener = aListener;
  return NS_OK;
}

NS_IMETHODIMP
DocumentChannelChild::Cancel(nsresult aStatusCode) {
  if (mCanceled) {
    return NS_OK;
  }

  mCanceled = true;
  if (CanSend()) {
    SendCancel(aStatusCode);
  }

  ShutdownListeners(aStatusCode);

  return NS_OK;
}

NS_IMETHODIMP
DocumentChannelChild::Suspend() {
  MOZ_CRASH("If we get here, something is broken");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
DocumentChannelChild::Resume() {
  MOZ_CRASH("If we get here, something is broken");
  return NS_ERROR_NOT_IMPLEMENTED;
}

//-----------------------------------------------------------------------------
// Remainder of nsIRequest/nsIChannel.
//-----------------------------------------------------------------------------

NS_IMETHODIMP DocumentChannelChild::GetNotificationCallbacks(
    nsIInterfaceRequestor** aCallbacks) {
  nsCOMPtr<nsIInterfaceRequestor> callbacks(mCallbacks);
  callbacks.forget(aCallbacks);
  return NS_OK;
}

NS_IMETHODIMP DocumentChannelChild::SetNotificationCallbacks(
    nsIInterfaceRequestor* aNotificationCallbacks) {
  mCallbacks = aNotificationCallbacks;
  return NS_OK;
}

NS_IMETHODIMP DocumentChannelChild::GetLoadGroup(nsILoadGroup** aLoadGroup) {
  nsCOMPtr<nsILoadGroup> loadGroup(mLoadGroup);
  loadGroup.forget(aLoadGroup);
  return NS_OK;
}

NS_IMETHODIMP DocumentChannelChild::SetLoadGroup(nsILoadGroup* aLoadGroup) {
  mLoadGroup = aLoadGroup;
  return NS_OK;
}

NS_IMETHODIMP DocumentChannelChild::GetStatus(nsresult* aStatus) {
  *aStatus = mStatus;
  return NS_OK;
}

NS_IMETHODIMP DocumentChannelChild::GetName(nsACString& aResult) {
  if (!mURI) {
    aResult.Truncate();
    return NS_OK;
  }
  return mURI->GetSpec(aResult);
}

NS_IMETHODIMP DocumentChannelChild::IsPending(bool* aResult) {
  *aResult = mIsPending;
  return NS_OK;
}

NS_IMETHODIMP DocumentChannelChild::GetLoadFlags(nsLoadFlags* aLoadFlags) {
  *aLoadFlags = mLoadFlags;
  return NS_OK;
}

NS_IMETHODIMP DocumentChannelChild::SetLoadFlags(nsLoadFlags aLoadFlags) {
  mLoadFlags = aLoadFlags;
  return NS_OK;
}

NS_IMETHODIMP DocumentChannelChild::GetOriginalURI(nsIURI** aOriginalURI) {
  nsCOMPtr<nsIURI> originalURI =
      mLoadState->OriginalURI() ? mLoadState->OriginalURI() : mLoadState->URI();
  originalURI.forget(aOriginalURI);
  return NS_OK;
}

NS_IMETHODIMP DocumentChannelChild::SetOriginalURI(nsIURI* aOriginalURI) {
  MOZ_CRASH("If we get here, something is broken");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentChannelChild::GetURI(nsIURI** aURI) {
  nsCOMPtr<nsIURI> uri(mURI);
  uri.forget(aURI);
  return NS_OK;
}

NS_IMETHODIMP DocumentChannelChild::GetOwner(nsISupports** aOwner) {
  nsCOMPtr<nsISupports> owner(mOwner);
  owner.forget(aOwner);
  return NS_OK;
}

NS_IMETHODIMP DocumentChannelChild::SetOwner(nsISupports* aOwner) {
  mOwner = aOwner;
  return NS_OK;
}

NS_IMETHODIMP DocumentChannelChild::GetSecurityInfo(
    nsISupports** aSecurityInfo) {
  *aSecurityInfo = nullptr;
  return NS_OK;
}

NS_IMETHODIMP DocumentChannelChild::GetContentType(nsACString& aContentType) {
  MOZ_CRASH("If we get here, something is broken");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentChannelChild::SetContentType(
    const nsACString& aContentType) {
  MOZ_CRASH("If we get here, something is broken");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentChannelChild::GetContentCharset(
    nsACString& aContentCharset) {
  MOZ_CRASH("If we get here, something is broken");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentChannelChild::SetContentCharset(
    const nsACString& aContentCharset) {
  MOZ_CRASH("If we get here, something is broken");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentChannelChild::GetContentLength(int64_t* aContentLength) {
  MOZ_CRASH("If we get here, something is broken");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentChannelChild::SetContentLength(int64_t aContentLength) {
  MOZ_CRASH("If we get here, something is broken");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentChannelChild::Open(nsIInputStream** aStream) {
  MOZ_CRASH("If we get here, something is broken");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentChannelChild::GetContentDisposition(
    uint32_t* aContentDisposition) {
  MOZ_CRASH("If we get here, something is broken");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentChannelChild::SetContentDisposition(
    uint32_t aContentDisposition) {
  MOZ_CRASH("If we get here, something is broken");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentChannelChild::GetContentDispositionFilename(
    nsAString& aContentDispositionFilename) {
  MOZ_CRASH("If we get here, something will be broken");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentChannelChild::SetContentDispositionFilename(
    const nsAString& aContentDispositionFilename) {
  MOZ_CRASH("If we get here, something will be broken");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentChannelChild::GetContentDispositionHeader(
    nsACString& aContentDispositionHeader) {
  MOZ_CRASH("If we get here, something is broken");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentChannelChild::GetLoadInfo(nsILoadInfo** aLoadInfo) {
  nsCOMPtr<nsILoadInfo> loadInfo(mLoadInfo);
  loadInfo.forget(aLoadInfo);
  return NS_OK;
}

NS_IMETHODIMP DocumentChannelChild::SetLoadInfo(nsILoadInfo* aLoadInfo) {
  MOZ_CRASH("If we get here, something is broken");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentChannelChild::GetIsDocument(bool* aIsDocument) {
  return NS_GetIsDocumentChannel(this, aIsDocument);
}

//-----------------------------------------------------------------------------
// nsIIdentChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
DocumentChannelChild::GetChannelId(uint64_t* aChannelId) {
  *aChannelId = mChannelId;
  return NS_OK;
}

NS_IMETHODIMP
DocumentChannelChild::SetChannelId(uint64_t aChannelId) {
  mChannelId = aChannelId;
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
