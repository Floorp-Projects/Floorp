/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BounceTrackingProtection.h"

#include "BounceTrackingProtectionStorage.h"
#include "BounceTrackingState.h"
#include "BounceTrackingRecord.h"

#include "BounceTrackingStateGlobal.h"
#include "ErrorList.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ContentBlockingAllowList.h"
#include "mozilla/Logging.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozilla/dom/Promise.h"
#include "nsDebug.h"
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

  mStorage = new BounceTrackingProtectionStorage();

  nsresult rv = mStorage->Init();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Error,
            ("storage init failed"));
    return;
  }

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

  rv = NS_NewTimerWithCallback(
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
  const Maybe<BounceTrackingRecord>& record =
      aBounceTrackingState->GetBounceTrackingRecord();
  NS_ENSURE_TRUE(record, NS_ERROR_FAILURE);

  // Get the bounce tracker map and the user activation map.
  RefPtr<BounceTrackingStateGlobal> globalState =
      mStorage->GetOrCreateStateGlobal(aBounceTrackingState);
  MOZ_ASSERT(globalState);

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
    if (globalState->HasUserActivation(host)) {
      MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
              ("%s: Skip host with recent user activation: %s", __FUNCTION__,
               PromiseFlatCString(host).get()));
      continue;
    }

    // If stateful bounce tracking map contains host, continue.
    if (globalState->HasBounceTracker(host)) {
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
    MOZ_ASSERT(!globalState->HasBounceTracker(host));
    nsresult rv = globalState->RecordBounceTracker(host, now);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }

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

  RefPtr<BounceTrackingStateGlobal> globalState =
      mStorage->GetOrCreateStateGlobal(aPrincipal);
  MOZ_ASSERT(globalState);

  return globalState->RecordUserActivation(siteHost, PR_Now());
}

NS_IMETHODIMP
BounceTrackingProtection::TestGetBounceTrackerCandidateHosts(
    JS::Handle<JS::Value> aOriginAttributes, JSContext* aCx,
    nsTArray<nsCString>& aCandidates) {
  MOZ_ASSERT(aCx);

  OriginAttributes oa;
  if (!aOriginAttributes.isObject() || !oa.Init(aCx, aOriginAttributes)) {
    return NS_ERROR_INVALID_ARG;
  }

  BounceTrackingStateGlobal* globalState = mStorage->GetOrCreateStateGlobal(oa);
  MOZ_ASSERT(globalState);

  for (const nsACString& host : globalState->BounceTrackersMapRef().Keys()) {
    aCandidates.AppendElement(host);
  }

  return NS_OK;
}

NS_IMETHODIMP
BounceTrackingProtection::TestGetUserActivationHosts(
    JS::Handle<JS::Value> aOriginAttributes, JSContext* aCx,
    nsTArray<nsCString>& aHosts) {
  MOZ_ASSERT(aCx);

  OriginAttributes oa;
  if (!aOriginAttributes.isObject() || !oa.Init(aCx, aOriginAttributes)) {
    return NS_ERROR_INVALID_ARG;
  }

  BounceTrackingStateGlobal* globalState = mStorage->GetOrCreateStateGlobal(oa);
  MOZ_ASSERT(globalState);

  for (const nsACString& host : globalState->UserActivationMapRef().Keys()) {
    aHosts.AppendElement(host);
  }

  return NS_OK;
}

NS_IMETHODIMP
BounceTrackingProtection::ClearAll() {
  BounceTrackingState::ResetAll();
  return mStorage->Clear();
}

NS_IMETHODIMP
BounceTrackingProtection::ClearBySiteHostAndOA(
    const nsACString& aSiteHost, JS::Handle<JS::Value> aOriginAttributes,
    JSContext* aCx) {
  NS_ENSURE_ARG_POINTER(aCx);

  OriginAttributes originAttributes;
  if (!aOriginAttributes.isObject() ||
      !originAttributes.Init(aCx, aOriginAttributes)) {
    return NS_ERROR_INVALID_ARG;
  }

  // Reset per tab state for tabs matching the given OriginAttributes.
  BounceTrackingState::ResetAllForOriginAttributes(originAttributes);

  return mStorage->ClearBySiteHost(aSiteHost, &originAttributes);
}

NS_IMETHODIMP
BounceTrackingProtection::ClearBySiteHost(const nsACString& aSiteHost) {
  BounceTrackingState::ResetAll();

  return mStorage->ClearBySiteHost(aSiteHost, nullptr);
}

NS_IMETHODIMP
BounceTrackingProtection::ClearByTimeRange(PRTime aFrom, PRTime aTo) {
  NS_ENSURE_TRUE(aFrom >= 0, NS_ERROR_INVALID_ARG);
  NS_ENSURE_TRUE(aFrom < aTo, NS_ERROR_INVALID_ARG);

  // Clear all BounceTrackingState, we don't keep track of time ranges.
  BounceTrackingState::ResetAll();

  return mStorage->ClearByTimeRange(aFrom, aTo);
}

NS_IMETHODIMP
BounceTrackingProtection::ClearByOriginAttributesPattern(
    const nsAString& aPattern) {
  OriginAttributesPattern pattern;
  if (!pattern.Init(aPattern)) {
    return NS_ERROR_INVALID_ARG;
  }

  // Reset all per-tab state matching the given OriginAttributesPattern.
  BounceTrackingState::ResetAllForOriginAttributesPattern(pattern);

  return mStorage->ClearByOriginAttributesPattern(pattern);
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
BounceTrackingProtection::TestClearExpiredUserActivations() {
  return ClearExpiredUserInteractions();
}

NS_IMETHODIMP
BounceTrackingProtection::TestAddBounceTrackerCandidate(
    JS::Handle<JS::Value> aOriginAttributes, const nsACString& aHost,
    const PRTime aBounceTime, JSContext* aCx) {
  MOZ_ASSERT(aCx);

  OriginAttributes oa;
  if (!aOriginAttributes.isObject() || !oa.Init(aCx, aOriginAttributes)) {
    return NS_ERROR_INVALID_ARG;
  }

  BounceTrackingStateGlobal* stateGlobal = mStorage->GetOrCreateStateGlobal(oa);
  MOZ_ASSERT(stateGlobal);

  // Ensure aHost is lowercase to match nsIURI and nsIPrincipal.
  nsAutoCString host(aHost);
  ToLowerCase(host);

  // Can not have a host in both maps.
  nsresult rv = stateGlobal->TestRemoveUserActivation(host);
  NS_ENSURE_SUCCESS(rv, rv);
  return stateGlobal->RecordBounceTracker(host, aBounceTime);
}

NS_IMETHODIMP
BounceTrackingProtection::TestAddUserActivation(
    JS::Handle<JS::Value> aOriginAttributes, const nsACString& aHost,
    const PRTime aActivationTime, JSContext* aCx) {
  MOZ_ASSERT(aCx);

  OriginAttributes oa;
  if (!aOriginAttributes.isObject() || !oa.Init(aCx, aOriginAttributes)) {
    return NS_ERROR_INVALID_ARG;
  }

  BounceTrackingStateGlobal* stateGlobal = mStorage->GetOrCreateStateGlobal(oa);
  MOZ_ASSERT(stateGlobal);

  // Ensure aHost is lowercase to match nsIURI and nsIPrincipal.
  nsAutoCString host(aHost);
  ToLowerCase(host);

  return stateGlobal->RecordUserActivation(host, aActivationTime);
}

RefPtr<BounceTrackingProtection::PurgeBounceTrackersMozPromise>
BounceTrackingProtection::PurgeBounceTrackers() {
  // Prevent multiple purge operations from running at the same time.
  if (mPurgeInProgress) {
    MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
            ("%s: Skip: Purge already in progress.", __FUNCTION__));
    return PurgeBounceTrackersMozPromise::CreateAndReject(
        nsresult::NS_ERROR_NOT_AVAILABLE, __func__);
  }
  mPurgeInProgress = true;

  // Obtain a cache of ContentBlockingAllowList permissions so we only need to
  // fetch permissions once even when we do multiple base domain lookups.
  ContentBlockingAllowListCache contentBlockingAllowListCache;

  // Collect promises for all clearing operations to later await on.
  nsTArray<RefPtr<ClearDataMozPromise>> clearPromises;

  // Run the purging algorithm for all global state objects.
  for (const auto& entry : mStorage->StateGlobalMapRef()) {
    const OriginAttributes& originAttributes = entry.GetKey();
    BounceTrackingStateGlobal* stateGlobal = entry.GetData();
    MOZ_ASSERT(stateGlobal);

    if (MOZ_LOG_TEST(gBounceTrackingProtectionLog, LogLevel::Debug)) {
      nsAutoCString oaSuffix;
      originAttributes.CreateSuffix(oaSuffix);
      MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
              ("%s: Running purge algorithm for OA: '%s'", __FUNCTION__,
               oaSuffix.get()));
    }

    nsresult rv = PurgeBounceTrackersForStateGlobal(
        stateGlobal, contentBlockingAllowListCache, clearPromises);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return PurgeBounceTrackersMozPromise::CreateAndReject(rv, __func__);
    }
  }

  // Wait for all data clearing operations to complete. mClearPromises contains
  // one promise per host / clear task.
  return ClearDataMozPromise::AllSettled(GetCurrentSerialEventTarget(),
                                         clearPromises)
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
                mPurgeInProgress = false;
                return PurgeBounceTrackersMozPromise::CreateAndReject(
                    NS_ERROR_FAILURE, __func__);
              }
              purgedSiteHosts.AppendElement(result.ResolveValue());
            }

            // No clearing errors, resolve.

            mPurgeInProgress = false;
            return PurgeBounceTrackersMozPromise::CreateAndResolve(
                std::move(purgedSiteHosts), __func__);
          });
}

nsresult BounceTrackingProtection::PurgeBounceTrackersForStateGlobal(
    BounceTrackingStateGlobal* aStateGlobal,
    ContentBlockingAllowListCache& aContentBlockingAllowList,
    nsTArray<RefPtr<ClearDataMozPromise>>& aClearPromises) {
  MOZ_ASSERT(aStateGlobal);
  MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
          ("%s: %s", __FUNCTION__, aStateGlobal->Describe().get()));

  const PRTime now = PR_Now();

  // 1. Remove hosts from the user activation map whose user activation flag has
  // expired.
  nsresult rv = ClearExpiredUserInteractions(aStateGlobal);
  NS_ENSURE_SUCCESS(rv, rv);

  // 2. Go over bounce tracker candidate map and purge state.
  rv = NS_OK;
  nsCOMPtr<nsIClearDataService> clearDataService =
      do_GetService("@mozilla.org/clear-data-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsTArray<nsCString> purgedSiteHosts;

  // Collect hosts to remove from the bounce trackers map. We can not remove
  // them while iterating over the map.
  nsTArray<nsCString> bounceTrackerCandidatesToRemove;

  for (auto hostIter = aStateGlobal->BounceTrackersMapRef().ConstIter();
       !hostIter.Done(); hostIter.Next()) {
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

    // Gecko specific: If the host is on the content blocking allow-list,
    // continue.
    bool isAllowListed = false;
    rv = aContentBlockingAllowList.CheckForBaseDomain(
        host, aStateGlobal->OriginAttributesRef(), isAllowListed);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }
    if (isAllowListed) {
      if (MOZ_LOG_TEST(gBounceTrackingProtectionLog, LogLevel::Debug)) {
        nsAutoCString originAttributeSuffix;
        aStateGlobal->OriginAttributesRef().CreateSuffix(originAttributeSuffix);
        MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
                ("%s: Skip host on the content blocking allow-list: host: %s, "
                 "originAttributes: %s",
                 __FUNCTION__, PromiseFlatCString(host).get(),
                 originAttributeSuffix.get()));
      }
      continue;
    }

    // No exception above applies, clear state for the given host.

    RefPtr<ClearDataMozPromise::Private> clearPromise =
        new ClearDataMozPromise::Private(__func__);
    RefPtr<ClearDataCallback> cb = new ClearDataCallback(clearPromise, host);

    MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
            ("%s: Purge state for host: %s", __FUNCTION__,
             PromiseFlatCString(host).get()));

    // TODO: Bug 1842067: Clear by site + OA.
    rv = clearDataService->DeleteDataFromBaseDomain(host, false,
                                                    TRACKER_PURGE_FLAGS, cb);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      clearPromise->Reject(0, __func__);
    }

    aClearPromises.AppendElement(clearPromise);

    // Remove it from the bounce trackers map, it's about to be purged. If the
    // clear call fails still remove it. We want to avoid an ever growing list
    // of hosts in case of repeated failures.
    bounceTrackerCandidatesToRemove.AppendElement(host);
  }

  // Remove hosts from the bounce trackers map which we executed purge calls
  // for.
  return aStateGlobal->RemoveBounceTrackers(bounceTrackerCandidatesToRemove);
}

nsresult BounceTrackingProtection::ClearExpiredUserInteractions(
    BounceTrackingStateGlobal* aStateGlobal) {
  if (!aStateGlobal && mStorage->StateGlobalMapRef().IsEmpty()) {
    // Nothing to clear.
    return NS_OK;
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

  // Clear user activation for the given state global.
  if (aStateGlobal) {
    return aStateGlobal->ClearUserActivationBefore(now -
                                                   activationLifetimeUsec);
  }

  // aStateGlobal not passed, clear user activation for all state globals.
  for (const auto& entry : mStorage->StateGlobalMapRef()) {
    const RefPtr<BounceTrackingStateGlobal>& stateGlobal = entry.GetData();
    MOZ_ASSERT(stateGlobal);

    nsresult rv =
        stateGlobal->ClearUserActivationBefore(now - activationLifetimeUsec);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
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
