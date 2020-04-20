/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BrowsingContextDocumentChannel.h"
#include "DocumentLoadListener.h"
#include "nsIBrowser.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/RemoteWebProgress.h"
#include "mozilla/dom/RemoteWebProgressRequest.h"
#include "mozilla/dom/WindowGlobalParent.h"

using namespace mozilla::dom;

extern mozilla::LazyLogModule gDocumentChannelLog;
#define LOG(fmt) MOZ_LOG(gDocumentChannelLog, mozilla::LogLevel::Verbose, fmt)

namespace mozilla {
namespace net {

already_AddRefed<nsIBrowser> BrowsingContextDocumentChannel::GetBrowser() {
  CanonicalBrowsingContext* bc = mParent->GetBrowsingContext();
  if (!bc || !bc->IsTopContent()) {
    NS_ASSERTION(false, "No BC, or subframe?");
    return nullptr;
  }

  nsCOMPtr<nsIBrowser> browser;
  RefPtr<Element> currentElement = bc->GetEmbedderElement();

  // In Responsive Design Mode, mFrameElement will be the <iframe mozbrowser>,
  // but we want the <xul:browser> that it is embedded in.
  while (currentElement) {
    browser = currentElement->AsBrowser();
    if (browser) {
      break;
    }

    BrowsingContext* browsingContext =
        currentElement->OwnerDoc()->GetBrowsingContext();
    currentElement =
        browsingContext ? browsingContext->GetEmbedderElement() : nullptr;
  }

  return browser.forget();
}

void BrowsingContextDocumentChannel::FireStateChange(uint32_t aStateFlags,
                                                     nsresult aStatus) {
  nsCOMPtr<nsIBrowser> browser = GetBrowser();
  if (!browser) {
    NS_ASSERTION(false, "Failed to find browser element!");
    return;
  }

  nsCOMPtr<nsIWebProgress> manager;
  nsresult rv = browser->GetRemoteWebProgressManager(getter_AddRefs(manager));
  if (NS_FAILED(rv)) {
    NS_ASSERTION(false, "Failed to get remote web progress manager?");
    return;
  }

  nsCOMPtr<nsIWebProgressListener> listener = do_QueryInterface(manager);
  if (!listener) {
    NS_ASSERTION(false, "Failed to get remote web progress listener");
    // We are no longer remote so we cannot forward this event.
    return;
  }

  nsCOMPtr<nsILoadInfo> loadInfo = mParent->GetChannel()->LoadInfo();
  nsCOMPtr<nsIWebProgress> webProgress = new RemoteWebProgress(
      manager, loadInfo->GetOuterWindowID(),
      /* aInnerDOMWindowID = */ 0, mParent->GetLoadType(), true, true);

  nsCOMPtr<nsIURI> uri, originalUri;
  mParent->GetChannel()->GetURI(getter_AddRefs(uri));
  mParent->GetChannel()->GetOriginalURI(getter_AddRefs(originalUri));
  nsCString matchedList;

  nsCOMPtr<nsIRequest> request = MakeAndAddRef<RemoteWebProgressRequest>(
      uri, originalUri, matchedList, Nothing());

  NS_DispatchToMainThread(
      NS_NewRunnableFunction("DocumentLoadListener::FireStateChange", [=]() {
        Unused << listener->OnStateChange(webProgress, request, aStateFlags,
                                          aStatus);
      }));
}

void BrowsingContextDocumentChannel::SetNavigating(bool aNavigating) {
  nsCOMPtr<nsIBrowser> browser = GetBrowser();
  if (!browser) {
    NS_ASSERTION(false, "Failed to find browser element!");
    return;
  }

  NS_DispatchToMainThread(
      NS_NewRunnableFunction("DocumentLoadListener::SetNavigating",
                             [=]() { browser->SetIsNavigating(aNavigating); }));
}

BrowsingContextDocumentChannel::BrowsingContextDocumentChannel(
    dom::CanonicalBrowsingContext* aBrowsingContext) {
  mParent = new DocumentLoadListener(aBrowsingContext, this);
}

// Creates the channel, and then calls AsyncOpen on it.
bool BrowsingContextDocumentChannel::Open(nsDocShellLoadState* aLoadState,
                                          uint64_t aOuterWindowId,
                                          bool aSetNavigating) {
  MOZ_LOG(gDocumentChannelLog, mozilla::LogLevel::Debug,
          ("BrowsingContextDocumentChannel::Open [this=%p]", this));
  SetNavigating(aSetNavigating);

  if (mParent->OpenFromParent(aLoadState, aOuterWindowId)) {
    FireStateChange(nsIWebProgressListener::STATE_START |
                        nsIWebProgressListener::STATE_IS_DOCUMENT |
                        nsIWebProgressListener::STATE_IS_REQUEST |
                        nsIWebProgressListener::STATE_IS_WINDOW |
                        nsIWebProgressListener::STATE_IS_NETWORK,
                    NS_OK);
    SetNavigating(false);
    return true;
  }
  SetNavigating(false);
  mParent = nullptr;
  return false;
}

void BrowsingContextDocumentChannel::DisconnectChildListeners(
    nsresult aStatus, nsresult aLoadGroupStatus, bool aSwitchingToNewProcess) {
  // If we're not switching the load to a new process, then it is
  // finished, and we should fire a state change to notify observers.
  // Normally the docshell would fire this, and it would get filtered
  // out by BrowserParent if needed.
  if (!aSwitchingToNewProcess) {
    FireStateChange(nsIWebProgressListener::STATE_STOP |
                        nsIWebProgressListener::STATE_IS_WINDOW |
                        nsIWebProgressListener::STATE_IS_NETWORK,
                    aStatus);
  }
  mParent = nullptr;
}

void BrowsingContextDocumentChannel::Delete() {
  if (mParent) {
    RefPtr<DocumentLoadListener> parent = std::move(mParent);
    parent->DocumentChannelBridgeDisconnected();
  }
}

RefPtr<PDocumentChannelParent::RedirectToRealChannelPromise>
BrowsingContextDocumentChannel::RedirectToRealChannel(
    nsTArray<ipc::Endpoint<extensions::PStreamFilterParent>>&&
        aStreamFilterEndpoints,
    uint32_t aRedirectFlags, uint32_t aLoadFlags) {
  MOZ_ASSERT_UNREACHABLE("RedirectToRealChannel unsupported!");
  return PDocumentChannelParent::RedirectToRealChannelPromise::CreateAndResolve(
      NS_ERROR_UNEXPECTED, __func__);
}

base::ProcessId BrowsingContextDocumentChannel::OtherPid() const {
  // We intentionally return 0 (rather than the OwnerProcessId of the browsing
  // context, since this is used to determine the process from which the load
  // originated.
  return 0;
}

}  // namespace net
}  // namespace mozilla

#undef LOG
