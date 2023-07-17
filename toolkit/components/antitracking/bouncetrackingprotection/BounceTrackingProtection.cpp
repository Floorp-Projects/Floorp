/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BounceTrackingProtection.h"

#include "BounceTrackingState.h"
#include "BounceTrackingRecord.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Logging.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "nsHashPropertyBag.h"
#include "nsIObserverService.h"
#include "nsIPrincipal.h"
#include "prtime.h"
#include "mozilla/dom/BrowsingContext.h"

#define TEST_OBSERVER_MSG_RECORD_BOUNCES_FINISHED "test-record-bounces-finished"

namespace mozilla {

NS_IMPL_ISUPPORTS(BounceTrackingProtection, nsIBounceTrackingProtection);

LazyLogModule gBounceTrackingProtectionLog("BounceTrackingProtection");

static StaticRefPtr<BounceTrackingProtection> sBounceTrackingProtection;

// static
already_AddRefed<BounceTrackingProtection>
BounceTrackingProtection::GetSingleton() {
  MOZ_ASSERT(XRE_IsParentProcess());

  if (!StaticPrefs::privacy_bounceTrackingProtection_enabled_AtStartup()) {
    return nullptr;
  }

  if (!sBounceTrackingProtection) {
    MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug, ("constructor"));
    sBounceTrackingProtection = new BounceTrackingProtection();

    RunOnShutdown([] { sBounceTrackingProtection = nullptr; });
  }

  return do_AddRef(sBounceTrackingProtection);
}

nsresult BounceTrackingProtection::RecordStatefulBounces(
    BounceTrackingState* aBounceTrackingState) {
  NS_ENSURE_ARG_POINTER(aBounceTrackingState);

  MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Info,
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

    MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
            ("%s: Added to mBounceTrackers: %s, Time: %" PRIu64, __FUNCTION__,
             PromiseFlatCString(host).get(), static_cast<uint64_t>(now)));
  }

  // Set navigable’s bounce tracking record to null.
  aBounceTrackingState->ResetBounceTrackingRecord();
  MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Info,
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
  mBounceTrackers.Clear();
  mUserActivation.Clear();

  return NS_OK;
}

}  // namespace mozilla
