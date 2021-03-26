/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SessionStoreDataCollector_h
#define mozilla_dom_SessionStoreDataCollector_h

#include "ErrorList.h"

#include "nsITimer.h"

#include "nsCycleCollectionParticipant.h"

#include "mozilla/RefPtr.h"

namespace mozilla::dom {

class BrowserChild;
class EventTarget;
class WindowGlobalChild;

namespace sessionstore {
class FormData;
}

class SessionStoreDataCollector final : public nsITimerCallback {
 public:
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(SessionStoreDataCollector)

  enum class Change { Input, Scroll };

  static already_AddRefed<SessionStoreDataCollector> CollectSessionStoreData(
      WindowGlobalChild* aWindowChild);

  void RecordInputChange();
  void RecordScrollChange();

  void Flush();

  void Cancel();

 private:
  void Collect();

  nsresult Apply(Maybe<sessionstore::FormData>&& aFormData,
                 Maybe<nsPoint>&& aScroll);

  SessionStoreDataCollector(WindowGlobalChild* aWindowChild, uint32_t aEpoch);
  ~SessionStoreDataCollector();

  RefPtr<WindowGlobalChild> mWindowChild;
  nsCOMPtr<nsITimer> mTimer;

  uint32_t mEpoch;
  bool mInputChanged : 1;
  bool mScrollChanged : 1;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_SessionStoreDataCollector_h
