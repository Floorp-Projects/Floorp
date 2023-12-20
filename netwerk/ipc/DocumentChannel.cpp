/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/net/DocumentChannel.h"

#include <inttypes.h>
#include <utility>
#include "mozIDOMWindow.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/Logging.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/Document.h"
#include "mozilla/net/DocumentChannelChild.h"
#include "mozilla/net/ParentProcessDocumentChannel.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsDocShell.h"
#include "nsDocShellLoadState.h"
#include "nsHttpHandler.h"
#include "nsIContentPolicy.h"
#include "nsIInterfaceRequestor.h"
#include "nsILoadContext.h"
#include "nsILoadGroup.h"
#include "nsILoadInfo.h"
#include "nsIStreamListener.h"
#include "nsIURI.h"
#include "nsMimeTypes.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"
#include "nsPIDOMWindowInlines.h"
#include "nsStringFwd.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "nscore.h"

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
  NS_INTERFACE_MAP_ENTRY_CONCRETE(DocumentChannel)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIRequest)
NS_INTERFACE_MAP_END

DocumentChannel::DocumentChannel(nsDocShellLoadState* aLoadState,
                                 net::LoadInfo* aLoadInfo,
                                 nsLoadFlags aLoadFlags, uint32_t aCacheKey,
                                 bool aUriModified,
                                 bool aIsEmbeddingBlockedError)
    : mLoadState(aLoadState),
      mCacheKey(aCacheKey),
      mLoadFlags(aLoadFlags),
      mURI(aLoadState->URI()),
      mLoadInfo(aLoadInfo),
      mUriModified(aUriModified),
      mIsEmbeddingBlockedError(aIsEmbeddingBlockedError) {
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
  nsCOMPtr<nsILoadGroup> loadGroup = mLoadGroup;

  mListener = nullptr;
  mLoadGroup = nullptr;
  mCallbacks = nullptr;

  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "DocumentChannel::ShutdownListeners", [=, self = RefPtr{this}] {
        if (listener) {
          listener->OnStopRequest(self, aStatusCode);
        }

        if (loadGroup) {
          loadGroup->RemoveRequest(self, nullptr, aStatusCode);
        }
      }));

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

static bool URIUsesDocChannel(nsIURI* aURI) {
  if (SchemeIsJavascript(aURI)) {
    return false;
  }

  nsCString spec = aURI->GetSpecOrDefault();
  return !spec.EqualsLiteral("about:crashcontent");
}

bool DocumentChannel::CanUseDocumentChannel(nsIURI* aURI) {
  // We want to use DocumentChannel if we're using a supported scheme.
  return URIUsesDocChannel(aURI);
}

/* static */
already_AddRefed<DocumentChannel> DocumentChannel::CreateForDocument(
    nsDocShellLoadState* aLoadState, class LoadInfo* aLoadInfo,
    nsLoadFlags aLoadFlags, nsIInterfaceRequestor* aNotificationCallbacks,
    uint32_t aCacheKey, bool aUriModified, bool aIsEmbeddingBlockedError) {
  RefPtr<DocumentChannel> channel;
  if (XRE_IsContentProcess()) {
    channel =
        new DocumentChannelChild(aLoadState, aLoadInfo, aLoadFlags, aCacheKey,
                                 aUriModified, aIsEmbeddingBlockedError);
  } else {
    channel = new ParentProcessDocumentChannel(
        aLoadState, aLoadInfo, aLoadFlags, aCacheKey, aUriModified,
        aIsEmbeddingBlockedError);
  }
  channel->SetNotificationCallbacks(aNotificationCallbacks);
  return channel.forget();
}

/* static */
already_AddRefed<DocumentChannel> DocumentChannel::CreateForObject(
    nsDocShellLoadState* aLoadState, class LoadInfo* aLoadInfo,
    nsLoadFlags aLoadFlags, nsIInterfaceRequestor* aNotificationCallbacks) {
  return CreateForDocument(aLoadState, aLoadInfo, aLoadFlags,
                           aNotificationCallbacks, 0, false, false);
}

NS_IMETHODIMP DocumentChannel::SetCanceledReason(const nsACString& aReason) {
  return SetCanceledReasonImpl(aReason);
}

NS_IMETHODIMP DocumentChannel::GetCanceledReason(nsACString& aReason) {
  return GetCanceledReasonImpl(aReason);
}

NS_IMETHODIMP DocumentChannel::CancelWithReason(nsresult aStatus,
                                                const nsACString& aReason) {
  return CancelWithReasonImpl(aStatus, aReason);
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
  // Setting load flags for TYPE_OBJECT is OK, so long as the channel to parent
  // isn't opened yet, or we're only setting the `LOAD_DOCUMENT_URI` flag.
  auto contentPolicy = mLoadInfo->GetExternalContentPolicyType();
  if (contentPolicy == ExtContentPolicy::TYPE_OBJECT) {
    if (mWasOpened) {
      MOZ_DIAGNOSTIC_ASSERT(
          aLoadFlags == (mLoadFlags | nsIChannel::LOAD_DOCUMENT_URI),
          "After the channel has been opened, can only set the "
          "`LOAD_DOCUMENT_URI` flag.");
    }
    mLoadFlags = aLoadFlags;
    return NS_OK;
  }

  MOZ_CRASH_UNSAFE_PRINTF(
      "DocumentChannel::SetLoadFlags: Don't set flags after creation "
      "(differing flags %x != %x)",
      (mLoadFlags ^ aLoadFlags) & mLoadFlags,
      (mLoadFlags ^ aLoadFlags) & aLoadFlags);
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

NS_IMETHODIMP DocumentChannel::GetSecurityInfo(
    nsITransportSecurityInfo** aSecurityInfo) {
  *aSecurityInfo = nullptr;
  return NS_OK;
}

NS_IMETHODIMP DocumentChannel::GetContentType(nsACString& aContentType) {
  // We may be trying to load HTML object data, and have determined that we're
  // going to be performing a document load. In that case, fake the "text/html"
  // content type for nsObjectLoadingContent.
  if ((mLoadFlags & nsIRequest::LOAD_HTML_OBJECT_DATA) &&
      (mLoadFlags & nsIChannel::LOAD_DOCUMENT_URI)) {
    aContentType = TEXT_HTML;
    return NS_OK;
  }

  NS_ERROR("If we get here, something is broken");
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

//-----------------------------------------------------------------------------
// Helpers
//-----------------------------------------------------------------------------

uint64_t InnerWindowIDForExtantDoc(nsDocShell* docShell) {
  if (!docShell) {
    return 0;
  }

  Document* doc = docShell->GetExtantDocument();
  if (!doc) {
    return 0;
  }

  return doc->InnerWindowID();
}

}  // namespace net
}  // namespace mozilla

#undef LOG
