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
#include "mozilla/dom/SessionStoreChild.h"
#include "mozilla/dom/SessionStoreUtils.h"
#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/StaticPrefs_browser.h"

#include "nsBaseHashtable.h"
#include "nsDocShell.h"
#include "nsGenericHTMLElement.h"
#include "nsIXULRuntime.h"
#include "nsPIDOMWindow.h"
#include "nsTHashMap.h"
#include "nsTHashtable.h"

using namespace mozilla;
using namespace mozilla::dom;

namespace {
constexpr auto kInput = u"input"_ns;
constexpr auto kScroll = u"mozvisualscroll"_ns;

static constexpr char kNoAutoUpdates[] =
    "browser.sessionstore.debug.no_auto_updates";
static constexpr char kInterval[] = "browser.sessionstore.interval";
static const char* kObservedPrefs[] = {kNoAutoUpdates, kInterval, nullptr};
}  // namespace

inline void ImplCycleCollectionUnlink(
    SessionStoreChangeListener::SessionStoreChangeTable& aField) {
  aField.Clear();
}

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    const SessionStoreChangeListener::SessionStoreChangeTable& aField,
    const char* aName, uint32_t aFlags = 0) {
  for (auto iter = aField.ConstIter(); !iter.Done(); iter.Next()) {
    CycleCollectionNoteChild(aCallback, iter.Key(), aName, aFlags);
  }
}

NS_IMPL_CYCLE_COLLECTION(SessionStoreChangeListener, mBrowsingContext,
                         mCurrentEventTarget, mSessionStoreChild,
                         mSessionStoreChanges)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SessionStoreChangeListener)
  NS_INTERFACE_MAP_ENTRY(nsINamed)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIObserver)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(SessionStoreChangeListener)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SessionStoreChangeListener)

NS_IMETHODIMP
SessionStoreChangeListener::GetName(nsACString& aName) {
  aName.AssignLiteral("SessionStoreChangeListener");
  return NS_OK;
}

NS_IMETHODIMP
SessionStoreChangeListener::Notify(nsITimer* aTimer) {
  FlushSessionStore();
  return NS_OK;
}

NS_IMETHODIMP
SessionStoreChangeListener::Observe(nsISupports* aSubject, const char* aTopic,
                                    const char16_t* aData) {
  FlushSessionStore();
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

  WindowContext* windowContext = inner->GetWindowContext();
  if (!windowContext) {
    return NS_OK;
  }

  RefPtr<BrowsingContext> browsingContext = windowContext->GetBrowsingContext();
  if (!browsingContext) {
    return NS_OK;
  }

  if (browsingContext->IsDynamic()) {
    return NS_OK;
  }

  nsAutoString eventType;
  aEvent->GetType(eventType);

  Change change = Change::None;

  if (eventType == kInput) {
    change = Change::Input;
  } else if (eventType == kScroll) {
    change = Change::Scroll;
  }

  RecordChange(windowContext, EnumSet(change));

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

static void CollectFormData(Document* aDocument,
                            Maybe<sessionstore::FormData>& aFormData) {
  aFormData.emplace();
  auto& formData = aFormData.ref();
  uint32_t size = SessionStoreUtils::CollectFormData(aDocument, formData);

  Element* body = aDocument->GetBody();
  if (aDocument->HasFlag(NODE_IS_EDITABLE) && body) {
    IgnoredErrorResult result;
    body->GetInnerHTML(formData.innerHTML(), result);
    size += formData.innerHTML().Length();
    if (!result.Failed()) {
      formData.hasData() = true;
    }
  }

  if (!formData.hasData()) {
    return;
  }

  nsIURI* documentURI = aDocument->GetDocumentURI();
  if (!documentURI) {
    return;
  }

  documentURI->GetSpecIgnoringRef(formData.uri());

  if (size > StaticPrefs::browser_sessionstore_dom_form_max_limit()) {
    aFormData = Nothing();
  }
}

void SessionStoreChangeListener::FlushSessionStore() {
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }

  for (auto& iter : mSessionStoreChanges) {
    WindowContext* windowContext = iter.GetKey();
    if (!windowContext) {
      continue;
    }

    RefPtr<Document> document = windowContext->GetDocument();
    if (!document) {
      continue;
    }

    EnumSet<Change> changes = iter.GetData();
    Maybe<sessionstore::FormData> maybeFormData;
    if (changes.contains(Change::Input)) {
      CollectFormData(document, maybeFormData);
    }

    Maybe<nsPoint> maybeScroll;
    PresShell* presShell = document->GetPresShell();

    if (presShell && changes.contains(Change::Scroll)) {
      maybeScroll = Some(presShell->GetVisualViewportOffset());
    }

    mSessionStoreChild->SendIncrementalSessionStoreUpdate(
        windowContext->GetBrowsingContext(), maybeFormData, maybeScroll,
        mEpoch);
  }

  mSessionStoreChanges.Clear();

  mSessionStoreChild->UpdateSessionStore(mCollectSessionHistory);
  mCollectSessionHistory = false;
}

/* static */
SessionStoreChangeListener* SessionStoreChangeListener::CollectSessionStoreData(
    WindowContext* aWindowContext, const EnumSet<Change>& aChanges) {
  SessionStoreChild* sessionStoreChild =
      SessionStoreChild::From(aWindowContext->GetWindowGlobalChild());
  if (!sessionStoreChild) {
    return nullptr;
  }

  SessionStoreChangeListener* sessionStoreChangeListener =
      sessionStoreChild->GetSessionStoreChangeListener();

  if (!sessionStoreChangeListener) {
    return nullptr;
  }

  sessionStoreChangeListener->RecordChange(aWindowContext, aChanges);

  return sessionStoreChangeListener;
}

/* static */
void SessionStoreChangeListener::FlushAllSessionStoreData(
    WindowContext* aWindowContext) {
  EnumSet<Change> allChanges(Change::Input, Change::Scroll);
  SessionStoreChangeListener* listener =
      CollectSessionStoreData(aWindowContext, allChanges);
  if (listener) {
    listener->FlushSessionStore();
  }
}

void SessionStoreChangeListener::SetActor(
    SessionStoreChild* aSessionStoreChild) {
  mSessionStoreChild = aSessionStoreChild;
}

void SessionStoreChangeListener::CollectWireframe() {
  if (auto* docShell = nsDocShell::Cast(mBrowsingContext->GetDocShell())) {
    if (docShell->CollectWireframe()) {
      mCollectSessionHistory = true;
    }
  }
}

void SessionStoreChangeListener::RecordChange(WindowContext* aWindowContext,
                                              EnumSet<Change> aChange) {
  EnsureTimer();

  Unused << mSessionStoreChanges.WithEntryHandle(
      aWindowContext, [&](auto entryHandle) -> EnumSet<Change>& {
        if (entryHandle) {
          *entryHandle += aChange;
          return *entryHandle;
        }

        return entryHandle.Insert(aChange);
      });
}

SessionStoreChangeListener::SessionStoreChangeListener(
    BrowsingContext* aBrowsingContext)
    : mBrowsingContext(aBrowsingContext),
      mEpoch(aBrowsingContext->GetSessionStoreEpoch()) {}

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

void SessionStoreChangeListener::EnsureTimer() {
  if (mTimer) {
    return;
  }

  if (!StaticPrefs::browser_sessionstore_debug_no_auto_updates()) {
    auto result = NS_NewTimerWithCallback(
        this, StaticPrefs::browser_sessionstore_interval(),
        nsITimer::TYPE_ONE_SHOT);
    if (result.isErr()) {
      return;
    }

    mTimer = result.unwrap();
  }
}
