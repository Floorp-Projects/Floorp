/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SessionStoreDataCollector.h"

#include "mozilla/PresShell.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/sessionstore/SessionStoreTypes.h"
#include "mozilla/dom/SessionStoreUtils.h"
#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/dom/WindowGlobalParent.h"

#include "nsGenericHTMLElement.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION(SessionStoreDataCollector, mWindowChild, mTimer)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SessionStoreDataCollector)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsITimerCallback)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(SessionStoreDataCollector)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SessionStoreDataCollector)

NS_IMETHODIMP
SessionStoreDataCollector::Notify(nsITimer* aTimer) {
  Collect();
  return NS_OK;
}

/* static */ already_AddRefed<SessionStoreDataCollector>
SessionStoreDataCollector::CollectSessionStoreData(
    WindowGlobalChild* aWindowChild) {
  MOZ_RELEASE_ASSERT(SessionStoreUtils::NATIVE_LISTENER);
  MOZ_DIAGNOSTIC_ASSERT(aWindowChild);

  RefPtr<SessionStoreDataCollector> listener =
      aWindowChild->GetSessionStoreDataCollector();

  uint32_t epoch =
      aWindowChild->BrowsingContext()->Top()->GetSessionStoreEpoch();
  if (listener) {
    MOZ_DIAGNOSTIC_ASSERT_IF(
        !StaticPrefs::browser_sessionstore_debug_no_auto_updates(),
        listener->mTimer);
    if (listener->mEpoch == epoch) {
      return listener.forget();
    }
    if (listener->mTimer) {
      listener->mTimer->Cancel();
    }
  }

  listener = new SessionStoreDataCollector(aWindowChild, epoch);

  if (!StaticPrefs::browser_sessionstore_debug_no_auto_updates()) {
    auto result = NS_NewTimerWithCallback(
        listener, StaticPrefs::browser_sessionstore_interval(),
        nsITimer::TYPE_ONE_SHOT);
    if (result.isErr()) {
      return nullptr;
    }

    listener->mTimer = result.unwrap();
  }

  aWindowChild->SetSessionStoreDataCollector(listener);
  return listener.forget();
}

void SessionStoreDataCollector::RecordInputChange() { mInputChanged = true; }

void SessionStoreDataCollector::RecordScrollChange() { mScrollChanged = true; }

void SessionStoreDataCollector::Flush() { Collect(); }

void SessionStoreDataCollector::Cancel() {
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }

  mWindowChild->SetSessionStoreDataCollector(nullptr);
}

void SessionStoreDataCollector::Collect() {
  if (this != mWindowChild->GetSessionStoreDataCollector()) {
    return;
  }

  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }

  nsGlobalWindowInner* inner = mWindowChild->GetWindowGlobal();
  if (!inner) {
    return;
  }

  Document* document = inner->GetDocument();
  if (!document) {
    return;
  }

  Maybe<sessionstore::FormData> maybeFormData;
  if (mInputChanged) {
    maybeFormData.emplace();
    auto& formData = maybeFormData.ref();
    SessionStoreUtils::CollectFormData(document, formData);

    Element* body = document->GetBody();
    if (document->HasFlag(NODE_IS_EDITABLE) && body) {
      IgnoredErrorResult result;
      body->GetInnerHTML(formData.innerHTML(), result);
      if (!result.Failed()) {
        formData.hasData() = true;
      }
    }
  }

  PresShell* presShell = document->GetPresShell();
  Maybe<nsPoint> maybeScroll;
  if (mScrollChanged && presShell) {
    maybeScroll = Some(presShell->GetVisualViewportOffset());
  }

  if (!mWindowChild->CanSend()) {
    return;
  }

  if (RefPtr<WindowGlobalParent> windowParent =
          mWindowChild->GetParentActor()) {
    windowParent->WriteFormDataAndScrollToSessionStore(maybeFormData,
                                                       maybeScroll, mEpoch);
  } else {
    mWindowChild->SendUpdateSessionStore(maybeFormData, maybeScroll, mEpoch);
  }

  mWindowChild->SetSessionStoreDataCollector(nullptr);
}

SessionStoreDataCollector::SessionStoreDataCollector(
    WindowGlobalChild* aWindowChild, uint32_t aEpoch)
    : mWindowChild(aWindowChild),
      mTimer(nullptr),
      mEpoch(aEpoch),
      mInputChanged(false),
      mScrollChanged(false) {}

SessionStoreDataCollector::~SessionStoreDataCollector() {
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }
}

}  // namespace mozilla::dom
