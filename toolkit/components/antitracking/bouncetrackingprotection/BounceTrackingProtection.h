/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef mozilla_BounceTrackingProtection_h__
#define mozilla_BounceTrackingProtection_h__

#include "mozilla/Logging.h"
#include "mozilla/MozPromise.h"
#include "mozilla/glean/GleanMetrics.h"
#include "nsIBounceTrackingProtection.h"
#include "nsIClearDataService.h"

class nsIPrincipal;
class nsITimer;

namespace mozilla {

class BounceTrackingState;
class BounceTrackingStateGlobal;
class BounceTrackingProtectionStorage;
class ContentBlockingAllowListCache;
class OriginAttributes;

extern LazyLogModule gBounceTrackingProtectionLog;

class BounceTrackingProtection final : public nsIBounceTrackingProtection {
  NS_DECL_ISUPPORTS
  NS_DECL_NSIBOUNCETRACKINGPROTECTION

 public:
  static already_AddRefed<BounceTrackingProtection> GetSingleton();

  // This algorithm is called when detecting the end of an extended navigation.
  // This could happen if a user-initiated navigation is detected in process
  // navigation start for bounce tracking, or if the client bounce detection
  // timer expires after process response received for bounce tracking without
  // observing a client redirect.
  [[nodiscard]] nsresult RecordStatefulBounces(
      BounceTrackingState* aBounceTrackingState);

  // Stores a user activation flag with a timestamp for the given principal. The
  // timestamp defaults to the current time, but can be overridden via
  // aActivationTime.
  [[nodiscard]] nsresult RecordUserActivation(
      nsIPrincipal* aPrincipal, Maybe<PRTime> aActivationTime = Nothing());

  // Clears expired user interaction flags for the given state global. If
  // aStateGlobal == nullptr, clears expired user interaction flags for all
  // state globals.
  [[nodiscard]] nsresult ClearExpiredUserInteractions(
      BounceTrackingStateGlobal* aStateGlobal = nullptr);

 private:
  BounceTrackingProtection();
  ~BounceTrackingProtection() = default;

  // Timer which periodically runs PurgeBounceTrackers.
  nsCOMPtr<nsITimer> mBounceTrackingPurgeTimer;

  // Storage for user agent globals.
  RefPtr<BounceTrackingProtectionStorage> mStorage;

  // Clear state for classified bounce trackers. To be called on an interval.
  using PurgeBounceTrackersMozPromise =
      MozPromise<nsTArray<nsCString>, nsresult, true>;
  RefPtr<PurgeBounceTrackersMozPromise> PurgeBounceTrackers();

  // Pending clear operations are stored as ClearDataMozPromise, one per host.
  using ClearDataMozPromise = MozPromise<nsCString, uint32_t, true>;

  // Clear state for classified bounce trackers for a specific state global.
  // aClearPromises is populated with promises for each host that is cleared.
  [[nodiscard]] nsresult PurgeBounceTrackersForStateGlobal(
      BounceTrackingStateGlobal* aStateGlobal,
      ContentBlockingAllowListCache& aContentBlockingAllowList,
      nsTArray<RefPtr<ClearDataMozPromise>>& aClearPromises);

  // Whether a purge operation is currently in progress. This avoids running
  // multiple purge operations at the same time.
  bool mPurgeInProgress = false;

  // Wraps nsIClearDataCallback in MozPromise.
  class ClearDataCallback final : public nsIClearDataCallback {
   public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICLEARDATACALLBACK

    explicit ClearDataCallback(ClearDataMozPromise::Private* aPromise,
                               const nsACString& aHost);

   private:
    virtual ~ClearDataCallback();

    nsCString mHost;

    void RecordClearDurationTelemetry();

    glean::TimerId mClearDurationTimer;
    RefPtr<ClearDataMozPromise::Private> mPromise;
  };

  // Imports user activation permissions from permission manager if needed. This
  // is important so we don't purge data for sites the user has interacted with
  // before the feature was enabled.
  [[nodiscard]] nsresult MaybeMigrateUserInteractionPermissions();
};

}  // namespace mozilla

#endif
