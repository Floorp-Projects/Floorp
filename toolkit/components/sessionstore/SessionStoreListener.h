/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SessionStoreListener_h
#define mozilla_dom_SessionStoreListener_h

#include "SessionStoreData.h"
#include "nsIDOMEventListener.h"
#include "nsIObserver.h"
#include "nsIPrivacyTransitionObserver.h"
#include "nsIWebProgressListener.h"
#include "nsWeakReference.h"

class nsITimer;

namespace mozilla::dom {

class ContentSessionStore {
 public:
  explicit ContentSessionStore(nsIDocShell* aDocShell);
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(ContentSessionStore)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(ContentSessionStore)

  void OnPrivateModeChanged(bool aEnabled);
  bool IsDocCapChanged() { return mDocCapChanged; }
  nsCString GetDocShellCaps();
  bool IsPrivateChanged() { return mPrivateChanged; }
  bool GetPrivateModeEnabled();

  void SetSHistoryChanged();
  // request "collect sessionHistory" which is happened in the parent process
  bool GetAndClearSHistoryChanged() {
    bool ret = mSHistoryChanged;
    mSHistoryChanged = false;
    return ret;
  }

  void OnDocumentStart();
  void OnDocumentEnd();
  bool UpdateNeeded() {
    return mPrivateChanged || mDocCapChanged || mSHistoryChanged;
  }

 private:
  virtual ~ContentSessionStore() = default;
  nsCString CollectDocShellCapabilities();

  nsCOMPtr<nsIDocShell> mDocShell;
  bool mPrivateChanged;
  bool mIsPrivate;
  bool mDocCapChanged;
  nsCString mDocCaps;
  // mSHistoryChanged means there are history changes which are found
  // in the child process. The flag is set when
  //    1. webProgress changes to STATE_START
  //    2. webProgress changes to STATE_STOP
  //    3. receiving "DOMTitleChanged" event
  bool mSHistoryChanged;
};

class TabListener : public nsIDOMEventListener,
                    public nsIObserver,
                    public nsIPrivacyTransitionObserver,
                    public nsIWebProgressListener,
                    public nsSupportsWeakReference {
 public:
  explicit TabListener(nsIDocShell* aDocShell, Element* aElement);
  EventTarget* GetEventTarget();
  nsresult Init();
  ContentSessionStore* GetSessionStore() { return mSessionStore; }
  // the function is called only when TabListener is in parent process
  void ForceFlushFromParent();
  void RemoveListeners();
  void SetEpoch(uint32_t aEpoch) { mEpoch = aEpoch; }
  uint32_t GetEpoch() { return mEpoch; }
  void UpdateSHistoryChanges() { AddTimerForUpdate(); }
  void SetOwnerContent(Element* aElement);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(TabListener, nsIDOMEventListener)

  NS_DECL_NSIDOMEVENTLISTENER
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIPRIVACYTRANSITIONOBSERVER
  NS_DECL_NSIWEBPROGRESSLISTENER

 private:
  static void TimerCallback(nsITimer* aTimer, void* aClosure);
  void AddTimerForUpdate();
  void StopTimerForUpdate();
  void AddEventListeners();
  void RemoveEventListeners();
  void UpdateSessionStore(bool aIsFlush = false);
  virtual ~TabListener();

  nsCOMPtr<nsIDocShell> mDocShell;
  RefPtr<ContentSessionStore> mSessionStore;
  RefPtr<mozilla::dom::Element> mOwnerContent;
  bool mProgressListenerRegistered;
  bool mEventListenerRegistered;
  bool mPrefObserverRegistered;
  // Timer used to update data
  nsCOMPtr<nsITimer> mUpdatedTimer;
  bool mTimeoutDisabled;
  int32_t mUpdateInterval;
  uint32_t mEpoch;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_SessionStoreListener_h
