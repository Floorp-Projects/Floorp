/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SessionStoreChild.h"

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/InProcessChild.h"
#include "mozilla/dom/InProcessParent.h"
#include "mozilla/dom/BrowserSessionStore.h"
#include "mozilla/dom/SessionStoreChangeListener.h"
#include "mozilla/dom/SessionStoreParent.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/ipc/Endpoint.h"
#include "nsCOMPtr.h"
#include "nsFrameLoader.h"

using namespace mozilla;
using namespace mozilla::dom;

class nsIDocShell;

static already_AddRefed<TabListener> CreateTabListener(nsIDocShell* aDocShell) {
  RefPtr<TabListener> tabListener =
      mozilla::MakeRefPtr<TabListener>(aDocShell, nullptr);
  nsresult rv = tabListener->Init();
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  return tabListener.forget();
}

already_AddRefed<SessionStoreChild> SessionStoreChild::GetOrCreate(
    BrowsingContext* aBrowsingContext, Element* aOwnerElement) {
  RefPtr<TabListener> tabListener =
      CreateTabListener(aBrowsingContext->GetDocShell());
  if (!tabListener) {
    return nullptr;
  }

  RefPtr<SessionStoreChangeListener> sessionStoreChangeListener =
      SessionStoreChangeListener::Create(aBrowsingContext);
  if (!sessionStoreChangeListener) {
    return nullptr;
  }

  RefPtr<SessionStoreChild> sessionStoreChild =
      new SessionStoreChild(tabListener, sessionStoreChangeListener);

  sessionStoreChangeListener->SetActor(sessionStoreChild);

  if (XRE_IsParentProcess()) {
    MOZ_DIAGNOSTIC_ASSERT(aOwnerElement);
    InProcessChild* inProcessChild = InProcessChild::Singleton();
    InProcessParent* inProcessParent = InProcessParent::Singleton();
    if (!inProcessChild || !inProcessParent) {
      return nullptr;
    }

    RefPtr<BrowserSessionStore> sessionStore =
        BrowserSessionStore::GetOrCreate(aBrowsingContext->Canonical()->Top());
    if (!sessionStore) {
      return nullptr;
    }

    CanonicalBrowsingContext* browsingContext = aBrowsingContext->Canonical();
    RefPtr<SessionStoreParent> sessionStoreParent =
        new SessionStoreParent(browsingContext, sessionStore);
    ManagedEndpoint<PSessionStoreParent> endpoint =
        inProcessChild->OpenPSessionStoreEndpoint(sessionStoreChild);
    inProcessParent->BindPSessionStoreEndpoint(std::move(endpoint),
                                               sessionStoreParent);
  } else {
    MOZ_DIAGNOSTIC_ASSERT(!aOwnerElement);
    RefPtr<BrowserChild> browserChild =
        BrowserChild::GetFrom(aBrowsingContext->GetDOMWindow());

    MOZ_DIAGNOSTIC_ASSERT(browserChild);
    MOZ_DIAGNOSTIC_ASSERT(aBrowsingContext->IsInProcess());
    sessionStoreChild = static_cast<SessionStoreChild*>(
        browserChild->SendPSessionStoreConstructor(sessionStoreChild));
  }

  return sessionStoreChild.forget();
}

/* static */
SessionStoreChild* SessionStoreChild::From(WindowGlobalChild* aWindowChild) {
  if (!aWindowChild) {
    return nullptr;
  }

  // If `aWindowChild` is inprocess
  if (RefPtr<BrowserChild> browserChild = aWindowChild->GetBrowserChild()) {
    return browserChild->GetSessionStoreChild();
  }

  if (XRE_IsContentProcess()) {
    return nullptr;
  }

  WindowGlobalParent* windowParent = aWindowChild->WindowContext()->Canonical();
  if (!windowParent) {
    return nullptr;
  }

  RefPtr<nsFrameLoader> frameLoader = windowParent->GetRootFrameLoader();
  if (!frameLoader) {
    return nullptr;
  }

  return frameLoader->GetSessionStoreChild();
}

SessionStoreChild::SessionStoreChild(
    TabListener* aSessionStoreListener,
    SessionStoreChangeListener* aSessionStoreChangeListener)
    : mSessionStoreListener(aSessionStoreListener),
      mSessionStoreChangeListener(aSessionStoreChangeListener) {}

void SessionStoreChild::SetEpoch(uint32_t aEpoch) {
  if (mSessionStoreListener) {
    mSessionStoreListener->SetEpoch(aEpoch);
  }

  if (mSessionStoreChangeListener) {
    mSessionStoreChangeListener->SetEpoch(aEpoch);
  }
}

void SessionStoreChild::SetOwnerContent(Element* aElement) {
  if (mSessionStoreChangeListener) {
    mSessionStoreChangeListener->FlushSessionStore();
  }

  if (!aElement) {
    return;
  }

  if (mSessionStoreListener) {
    mSessionStoreListener->SetOwnerContent(aElement);
  }
}

void SessionStoreChild::Stop() {
  if (mSessionStoreListener) {
    mSessionStoreListener->RemoveListeners();
    mSessionStoreListener = nullptr;
  }

  if (mSessionStoreChangeListener) {
    mSessionStoreChangeListener->Stop();
  }
}

void SessionStoreChild::UpdateEventTargets() {
  if (mSessionStoreChangeListener) {
    mSessionStoreChangeListener->UpdateEventTargets();
  }
}

void SessionStoreChild::UpdateSessionStore(bool aSessionHistoryUpdate,
                                           const MaybeSessionStoreZoom& aZoom) {
  if (!mSessionStoreListener) {
    // This is the case when we're shutting down, and expect a final update.
    SessionStoreUpdate(Nothing(), Nothing(), Nothing(), aSessionHistoryUpdate,
                       0);
    return;
  }

  RefPtr<ContentSessionStore> store = mSessionStoreListener->GetSessionStore();

  Maybe<nsCString> docShellCaps;
  if (store->IsDocCapChanged()) {
    docShellCaps.emplace(store->GetDocShellCaps());
  }

  Maybe<bool> privatedMode;
  if (store->IsPrivateChanged()) {
    privatedMode.emplace(store->GetPrivateModeEnabled());
  }

  SessionStoreUpdate(
      docShellCaps, privatedMode, aZoom,
      store->GetAndClearSHistoryChanged() || aSessionHistoryUpdate,
      mSessionStoreListener->GetEpoch());
}

void SessionStoreChild::FlushSessionStore() {
  if (mSessionStoreChangeListener) {
    mSessionStoreChangeListener->FlushSessionStore();
  }
}

void SessionStoreChild::UpdateSHistoryChanges() {
  if (mSessionStoreListener) {
    mSessionStoreListener->UpdateSHistoryChanges();
  }
}

mozilla::ipc::IPCResult SessionStoreChild::RecvFlushTabState(
    FlushTabStateResolver&& aResolver) {
  if (mSessionStoreChangeListener) {
    mSessionStoreChangeListener->FlushSessionStore();
  }
  aResolver(true);

  return IPC_OK();
}

void SessionStoreChild::SessionStoreUpdate(
    const Maybe<nsCString>& aDocShellCaps, const Maybe<bool>& aPrivatedMode,
    const MaybeSessionStoreZoom& aZoom, const bool aNeedCollectSHistory,
    const uint32_t& aEpoch) {
  if (XRE_IsContentProcess()) {
    Unused << SendSessionStoreUpdate(aDocShellCaps, aPrivatedMode, aZoom,
                                     aNeedCollectSHistory, aEpoch);
  } else if (SessionStoreParent* sessionStoreParent =
                 static_cast<SessionStoreParent*>(
                     InProcessChild::ParentActorFor(this))) {
    sessionStoreParent->SessionStoreUpdate(aDocShellCaps, aPrivatedMode, aZoom,
                                           aNeedCollectSHistory, aEpoch);
  }
}

void SessionStoreChild::IncrementalSessionStoreUpdate(
    const MaybeDiscarded<BrowsingContext>& aBrowsingContext,
    const Maybe<FormData>& aFormData, const Maybe<nsPoint>& aScrollPosition,
    uint32_t aEpoch) {
  if (XRE_IsContentProcess()) {
    Unused << SendIncrementalSessionStoreUpdate(aBrowsingContext, aFormData,
                                                aScrollPosition, aEpoch);
  } else if (SessionStoreParent* sessionStoreParent =
                 static_cast<SessionStoreParent*>(
                     InProcessChild::ParentActorFor(this))) {
    sessionStoreParent->IncrementalSessionStoreUpdate(
        aBrowsingContext, aFormData, aScrollPosition, aEpoch);
  }
}

void SessionStoreChild::ResetSessionStore(
    const MaybeDiscarded<BrowsingContext>& aBrowsingContext, uint32_t aEpoch) {
  if (XRE_IsContentProcess()) {
    Unused << SendResetSessionStore(aBrowsingContext, aEpoch);
  } else if (SessionStoreParent* sessionStoreParent =
                 static_cast<SessionStoreParent*>(
                     InProcessChild::ParentActorFor(this))) {
    sessionStoreParent->ResetSessionStore(aBrowsingContext, aEpoch);
  }
}

NS_IMPL_CYCLE_COLLECTION(SessionStoreChild, mSessionStoreListener,
                         mSessionStoreChangeListener)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(SessionStoreChild, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(SessionStoreChild, Release)
