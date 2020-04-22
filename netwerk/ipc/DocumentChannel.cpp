/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocumentChannel.h"

#include "SerializedLoadContext.h"
#include "mozIThirdPartyUtil.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/nsCSPContext.h"
#include "mozilla/extensions/StreamFilterParent.h"
#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/net/DocumentChannelChild.h"
#include "mozilla/net/HttpChannelChild.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/net/ParentProcessDocumentChannel.h"
#include "mozilla/net/UrlClassifierCommon.h"
#include "nsContentSecurityManager.h"
#include "nsDocShell.h"
#include "nsDocShellLoadState.h"
#include "nsHttpHandler.h"
#include "nsIInputStreamChannel.h"
#include "nsNetUtil.h"
#include "nsQueryObject.h"
#include "nsSerializationHelper.h"
#include "nsStreamListenerWrapper.h"
#include "nsStringStream.h"
#include "nsURLHelper.h"

using namespace mozilla::dom;
using namespace mozilla::ipc;

extern mozilla::LazyLogModule gDocumentChannelLog;
#define LOG(fmt) MOZ_LOG(gDocumentChannelLog, mozilla::LogLevel::Verbose, fmt)

namespace mozilla {
namespace net {

//-----------------------------------------------------------------------------
// DocumentChannel::nsISupports

NS_IMPL_ADDREF(DocumentChannel)
NS_IMPL_RELEASE(DocumentChannel)

NS_INTERFACE_MAP_BEGIN(DocumentChannel)
  NS_INTERFACE_MAP_ENTRY(nsIRequest)
  NS_INTERFACE_MAP_ENTRY(nsIChannel)
  NS_INTERFACE_MAP_ENTRY(nsIIdentChannel)
  NS_INTERFACE_MAP_ENTRY(nsITraceableChannel)
  NS_INTERFACE_MAP_ENTRY_CONCRETE(DocumentChannel)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIRequest)
NS_INTERFACE_MAP_END

DocumentChannel::DocumentChannel(nsDocShellLoadState* aLoadState,
                                 net::LoadInfo* aLoadInfo,
                                 nsLoadFlags aLoadFlags, uint32_t aCacheKey)
    : mAsyncOpenTime(TimeStamp::Now()),
      mLoadState(aLoadState),
      mCacheKey(aCacheKey),
      mLoadFlags(aLoadFlags),
      mURI(aLoadState->URI()),
      mLoadInfo(aLoadInfo) {
  LOG(("DocumentChannel ctor [this=%p, uri=%s]", this,
       aLoadState->URI()->GetSpecOrDefault().get()));
  RefPtr<nsHttpHandler> handler = nsHttpHandler::GetInstance();
  uint64_t channelId;
  Unused << handler->NewChannelId(channelId);
  mChannelId = channelId;
}

NS_IMETHODIMP
DocumentChannel::AsyncOpen(nsIStreamListener* aListener) {
  MOZ_CRASH("If we get here, something is broken");
  return NS_ERROR_NOT_IMPLEMENTED;
}

void DocumentChannel::ShutdownListeners(nsresult aStatusCode) {
  LOG(("DocumentChannel ShutdownListeners [this=%p, status=%" PRIx32 "]", this,
       static_cast<uint32_t>(aStatusCode)));
  mStatus = aStatusCode;

  nsCOMPtr<nsIStreamListener> listener = mListener;
  if (listener) {
    listener->OnStartRequest(this);
  }

  mIsPending = false;

  listener = mListener;  // it might have changed!
  if (listener) {
    listener->OnStopRequest(this, aStatusCode);
  }
  mListener = nullptr;
  mCallbacks = nullptr;

  if (mLoadGroup) {
    mLoadGroup->RemoveRequest(this, nullptr, aStatusCode);
    mLoadGroup = nullptr;
  }

  DeleteIPDL();
}

void DocumentChannel::DisconnectChildListeners(
    const nsresult& aStatus, const nsresult& aLoadGroupStatus) {
  MOZ_ASSERT(NS_FAILED(aStatus));
  mStatus = aLoadGroupStatus;
  // Make sure we remove from the load group before
  // setting mStatus, as existing tests expect the
  // status to be successful when we disconnect.
  if (mLoadGroup) {
    mLoadGroup->RemoveRequest(this, nullptr, aStatus);
    mLoadGroup = nullptr;
  }

  ShutdownListeners(aStatus);
}

nsDocShell* DocumentChannel::GetDocShell() {
  nsCOMPtr<nsILoadContext> loadContext;
  NS_QueryNotificationCallbacks(this, loadContext);
  if (!loadContext) {
    return nullptr;
  }
  nsCOMPtr<mozIDOMWindowProxy> domWindow;
  loadContext->GetAssociatedWindow(getter_AddRefs(domWindow));
  if (!domWindow) {
    return nullptr;
  }
  auto* pDomWindow = nsPIDOMWindowOuter::From(domWindow);
  nsIDocShell* docshell = pDomWindow->GetDocShell();
  return nsDocShell::Cast(docshell);
}

// Changes here should also be made in
// E10SUtils.documentChannelPermittedForURI().
static bool URIUsesDocChannel(nsIURI* aURI) {
  if (SchemeIsJavascript(aURI) || NS_IsAboutBlank(aURI)) {
    return false;
  }

  nsCString spec = aURI->GetSpecOrDefault();
  return !spec.EqualsLiteral("about:printpreview") &&
         !spec.EqualsLiteral("about:crashcontent");
}

/* static */
already_AddRefed<DocumentChannel> DocumentChannel::CreateDocumentChannel(
    nsDocShellLoadState* aLoadState, class LoadInfo* aLoadInfo,
    nsLoadFlags aLoadFlags, nsIInterfaceRequestor* aNotificationCallbacks,
    uint32_t aCacheKey) {
  RefPtr<DocumentChannel> channel;
  if (XRE_IsContentProcess()) {
    channel =
        new DocumentChannelChild(aLoadState, aLoadInfo, aLoadFlags, aCacheKey);
  } else {
    channel = new ParentProcessDocumentChannel(aLoadState, aLoadInfo,
                                               aLoadFlags, aCacheKey);
  }
  channel->SetNotificationCallbacks(aNotificationCallbacks);
  return channel.forget();
}

bool DocumentChannel::CanUseDocumentChannel(nsDocShellLoadState* aLoadState) {
  MOZ_ASSERT(aLoadState);

  if (XRE_IsParentProcess() &&
      !StaticPrefs::browser_tabs_documentchannel_ppdc()) {
    return false;
  }

  // We want to use DocumentChannel if we're using a supported scheme. Sandboxed
  // srcdoc loads break due to failing assertions after changing processes, and
  // non-sandboxed srcdoc loads need to share the same principal object as their
  // outer document (and must load in the same process), which breaks if we
  // serialize to the parent process.
  return StaticPrefs::browser_tabs_documentchannel() &&
         !aLoadState->HasLoadFlags(nsDocShell::INTERNAL_LOAD_FLAGS_IS_SRCDOC) &&
         URIUsesDocChannel(aLoadState->URI());
}

//-----------------------------------------------------------------------------
// DocumentChannel::nsITraceableChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
DocumentChannel::SetNewListener(nsIStreamListener* aListener,
                                nsIStreamListener** _retval) {
  LOG(("DocumentChannel SetNewListener [this=%p, aListener=%p]", this,
       aListener));
  NS_ENSURE_ARG_POINTER(aListener);

  nsCOMPtr<nsIStreamListener> wrapper = new nsStreamListenerWrapper(mListener);

  wrapper.forget(_retval);
  mListener = aListener;
  return NS_OK;
}

NS_IMETHODIMP
DocumentChannel::Cancel(nsresult aStatusCode) {
  MOZ_CRASH("If we get here, something is broken");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
DocumentChannel::Suspend() {
  MOZ_CRASH("If we get here, something is broken");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
DocumentChannel::Resume() {
  MOZ_CRASH("If we get here, something is broken");
  return NS_ERROR_NOT_IMPLEMENTED;
}

//-----------------------------------------------------------------------------
// Remainder of nsIRequest/nsIChannel.
//-----------------------------------------------------------------------------

NS_IMETHODIMP DocumentChannel::GetNotificationCallbacks(
    nsIInterfaceRequestor** aCallbacks) {
  nsCOMPtr<nsIInterfaceRequestor> callbacks(mCallbacks);
  callbacks.forget(aCallbacks);
  return NS_OK;
}

NS_IMETHODIMP DocumentChannel::SetNotificationCallbacks(
    nsIInterfaceRequestor* aNotificationCallbacks) {
  mCallbacks = aNotificationCallbacks;
  return NS_OK;
}

NS_IMETHODIMP DocumentChannel::GetLoadGroup(nsILoadGroup** aLoadGroup) {
  nsCOMPtr<nsILoadGroup> loadGroup(mLoadGroup);
  loadGroup.forget(aLoadGroup);
  return NS_OK;
}

NS_IMETHODIMP DocumentChannel::SetLoadGroup(nsILoadGroup* aLoadGroup) {
  mLoadGroup = aLoadGroup;
  return NS_OK;
}

NS_IMETHODIMP DocumentChannel::GetStatus(nsresult* aStatus) {
  *aStatus = mStatus;
  return NS_OK;
}

NS_IMETHODIMP DocumentChannel::GetName(nsACString& aResult) {
  if (!mURI) {
    aResult.Truncate();
    return NS_OK;
  }
  nsCString spec;
  nsresult rv = mURI->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  aResult.AssignLiteral("documentchannel:");
  aResult.Append(spec);
  return NS_OK;
}

NS_IMETHODIMP DocumentChannel::IsPending(bool* aResult) {
  *aResult = mIsPending;
  return NS_OK;
}

NS_IMETHODIMP DocumentChannel::GetLoadFlags(nsLoadFlags* aLoadFlags) {
  *aLoadFlags = mLoadFlags;
  return NS_OK;
}

NS_IMETHODIMP
DocumentChannel::GetTRRMode(nsIRequest::TRRMode* aTRRMode) {
  return GetTRRModeImpl(aTRRMode);
}

NS_IMETHODIMP
DocumentChannel::SetTRRMode(nsIRequest::TRRMode aTRRMode) {
  return SetTRRModeImpl(aTRRMode);
}

NS_IMETHODIMP DocumentChannel::SetLoadFlags(nsLoadFlags aLoadFlags) {
  mLoadFlags = aLoadFlags;
  return NS_OK;
}

NS_IMETHODIMP DocumentChannel::GetOriginalURI(nsIURI** aOriginalURI) {
  nsCOMPtr<nsIURI> originalURI =
      mLoadState->OriginalURI() ? mLoadState->OriginalURI() : mLoadState->URI();
  originalURI.forget(aOriginalURI);
  return NS_OK;
}

NS_IMETHODIMP DocumentChannel::SetOriginalURI(nsIURI* aOriginalURI) {
  MOZ_CRASH("If we get here, something is broken");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentChannel::GetURI(nsIURI** aURI) {
  nsCOMPtr<nsIURI> uri(mURI);
  uri.forget(aURI);
  return NS_OK;
}

NS_IMETHODIMP DocumentChannel::GetOwner(nsISupports** aOwner) {
  nsCOMPtr<nsISupports> owner(mOwner);
  owner.forget(aOwner);
  return NS_OK;
}

NS_IMETHODIMP DocumentChannel::SetOwner(nsISupports* aOwner) {
  mOwner = aOwner;
  return NS_OK;
}

NS_IMETHODIMP DocumentChannel::GetSecurityInfo(nsISupports** aSecurityInfo) {
  *aSecurityInfo = nullptr;
  return NS_OK;
}

NS_IMETHODIMP DocumentChannel::GetContentType(nsACString& aContentType) {
  MOZ_CRASH("If we get here, something is broken");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentChannel::SetContentType(const nsACString& aContentType) {
  MOZ_CRASH("If we get here, something is broken");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentChannel::GetContentCharset(nsACString& aContentCharset) {
  MOZ_CRASH("If we get here, something is broken");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentChannel::SetContentCharset(
    const nsACString& aContentCharset) {
  MOZ_CRASH("If we get here, something is broken");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentChannel::GetContentLength(int64_t* aContentLength) {
  MOZ_CRASH("If we get here, something is broken");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentChannel::SetContentLength(int64_t aContentLength) {
  MOZ_CRASH("If we get here, something is broken");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentChannel::Open(nsIInputStream** aStream) {
  MOZ_CRASH("If we get here, something is broken");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentChannel::GetContentDisposition(
    uint32_t* aContentDisposition) {
  MOZ_CRASH("If we get here, something is broken");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentChannel::SetContentDisposition(
    uint32_t aContentDisposition) {
  MOZ_CRASH("If we get here, something is broken");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentChannel::GetContentDispositionFilename(
    nsAString& aContentDispositionFilename) {
  MOZ_CRASH("If we get here, something will be broken");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentChannel::SetContentDispositionFilename(
    const nsAString& aContentDispositionFilename) {
  MOZ_CRASH("If we get here, something will be broken");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentChannel::GetContentDispositionHeader(
    nsACString& aContentDispositionHeader) {
  MOZ_CRASH("If we get here, something is broken");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentChannel::GetLoadInfo(nsILoadInfo** aLoadInfo) {
  nsCOMPtr<nsILoadInfo> loadInfo(mLoadInfo);
  loadInfo.forget(aLoadInfo);
  return NS_OK;
}

NS_IMETHODIMP DocumentChannel::SetLoadInfo(nsILoadInfo* aLoadInfo) {
  MOZ_CRASH("If we get here, something is broken");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP DocumentChannel::GetIsDocument(bool* aIsDocument) {
  return NS_GetIsDocumentChannel(this, aIsDocument);
}

NS_IMETHODIMP DocumentChannel::GetCanceled(bool* aCanceled) {
  *aCanceled = mCanceled;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsIIdentChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
DocumentChannel::GetChannelId(uint64_t* aChannelId) {
  *aChannelId = mChannelId;
  return NS_OK;
}

NS_IMETHODIMP
DocumentChannel::SetChannelId(uint64_t aChannelId) {
  mChannelId = aChannelId;
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla

#undef LOG
