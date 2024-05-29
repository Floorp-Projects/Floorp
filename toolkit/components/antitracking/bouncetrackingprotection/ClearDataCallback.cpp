/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClearDataCallback.h"
#include "mozilla/glean/GleanMetrics.h"
#include "nsIURIClassifier.h"
#include "mozilla/net/UrlClassifierFeatureFactory.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/ClearOnShutdown.h"

using namespace mozilla;

// Used in automation. Dispatched when a site host has been purged, classified
// and telemetry has been collected for the given host.
#define TEST_OBSERVER_MSG_RECORDED_PURGE_TELEMETRY \
  "bounce-tracking-protection-recorded-purge-telemetry"

// List of features classifying bounce trackers that have been purged.
static constexpr nsLiteralCString kUrlClassifierFeatures[] = {
    "emailtracking-protection"_ns,
    "fingerprinting-protection"_ns,
    "socialtracking-protection"_ns,
    "tracking-protection"_ns,
};
static_assert(ArrayLength(kUrlClassifierFeatures) > 0,
              "At least one URL classifier feature must be defined");

NS_IMPL_ISUPPORTS(ClearDataCallback, nsIClearDataCallback,
                  nsIUrlClassifierFeatureCallback);

// static
nsTArray<RefPtr<nsIUrlClassifierFeature>>
    ClearDataCallback::sUrlClassifierFeatures;

ClearDataCallback::ClearDataCallback(ClearDataMozPromise::Private* aPromise,
                                     const nsACString& aHost)
    : mHost(aHost), mPromise(aPromise), mClearDurationTimer(0) {
  MOZ_ASSERT(!aHost.IsEmpty(), "Host must not be empty");

  if (!StaticPrefs::privacy_bounceTrackingProtection_enableDryRunMode()) {
    // Only collect timing information when actually performing the deletion
    mClearDurationTimer =
        glean::bounce_tracking_protection::purge_duration.Start();
    MOZ_ASSERT(mClearDurationTimer);
  }

  // Populate feature list for URL classification as needed.
  if (sUrlClassifierFeatures.IsEmpty()) {
    // Construct the list of classifier features used for purging telemetry.
    for (const nsCString& featureName : kUrlClassifierFeatures) {
      nsCOMPtr<nsIUrlClassifierFeature> feature =
          net::UrlClassifierFeatureFactory::GetFeatureByName(featureName);
      if (NS_WARN_IF(!feature)) {
        continue;
      }
      sUrlClassifierFeatures.AppendElement(feature);
    }
    MOZ_ASSERT(!sUrlClassifierFeatures.IsEmpty(),
               "At least one URL classifier feature must be present");
    RunOnShutdown([] { sUrlClassifierFeatures.Clear(); });
  }
};

ClearDataCallback::~ClearDataCallback() {
  mPromise->Reject(0, __func__);
  if (mClearDurationTimer) {
    glean::bounce_tracking_protection::purge_duration.Cancel(
        std::move(mClearDurationTimer));
  }
}

// nsIClearDataCallback implementation
NS_IMETHODIMP ClearDataCallback::OnDataDeleted(uint32_t aFailedFlags) {
  if (aFailedFlags) {
    mPromise->Reject(aFailedFlags, __func__);
  } else {
    MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
            ("%s: Cleared %s", __FUNCTION__, mHost.get()));
    mPromise->Resolve(mHost, __func__);

    // Only record classifications on successful deletion.
    RecordURLClassifierTelemetry();
  }
  // Always collect clear duration and purge count.
  RecordClearDurationTelemetry();
  RecordPurgeCountTelemetry(aFailedFlags != 0);
  return NS_OK;
}

void ClearDataCallback::RecordClearDurationTelemetry() {
  if (mClearDurationTimer) {
    glean::bounce_tracking_protection::purge_duration.StopAndAccumulate(
        std::move(mClearDurationTimer));
    mClearDurationTimer = 0;
  }
}

void ClearDataCallback::RecordPurgeCountTelemetry(bool aFailed) {
  if (StaticPrefs::privacy_bounceTrackingProtection_enableDryRunMode()) {
    MOZ_ASSERT(aFailed == 0, "Dry-run purge can't fail");
    glean::bounce_tracking_protection::purge_count.Get("dry"_ns).Add(1);
  } else if (aFailed) {
    glean::bounce_tracking_protection::purge_count.Get("failure"_ns).Add(1);
  } else {
    glean::bounce_tracking_protection::purge_count.Get("success"_ns).Add(1);
  }
}

void ClearDataCallback::RecordURLClassifierTelemetry() {
  nsresult rv = NS_OK;
  nsCOMPtr<nsIURIClassifier> uriClassifier =
      do_GetService(NS_URICLASSIFIERSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS_VOID(rv);
  NS_ENSURE_TRUE_VOID(uriClassifier);

  // Create a copy of the site host because we might have to mutate it.
  nsAutoCString siteHost(mHost);
  nsContentUtils::MaybeFixIPv6Host(siteHost);

  // Create URI from siteHost
  nsAutoCString uriStr("https://");
  uriStr.Append(siteHost);

  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), uriStr);
  NS_ENSURE_SUCCESS_VOID(rv);

  rv = uriClassifier->AsyncClassifyLocalWithFeatures(
      uri, sUrlClassifierFeatures, nsIUrlClassifierFeature::blocklist, this);
  NS_ENSURE_SUCCESS_VOID(rv);
}

// nsIUrlClassifierFeatureCallback
// Used for telemetry only.
NS_IMETHODIMP
ClearDataCallback::OnClassifyComplete(
    const nsTArray<RefPtr<nsIUrlClassifierFeatureResult>>& aResults) {
  if (!aResults.IsEmpty()) {
    // Classified as a tracker => Increase Glean counter. We don't have to count
    // non-classified hosts because we already keep track of the total count of
    // successful purges.
    glean::bounce_tracking_protection::purge_count_classified_tracker.Add(1);
  }

  // In test mode dispatch an observer message to indicate we've completed
  // collecting telemetry for the purge for the given host. This is needed
  // because classification happens async.
  if (StaticPrefs::privacy_bounceTrackingProtection_enableTestMode()) {
    nsCOMPtr<nsIObserverService> obsSvc =
        mozilla::services::GetObserverService();
    NS_ENSURE_TRUE(obsSvc, NS_ERROR_FAILURE);

    nsresult rv = obsSvc->NotifyObservers(
        nullptr, TEST_OBSERVER_MSG_RECORDED_PURGE_TELEMETRY,
        NS_ConvertUTF8toUTF16(mHost).get());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}
