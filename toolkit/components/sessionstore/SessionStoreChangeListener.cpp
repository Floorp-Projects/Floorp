/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SessionStoreChangeListener.h"

#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/SessionStoreDataCollector.h"
#include "mozilla/dom/SessionStoreUtils.h"
#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/Preferences.h"

#include "nsDocShell.h"
#include "nsPIDOMWindow.h"

namespace {
constexpr auto kInput = u"input"_ns;
constexpr auto kScroll = u"mozvisualscroll"_ns;

static constexpr char kNoAutoUpdates[] =
    "browser.sessionstore.debug.no_auto_updates";
static constexpr char kInterval[] = "browser.sessionstore.interval";
static const char* kObservedPrefs[] = {kNoAutoUpdates, kInterval, nullptr};
}  // namespace

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION(SessionStoreChangeListener, mBrowsingContext,
                         mCurrentEventTarget)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SessionStoreChangeListener)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIObserver)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(SessionStoreChangeListener)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SessionStoreChangeListener)

NS_IMETHODIMP
SessionStoreChangeListener::Observe(nsISupports* aSubject, const char* aTopic,
                                    const char16_t* aData) {
  Flush();
  return NS_OK;
}

NS_IMETHODIMP
SessionStoreChangeListener::HandleEvent(dom::Event* aEvent) {
  EventTarget* target = aEvent->GetTarget();
  if (!target) {
    return NS_OK;
  }

  nsCOMPtr<nsPIDOMWindowInner> inner =
      do_QueryInterface(target->GetOwnerGlobal());
  if (!inner) {
    return NS_OK;
  }

  WindowGlobalChild* windowChild = inner->GetWindowGlobalChild();
  if (!windowChild) {
    return NS_OK;
  }

  RefPtr<BrowsingContext> browsingContext = windowChild->BrowsingContext();
  if (!browsingContext) {
    return NS_OK;
  }

  bool dynamic = false;
  BrowsingContext* current = browsingContext;
  while (current) {
    if ((dynamic = current->CreatedDynamically())) {
      break;
    }
    current = current->GetParent();
  }

  if (dynamic) {
    return NS_OK;
  }

  nsAutoString eventType;
  aEvent->GetType(eventType);

  RefPtr<SessionStoreDataCollector> collector =
      SessionStoreDataCollector::CollectSessionStoreData(windowChild);

  if (!collector) {
    return NS_OK;
  }

  if (eventType == kInput) {
    collector->RecordInputChange();
  } else if (eventType == kScroll) {
    collector->RecordScrollChange();
  }

  return NS_OK;
}

/* static */ already_AddRefed<SessionStoreChangeListener>
SessionStoreChangeListener::Create(BrowsingContext* aBrowsingContext) {
  MOZ_RELEASE_ASSERT(SessionStoreUtils::NATIVE_LISTENER);
  if (!aBrowsingContext) {
    return nullptr;
  }

  RefPtr<SessionStoreChangeListener> listener =
      new SessionStoreChangeListener(aBrowsingContext);
  listener->Init();

  return listener.forget();
}

void SessionStoreChangeListener::Stop() {
  RemoveEventListeners();
  Preferences::RemoveObservers(this, kObservedPrefs);
}

void SessionStoreChangeListener::UpdateEventTargets() {
  RemoveEventListeners();
  AddEventListeners();
}

void SessionStoreChangeListener::Flush() {
  mBrowsingContext->FlushSessionStore();
}

SessionStoreChangeListener::SessionStoreChangeListener(
    BrowsingContext* aBrowsingContext)
    : mBrowsingContext(aBrowsingContext) {}

void SessionStoreChangeListener::Init() {
  AddEventListeners();

  Preferences::AddStrongObservers(this, kObservedPrefs);
}

EventTarget* SessionStoreChangeListener::GetEventTarget() {
  if (mBrowsingContext->GetDOMWindow()) {
    return mBrowsingContext->GetDOMWindow()->GetChromeEventHandler();
  }

  return nullptr;
}

void SessionStoreChangeListener::AddEventListeners() {
  if (EventTarget* target = GetEventTarget()) {
    target->AddSystemEventListener(kInput, this, false);
    target->AddSystemEventListener(kScroll, this, false);
    mCurrentEventTarget = target;
  }
}

void SessionStoreChangeListener::RemoveEventListeners() {
  if (mCurrentEventTarget) {
    mCurrentEventTarget->RemoveSystemEventListener(kInput, this, false);
    mCurrentEventTarget->RemoveSystemEventListener(kScroll, this, false);
  }

  mCurrentEventTarget = nullptr;
}

}  // namespace mozilla::dom
