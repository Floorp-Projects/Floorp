/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BounceTrackingProtection.h"

#include "BounceTrackingState.h"
#include "BounceTrackingRecord.h"

#include "ErrorList.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Logging.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozilla/dom/BrowsingContextBinding.h"
#include "mozilla/dom/Promise.h"
#include "nsHashPropertyBag.h"
#include "nsIClearDataService.h"
#include "nsIObserverService.h"
#include "nsIPrincipal.h"
#include "nsISupports.h"
#include "nsServiceManagerUtils.h"
#include "nscore.h"
#include "prtime.h"
#include "mozilla/dom/BrowsingContext.h"
#include "xpcpublic.h"

#define TEST_OBSERVER_MSG_RECORD_BOUNCES_FINISHED "test-record-bounces-finished"

namespace mozilla {

NS_IMPL_ISUPPORTS(BounceTrackingProtection, nsIBounceTrackingProtection);

LazyLogModule gBounceTrackingProtectionLog("BounceTrackingProtection");

static StaticRefPtr<BounceTrackingProtection> sBounceTrackingProtection;

static constexpr uint32_t TRACKER_PURGE_FLAGS =
    nsIClearDataService::CLEAR_ALL_CACHES | nsIClearDataService::CLEAR_COOKIES |
    nsIClearDataService::CLEAR_DOM_STORAGES |
    nsIClearDataService::CLEAR_CLIENT_AUTH_REMEMBER_SERVICE |
    nsIClearDataService::CLEAR_EME | nsIClearDataService::CLEAR_MEDIA_DEVICES |
    nsIClearDataService::CLEAR_STORAGE_ACCESS |
    nsIClearDataService::CLEAR_AUTH_TOKENS |
    nsIClearDataService::CLEAR_AUTH_CACHE;

// static
already_AddRefed<BounceTrackingProtection>
BounceTrackingProtection::GetSingleton() {
  MOZ_ASSERT(XRE_IsParentProcess());

  if (!StaticPrefs::privacy_bounceTrackingProtection_enabled_AtStartup()) {
    return nullptr;
  }

  if (!sBounceTrackingProtection) {
    sBounceTrackingProtection = new BounceTrackingProtection();

    RunOnShutdown([] { sBounceTrackingProtection = nullptr; });
  }

  return do_AddRef(sBounceTrackingProtection);
}

BounceTrackingProtection::BounceTrackingProtection() {
  MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug, ("constructor"));

  // Schedule timer for tracker purging. The timer interval is determined by
  // pref.
  uint32_t purgeTimerPeriod = StaticPrefs::
      privacy_bounceTrackingProtection_bounceTrackingPurgeTimerPeriodSec();

  // The pref can be set to 0 to disable interval purging.
  if (purgeTimerPeriod == 0) {
    return;
  }

  MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
          ("Scheduling mBounceTrackingPurgeTimer. Interval: %d seconds.",
           purgeTimerPeriod));

  DebugOnly<nsresult> rv = NS_NewTimerWithCallback(
      getter_AddRefs(mBounceTrackingPurgeTimer),
      [](auto) {
        if (!sBounceTrackingProtection) {
          return;
        }
        sBounceTrackingProtection->PurgeBounceTrackers()->Then(
            GetMainThreadSerialEventTarget(), __func__,
            [] {
              MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
                      ("%s: PurgeBounceTrackers finished after timer call.",
                       __FUNCTION__));
            },
            [] { NS_WARNING("RunPurgeBounceTrackers failed"); });
      },
      purgeTimerPeriod * PR_MSEC_PER_SEC, nsITimer::TYPE_REPEATING_SLACK,
      "mBounceTrackingPurgeTimer");

  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "Failed to schedule timer for RunPurgeBounceTrackers.");
}

nsresult BounceTrackingProtection::RecordStatefulBounces(
    BounceTrackingState* aBounceTrackingState) {
  NS_ENSURE_ARG_POINTER(aBounceTrackingState);

  MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
          ("%s: aBounceTrackingState: %s", __FUNCTION__,
           aBounceTrackingState->Describe().get()));

  // Assert: navigable’s bounce tracking record is not null.
  BounceTrackingRecord* record =
      aBounceTrackingState->GetBounceTrackingRecord();
  NS_ENSURE_TRUE(record, NS_ERROR_FAILURE);

  // For each host in navigable’s bounce tracking record's bounce set:
  for (const nsACString& host : record->GetBounceHosts()) {
    // If host equals navigable’s bounce tracking record's initial host,
    // continue.
    if (host == record->GetInitialHost()) {
      MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
              ("%s: Skip host == initialHost: %s", __FUNCTION__,
               PromiseFlatCString(host).get()));
      continue;
    }
    // If host equals navigable’s bounce tracking record's final host, continue.
    if (host == record->GetFinalHost()) {
      MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
              ("%s: Skip host == finalHost: %s", __FUNCTION__,
               PromiseFlatCString(host).get()));
      continue;
    }

    // If user activation map contains host, continue.
    if (mUserActivation.Contains(host)) {
      MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
              ("%s: Skip host with recent user activation: %s", __FUNCTION__,
               PromiseFlatCString(host).get()));
      continue;
    }

    // If stateful bounce tracking map contains host, continue.
    if (mBounceTrackers.Contains(host)) {
      MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
              ("%s: Skip already existing host: %s", __FUNCTION__,
               PromiseFlatCString(host).get()));
      continue;
    }

    // If navigable’s bounce tracking record's storage access set does not
    // contain host, continue.
    if (StaticPrefs::
            privacy_bounceTrackingProtection_requireStatefulBounces() &&
        !record->GetStorageAccessHosts().Contains(host)) {
      MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
              ("%s: Skip host without storage access: %s", __FUNCTION__,
               PromiseFlatCString(host).get()));
      continue;
    }

    // Set stateful bounce tracking map[host] to topDocument’s relevant settings
    // object's current wall time.
    PRTime now = PR_Now();
    MOZ_ASSERT(!mBounceTrackers.Contains(host));
    mBounceTrackers.InsertOrUpdate(host, now);

    MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Info,
            ("%s: Added candidate to mBounceTrackers: %s, Time: %" PRIu64,
             __FUNCTION__, PromiseFlatCString(host).get(),
             static_cast<uint64_t>(now)));
  }

  // Set navigable’s bounce tracking record to null.
  aBounceTrackingState->ResetBounceTrackingRecord();
  MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
          ("%s: Done, reset aBounceTrackingState: %s", __FUNCTION__,
           aBounceTrackingState->Describe().get()));

  // If running in test automation, dispatch an observer message indicating
  // we're finished recording bounces.
  if (StaticPrefs::privacy_bounceTrackingProtection_enableTestMode()) {
    nsCOMPtr<nsIObserverService> obsSvc =
        mozilla::services::GetObserverService();
    NS_ENSURE_TRUE(obsSvc, NS_ERROR_FAILURE);

    RefPtr<nsHashPropertyBag> props = new nsHashPropertyBag();

    nsresult rv = props->SetPropertyAsUint64(
        u"browserId"_ns, aBounceTrackingState->GetBrowserId());
    NS_ENSURE_SUCCESS(rv, rv);

    rv = obsSvc->NotifyObservers(
        ToSupports(props), TEST_OBSERVER_MSG_RECORD_BOUNCES_FINISHED, nullptr);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult BounceTrackingProtection::RecordUserActivation(
    nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(XRE_IsParentProcess());

  NS_ENSURE_ARG_POINTER(aPrincipal);
  NS_ENSURE_TRUE(aPrincipal->GetIsContentPrincipal(), NS_ERROR_FAILURE);

  nsAutoCString siteHost;
  nsresult rv = aPrincipal->GetBaseDomain(siteHost);
  NS_ENSURE_SUCCESS(rv, rv);

  MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Info,
          ("%s: siteHost: %s", __FUNCTION__, siteHost.get()));

  bool hasRemoved = mBounceTrackers.Remove(siteHost);
  if (hasRemoved) {
    MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
            ("%s: Removed bounce tracking candidate due to user activation: %s",
             __FUNCTION__, siteHost.get()));
  }
  MOZ_ASSERT(!mBounceTrackers.Contains(siteHost));

  mUserActivation.InsertOrUpdate(siteHost, PR_Now());

  return NS_OK;
}

NS_IMETHODIMP
BounceTrackingProtection::GetBounceTrackerCandidateHosts(
    nsTArray<nsCString>& aCandidates) {
  for (const nsACString& host : mBounceTrackers.Keys()) {
    aCandidates.AppendElement(host);
  }

  return NS_OK;
}

NS_IMETHODIMP
BounceTrackingProtection::GetUserActivationHosts(nsTArray<nsCString>& aHosts) {
  for (const nsACString& host : mUserActivation.Keys()) {
    aHosts.AppendElement(host);
  }

  return NS_OK;
}

NS_IMETHODIMP
BounceTrackingProtection::Reset() {
  BounceTrackingState::ResetAll();

  mBounceTrackers.Clear();
  mUserActivation.Clear();

  return NS_OK;
}

NS_IMETHODIMP
BounceTrackingProtection::TestRunPurgeBounceTrackers(
    JSContext* aCx, mozilla::dom::Promise** aPromise) {
  NS_ENSURE_ARG_POINTER(aCx);
  NS_ENSURE_ARG_POINTER(aPromise);

  nsIGlobalObject* globalObject = xpc::CurrentNativeGlobal(aCx);
  if (!globalObject) {
    return NS_ERROR_UNEXPECTED;
  }

  ErrorResult result;
  RefPtr<dom::Promise> promise = dom::Promise::Create(globalObject, result);
  if (result.Failed()) {
    return result.StealNSResult();
  }

  // PurgeBounceTrackers returns a MozPromise, wrap it in a dom::Promise
  // required for XPCOM.
  PurgeBounceTrackers()->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [promise](const PurgeBounceTrackersMozPromise::ResolveValueType&
                    purgedSiteHosts) {
        promise->MaybeResolve(purgedSiteHosts);
      },
      [promise] { promise->MaybeRejectWithUndefined(); });

  promise.forget(aPromise);
  return NS_OK;
}

NS_IMETHODIMP
BounceTrackingProtection::TestAddBounceTrackerCandidate(
    const nsACString& aHost, const PRTime aBounceTime) {
  // Can not have a host in both maps.
  mUserActivation.Remove(aHost);
  mBounceTrackers.InsertOrUpdate(aHost, aBounceTime);
  return NS_OK;
}

NS_IMETHODIMP
BounceTrackingProtection::TestAddUserActivation(const nsACString& aHost,
                                                const PRTime aActivationTime) {
  // Can not have a host in both maps.
  mBounceTrackers.Remove(aHost);
  mUserActivation.InsertOrUpdate(aHost, aActivationTime);
  return NS_OK;
}

RefPtr<BounceTrackingProtection::PurgeBounceTrackersMozPromise>
BounceTrackingProtection::PurgeBounceTrackers() {
  MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
          ("%s: #mUserActivation: %d, #mBounceTrackers: %d", __FUNCTION__,
           mUserActivation.Count(), mBounceTrackers.Count()));

  // Purge already in progress.
  if (!mClearPromises.IsEmpty()) {
    MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
            ("%s: Skip: Purge already in progress.", __FUNCTION__));
    return PurgeBounceTrackersMozPromise::CreateAndReject(
        NS_ERROR_NOT_AVAILABLE, __func__);
  }

  const PRTime now = PR_Now();
  // Convert the user activation lifetime into microseconds for calculation with
  // PRTime values. The pref is a 32-bit value. Cast into 64-bit before
  // multiplying so we get the correct result.
  int64_t activationLifetimeUsec =
      static_cast<int64_t>(
          StaticPrefs::
              privacy_bounceTrackingProtection_bounceTrackingActivationLifetimeSec()) *
      PR_USEC_PER_SEC;

  // 1. Remove hosts from the user activation map whose user activation flag has
  // expired.
  for (auto hostIter = mUserActivation.Iter(); !hostIter.Done();
       hostIter.Next()) {
    const nsACString& host = hostIter.Key();

    // Ensure that mBounceTrackers and mUserActivation maps are disjoint. A host
    // can never be in both maps.
    MOZ_ASSERT(!mBounceTrackers.Contains(host));

    // If activationTime + bounce tracking activation lifetime is before now,
    // then remove host from user activation map.
    const PRTime& activationTime = hostIter.Data();
    if ((activationTime + activationLifetimeUsec) < now) {
      MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
              ("%s: Remove expired user activation for %s", __FUNCTION__,
               PromiseFlatCString(host).get()));
      hostIter.Remove();
    }
  }

  // 2. Go over bounce tracker candidate map and purge state.
  nsresult rv = NS_OK;
  nsCOMPtr<nsIClearDataService> clearDataService =
      do_GetService("@mozilla.org/clear-data-service;1", &rv);
  if (NS_FAILED(rv)) {
    return PurgeBounceTrackersMozPromise::CreateAndReject(rv, __func__);
  }

  mClearPromises.Clear();
  nsTArray<nsCString> purgedSiteHosts;

  for (auto hostIter = mBounceTrackers.Iter(); !hostIter.Done();
       hostIter.Next()) {
    const nsACString& host = hostIter.Key();
    const PRTime& bounceTime = hostIter.Data();

    // If bounceTime + bounce tracking grace period is after now, then continue.
    // The host is still within the grace period and must not be purged.
    if (bounceTime +
            StaticPrefs::
                    privacy_bounceTrackingProtection_bounceTrackingGracePeriodSec() *
                PR_USEC_PER_SEC >
        now) {
      MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
              ("%s: Skip host within bounce tracking grace period %s",
               __FUNCTION__, PromiseFlatCString(host).get()));

      continue;
    }

    // If there is a top-level traversable whose active document's origin's
    // site's host equals host, then continue.
    // TODO: Bug 1842047: Implement a more accurate check that calls into the
    // browser implementations to determine whether the site is currently open
    // on the top level.
    bool hostIsActive;
    rv = BounceTrackingState::HasBounceTrackingStateForSite(host, hostIsActive);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      hostIsActive = false;
    }
    if (hostIsActive) {
      MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
              ("%s: Skip host which is active %s", __FUNCTION__,
               PromiseFlatCString(host).get()));
      continue;
    }

    // No exception above applies, clear state for the given host.

    RefPtr<ClearDataMozPromise::Private> clearPromise =
        new ClearDataMozPromise::Private(__func__);
    RefPtr<ClearDataCallback> cb = new ClearDataCallback(clearPromise, host);

    MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
            ("%s: Purge state for host: %s", __FUNCTION__,
             PromiseFlatCString(host).get()));

    // TODO: Bug 1842067: DeleteDataFromBaseDomain clears the whole cookie jar,
    // including state partitioned under `host` as the top level. Consider only
    // clearing unpartitioned state.
    rv = clearDataService->DeleteDataFromBaseDomain(host, false,
                                                    TRACKER_PURGE_FLAGS, cb);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      clearPromise->Reject(0, __func__);
    }

    mClearPromises.AppendElement(clearPromise);

    // Remove it from the bounce trackers map, it's about to be purged. If the
    // clear call fails still remove it. We want to avoid an ever growing list
    // of hosts in case of repeated failures.
    hostIter.Remove();
  }

  // Wait for all data clearing operations to complete. mClearPromises contains
  // one promise per host / clear task.
  return ClearDataMozPromise::AllSettled(GetCurrentSerialEventTarget(),
                                         mClearPromises)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [&](ClearDataMozPromise::AllSettledPromiseType::ResolveOrRejectValue&&
                  aResults) {
            MOZ_ASSERT(aResults.IsResolve(), "AllSettled never rejects");

            MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Info,
                    ("%s: Done. Cleared %zu hosts.", __FUNCTION__,
                     aResults.ResolveValue().Length()));

            nsTArray<nsCString> purgedSiteHosts;
            // If any clear call failed reject.
            for (auto& result : aResults.ResolveValue()) {
              if (result.IsReject()) {
                mClearPromises.Clear();
                return PurgeBounceTrackersMozPromise::CreateAndReject(
                    NS_ERROR_FAILURE, __func__);
              }
              purgedSiteHosts.AppendElement(result.ResolveValue());
            }

            // No clearing errors, resolve.
            mClearPromises.Clear();
            return PurgeBounceTrackersMozPromise::CreateAndResolve(
                std::move(purgedSiteHosts), __func__);
          });
}

// ClearDataCallback

NS_IMPL_ISUPPORTS(BounceTrackingProtection::ClearDataCallback,
                  nsIClearDataCallback);

// nsIClearDataCallback implementation
NS_IMETHODIMP BounceTrackingProtection::ClearDataCallback::OnDataDeleted(
    uint32_t aFailedFlags) {
  if (aFailedFlags) {
    mPromise->Reject(aFailedFlags, __func__);
  } else {
    MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Info,
            ("%s: Cleared %s", __FUNCTION__, mHost.get()));
    mPromise->Resolve(std::move(mHost), __func__);
  }
  return NS_OK;
}

}  // namespace mozilla
