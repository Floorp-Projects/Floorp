/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_BounceTrackingState_h
#define mozilla_BounceTrackingState_h

#include "BounceTrackingRecord.h"
#include "mozilla/WeakPtr.h"
#include "mozilla/OriginAttributes.h"
#include "nsIPrincipal.h"
#include "nsIWeakReferenceUtils.h"
#include "nsStringFwd.h"
#include "nsIWebProgressListener.h"
#include "nsWeakReference.h"

class nsIChannel;
class nsITimer;
class nsIPrincipal;

namespace mozilla {

class BounceTrackingProtection;

namespace dom {
class CanonicalBrowsingContext;
class BrowsingContext;
class BrowsingContextWebProgress;
}  // namespace dom

/**
 * This class manages the bounce tracking state for a given tab. It is attached
 * to top-level CanonicalBrowsingContexts.
 */
class BounceTrackingState : public nsIWebProgressListener,
                            public nsSupportsWeakReference,
                            public SupportsWeakPtr {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWEBPROGRESSLISTENER

  // Gets or creates an existing BrowsingContextState keyed by browserId. May
  // return nullptr if the given web progress / browsing context is not suitable
  // (see ShouldCreateBounceTrackingStateForWebProgress).
  static already_AddRefed<BounceTrackingState> GetOrCreate(
      dom::BrowsingContextWebProgress* aWebProgress, nsresult& aRv);

  // Reset state for all BounceTrackingState instances this includes resetting
  // BounceTrackingRecords and cancelling any running timers.
  static void ResetAll();
  static void ResetAllForOriginAttributes(
      const OriginAttributes& aOriginAttributes);
  static void ResetAllForOriginAttributesPattern(
      const OriginAttributesPattern& aPattern);

  const Maybe<BounceTrackingRecord>& GetBounceTrackingRecord();

  void ResetBounceTrackingRecord();

  // Callback for when we received a response from the server and are about to
  // create a document for the response. Calls into
  // BounceTrackingState::OnResponseReceived.
  [[nodiscard]] nsresult OnDocumentStartRequest(nsIChannel* aChannel);

  // At the start of a navigation, either initialize a new bounce tracking
  // record, or append a client-side redirect to the current bounce tracking
  // record.
  // Should only be called for top level content navigations.
  [[nodiscard]] nsresult OnStartNavigation(
      nsIPrincipal* aTriggeringPrincipal,
      const bool aHasValidUserGestureActivation);

  // Record sites which have written cookies in the current extended
  // navigation.
  [[nodiscard]] nsresult OnCookieWrite(const nsACString& aSiteHost);

  // Whether the given BrowsingContext should hold a BounceTrackingState
  // instance to monitor bounce tracking navigations.
  static bool ShouldCreateBounceTrackingStateForBC(
      dom::CanonicalBrowsingContext* aBrowsingContext);

  // Whether the given principal should be tracked for bounce tracking.
  static bool ShouldTrackPrincipal(nsIPrincipal* aPrincipal);

  // Check if there is a BounceTrackingState which current browsing context is
  // associated with aSiteHost.
  // This is an approximation for checking if a given site is currently loaded
  // in the top level context, e.g. in a tab. See Bug 1842047 for adding a more
  // accurate check that calls into the browser implementations.
  [[nodiscard]] static nsresult HasBounceTrackingStateForSite(
      const nsACString& aSiteHost, bool& aResult);

  // Get the currently associated BrowsingContext. Returns nullptr if it has not
  // been attached yet.
  already_AddRefed<dom::BrowsingContext> CurrentBrowsingContext();

  uint64_t GetBrowserId() { return mBrowserId; }

  const OriginAttributes& OriginAttributesRef();

  // Create a string that describes this object. Used for logging.
  nsCString Describe();

  // Record sites which have accessed storage in the current extended
  // navigation.
  [[nodiscard]] nsresult OnStorageAccess(nsIPrincipal* aPrincipal);

 private:
  explicit BounceTrackingState();
  virtual ~BounceTrackingState();

  bool mIsInitialized{false};

  uint64_t mBrowserId{};

  // OriginAttributes associated with the browser this state is attached to.
  OriginAttributes mOriginAttributes;

  // Reference to the BounceTrackingProtection singleton.
  RefPtr<BounceTrackingProtection> mBounceTrackingProtection;

  // Record to keep track of extended navigation data. Reset on extended
  // navigation end.
  Maybe<BounceTrackingRecord> mBounceTrackingRecord;

  // Timer to wait to wait for a client redirect after a navigation ends.
  RefPtr<nsITimer> mClientBounceDetectionTimeout;

  // Reset state for all BounceTrackingState instances this includes resetting
  // BounceTrackingRecords and cancelling any running timers.
  // Optionally filter by OriginAttributes or OriginAttributesPattern.
  static void Reset(const OriginAttributes* aOriginAttributes,
                    const OriginAttributesPattern* aPattern);

  // Whether the given web progress should hold a BounceTrackingState
  // instance to monitor bounce tracking navigations.
  static bool ShouldCreateBounceTrackingStateForWebProgress(
      dom::BrowsingContextWebProgress* aWebProgress);

  // Init to be called after creation, attaches nsIWebProgressListener.
  [[nodiscard]] nsresult Init(dom::BrowsingContextWebProgress* aWebProgress);

  // When the response is received at the end of a navigation, fill the
  // bounce set.
  [[nodiscard]] nsresult OnResponseReceived(
      const nsTArray<nsCString>& aSiteList);

  // When the document is loaded at the end of a navigation, update the
  // final host.
  [[nodiscard]] nsresult OnDocumentLoaded(nsIPrincipal* aDocumentPrincipal);

  // Record sites which have activated service workers in the current
  // extended navigation.
  [[nodiscard]] nsresult OnServiceWorkerActivation();
};

}  // namespace mozilla

#endif
