/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SessionStoreChangeListener_h
#define mozilla_dom_SessionStoreChangeListener_h

#include "ErrorList.h"

#include "PLDHashTable.h"
#include "mozilla/EnumSet.h"
#include "nsCycleCollectionParticipant.h"
#include "nsHashKeys.h"
#include "nsIDOMEventListener.h"
#include "nsIObserver.h"
#include "nsISupports.h"
#include "nsITimer.h"
#include "nsINamed.h"
#include "nsHashtablesFwd.h"

#include "mozilla/EnumSet.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Result.h"

#include "mozilla/dom/WindowGlobalChild.h"

namespace mozilla::dom {

class BrowsingContext;
class Element;
class EventTarget;
class SessionStoreChild;
class WindowContext;

class SessionStoreChangeListener final : public nsINamed,
                                         public nsIObserver,
                                         public nsITimerCallback,
                                         public nsIDOMEventListener {
 public:
  NS_DECL_NSINAMED
  NS_DECL_NSIOBSERVER
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSIDOMEVENTLISTENER
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(SessionStoreChangeListener,
                                           nsIObserver)

  static already_AddRefed<SessionStoreChangeListener> Create(
      BrowsingContext* aBrowsingContext);

  void Stop();

  void UpdateEventTargets();

  void FlushSessionStore();

  enum class Change { Input, Scroll, SessionHistory, WireFrame };

  static SessionStoreChangeListener* CollectSessionStoreData(
      WindowContext* aWindowContext, const EnumSet<Change>& aChanges);

  void SetActor(SessionStoreChild* aSessionStoreChild);

  void SetEpoch(uint32_t aEpoch) { mEpoch = aEpoch; }

  uint32_t GetEpoch() const { return mEpoch; }

  bool CollectWireframe();

 private:
  void RecordChange(WindowContext* aWindowContext, EnumSet<Change> aChanges);

 public:
  using SessionStoreChangeTable =
      nsTHashMap<RefPtr<WindowContext>, EnumSet<Change>>;

 private:
  explicit SessionStoreChangeListener(BrowsingContext* aBrowsingContext);
  ~SessionStoreChangeListener() = default;

  void Init();

  EventTarget* GetEventTarget();

  void AddEventListeners();
  void RemoveEventListeners();

  void EnsureTimer();

  RefPtr<BrowsingContext> mBrowsingContext;
  RefPtr<EventTarget> mCurrentEventTarget;

  uint32_t mEpoch;
  nsCOMPtr<nsITimer> mTimer;
  RefPtr<SessionStoreChild> mSessionStoreChild;
  SessionStoreChangeTable mSessionStoreChanges;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_SessionStoreChangeListener_h
