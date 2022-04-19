/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SessionStoreChild_h
#define mozilla_dom_SessionStoreChild_h

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/dom/PSessionStoreChild.h"
#include "mozilla/dom/SessionStoreChangeListener.h"
#include "mozilla/dom/SessionStoreListener.h"
#include "mozilla/RefPtr.h"

#include "nsCycleCollectionParticipant.h"

namespace mozilla::dom {
class BrowsingContext;
class SessionStoreChangeListener;
class TabListener;

class SessionStoreChild final : public PSessionStoreChild {
 public:
  static already_AddRefed<SessionStoreChild> GetOrCreate(
      BrowsingContext* aBrowsingContext, Element* aOwnerElement = nullptr);

  static SessionStoreChild* From(WindowGlobalChild* aWindowChild);

  void SetEpoch(uint32_t aEpoch);
  void SetOwnerContent(Element* aElement);
  void Stop();
  void UpdateEventTargets();
  void UpdateSessionStore(bool aSessionHistoryUpdate = false);
  void FlushSessionStore();
  void UpdateSHistoryChanges();

  SessionStoreChangeListener* GetSessionStoreChangeListener() const {
    return mSessionStoreChangeListener;
  }

  mozilla::ipc::IPCResult RecvFlushTabState(FlushTabStateResolver&& aResolver);

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(SessionStoreChild)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(SessionStoreChild)

 private:
  SessionStoreChild(TabListener* aSessionStoreListener,
                    SessionStoreChangeListener* aSessionStoreChangeListener);
  ~SessionStoreChild() = default;

  RefPtr<TabListener> mSessionStoreListener;
  RefPtr<SessionStoreChangeListener> mSessionStoreChangeListener;
};
}  // namespace mozilla::dom

#endif  // mozilla_dom_SessionStoreChild_h
