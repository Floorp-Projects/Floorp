/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SessionStoreListener_h
#define mozilla_dom_SessionStoreListener_h

#include "nsIDOMEventListener.h"
#include "nsIPrivacyTransitionObserver.h"
#include "nsIWebProgressListener.h"

class nsITimer;

namespace mozilla {
namespace dom {

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
  void SetScrollPositionChanged() { mScrollChanged = WITH_CHANGE; }
  bool IsScrollPositionChanged() { return mScrollChanged != NO_CHANGE; }
  void GetScrollPositions(nsTArray<nsCString>& aPositions,
                          nsTArray<int32_t>& aPositionDescendants);
  void OnDocumentStart();
  void OnDocumentEnd();
  bool UpdateNeeded() {
    return mPrivateChanged || mDocCapChanged || IsScrollPositionChanged();
  }

 private:
  virtual ~ContentSessionStore() = default;
  nsCString CollectDocShellCapabilities();

  nsCOMPtr<nsIDocShell> mDocShell;
  bool mPrivateChanged;
  bool mIsPrivate;
  enum {
    NO_CHANGE,
    PAGELOADEDSTART,  // set when the state of document is STATE_START
    WITH_CHANGE,      // set when the change event is observed
  } mScrollChanged;
  bool mDocCapChanged;
  nsCString mDocCaps;
};

class TabListener : public nsIDOMEventListener,
                    public nsIObserver,
                    public nsIPrivacyTransitionObserver,
                    public nsIWebProgressListener,
                    public nsSupportsWeakReference {
 public:
  explicit TabListener(nsIDocShell* aDocShell, Element* aElement);
  nsresult Init();
  ContentSessionStore* GetSessionStore() { return mSessionStore; }
  // the function is called only when TabListener is in parent process
  bool ForceFlushFromParent(uint32_t aFlushId);
  void RemoveListeners();

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
  bool UpdateSessionStore(uint32_t aFlushId = 0);
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
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_SessionStoreListener_h
