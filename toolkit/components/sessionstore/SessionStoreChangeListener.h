/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SessionStoreChangeListener_h
#define mozilla_dom_SessionStoreChangeListener_h

#include "ErrorList.h"

#include "nsIDOMEventListener.h"
#include "nsIObserver.h"

#include "nsCycleCollectionParticipant.h"

#include "mozilla/RefPtr.h"
#include "mozilla/Result.h"

namespace mozilla::dom {

class Element;
class EventTarget;
class SessionStoreDataCollector;
class BrowsingContext;

class SessionStoreChangeListener final : public nsIObserver,
                                         public nsIDOMEventListener {
 public:
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIDOMEVENTLISTENER
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(SessionStoreChangeListener,
                                           nsIObserver)

  static already_AddRefed<SessionStoreChangeListener> Create(
      BrowsingContext* aBrowsingContext);

  void Stop();

  void UpdateEventTargets();

  void Flush();

 private:
  explicit SessionStoreChangeListener(BrowsingContext* aBrowsingContext);
  ~SessionStoreChangeListener() = default;

  void Init();

  EventTarget* GetEventTarget();

  void AddEventListeners();
  void RemoveEventListeners();

  RefPtr<BrowsingContext> mBrowsingContext;
  RefPtr<EventTarget> mCurrentEventTarget;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_SessionStoreChangeListener_h
