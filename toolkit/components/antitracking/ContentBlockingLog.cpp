/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AntiTrackingLog.h"
#include "ContentBlockingLog.h"

#include "nsITrackingDBService.h"
#include "nsStringStream.h"
#include "nsTArray.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/RandomNum.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozilla/StaticPrefs_telemetry.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Unused.h"
#include "mozilla/XorShift128PlusRNG.h"
#include "mozilla/ipc/IPCStreamUtils.h"

namespace mozilla {

using ipc::AutoIPCStream;

typedef Telemetry::OriginMetricID OriginMetricID;

// sync with TelemetryOriginData.inc
const nsLiteralCString ContentBlockingLog::kDummyOriginHash = "PAGELOAD"_ns;

// randomly choose 1% users included in the content blocking measurement
// based on their client id.
static constexpr double kRatioReportUser = 0.01;

// randomly choose 0.14% documents when the page is unload.
static constexpr double kRatioReportDocument = 0.0014;

static bool IsReportingPerUserEnabled() {
  MOZ_ASSERT(NS_IsMainThread());

  static Maybe<bool> sIsReportingEnabled;

  if (sIsReportingEnabled.isSome()) {
    return sIsReportingEnabled.value();
  }

  nsAutoCString cachedClientId;
  if (NS_FAILED(Preferences::GetCString("toolkit.telemetry.cachedClientID",
                                        cachedClientId))) {
    return false;
  }

  nsID clientId;
  if (!clientId.Parse(cachedClientId.get())) {
    return false;
  }

  /**
   * UUID might not be uniform-distributed (although some part of it could be).
   * In order to generate more random result, usually we use a hash function,
   * but here we hope it's fast and doesn't have to be cryptographic-safe.
   * |XorShift128PlusRNG| looks like a good alternative because it takes a
   * 128-bit data as its seed and always generate identical sequence if the
   * initial seed is the same.
   */
  static_assert(sizeof(nsID) == 16, "nsID is 128-bit");
  uint64_t* init = reinterpret_cast<uint64_t*>(&clientId);
  non_crypto::XorShift128PlusRNG rng(init[0], init[1]);
  sIsReportingEnabled.emplace(rng.nextDouble() <= kRatioReportUser);

  return sIsReportingEnabled.value();
}

static bool IsReportingPerDocumentEnabled() {
  constexpr double boundary =
      kRatioReportDocument * double(std::numeric_limits<uint64_t>::max());
  Maybe<uint64_t> randomNum = RandomUint64();
  return randomNum.isSome() && randomNum.value() <= boundary;
}

static bool IsReportingEnabled() {
  if (StaticPrefs::telemetry_origin_telemetry_test_mode_enabled()) {
    return true;
  } else if (!StaticPrefs::
                 privacy_trackingprotection_origin_telemetry_enabled()) {
    return false;
  }

  return IsReportingPerUserEnabled() && IsReportingPerDocumentEnabled();
}

static void ReportOriginSingleHash(OriginMetricID aId,
                                   const nsACString& aOrigin) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());

  LOG(("ReportOriginSingleHash metric=%s",
       Telemetry::MetricIDToString[static_cast<uint32_t>(aId)]));
  LOG(("ReportOriginSingleHash origin=%s", PromiseFlatCString(aOrigin).get()));

  Telemetry::RecordOrigin(aId, aOrigin);
}

Maybe<uint32_t> ContentBlockingLog::RecordLogParent(
    const nsACString& aOrigin, uint32_t aType, bool aBlocked,
    const Maybe<ContentBlockingNotifier::StorageAccessPermissionGrantedReason>&
        aReason,
    const nsTArray<nsCString>& aTrackingFullHashes) {
  MOZ_ASSERT(XRE_IsParentProcess());

  uint32_t events = GetContentBlockingEventsInLog();

  bool blockedValue = aBlocked;
  bool unblocked = false;

  switch (aType) {
    case nsIWebProgressListener::STATE_COOKIES_LOADED:
      MOZ_ASSERT(!aBlocked,
                 "We don't expected to see blocked STATE_COOKIES_LOADED");
      [[fallthrough]];

    case nsIWebProgressListener::STATE_COOKIES_LOADED_TRACKER:
      MOZ_ASSERT(
          !aBlocked,
          "We don't expected to see blocked STATE_COOKIES_LOADED_TRACKER");
      [[fallthrough]];

    case nsIWebProgressListener::STATE_COOKIES_LOADED_SOCIALTRACKER:
      MOZ_ASSERT(!aBlocked,
                 "We don't expected to see blocked "
                 "STATE_COOKIES_LOADED_SOCIALTRACKER");
      // Note that the logic in these branches are the logical negation of the
      // logic in other branches, since the Document API we have is phrased
      // in "loaded" terms as opposed to "blocked" terms.
      blockedValue = !aBlocked;
      [[fallthrough]];

    case nsIWebProgressListener::STATE_BLOCKED_TRACKING_CONTENT:
    case nsIWebProgressListener::STATE_LOADED_LEVEL_1_TRACKING_CONTENT:
    case nsIWebProgressListener::STATE_LOADED_LEVEL_2_TRACKING_CONTENT:
    case nsIWebProgressListener::STATE_BLOCKED_FINGERPRINTING_CONTENT:
    case nsIWebProgressListener::STATE_LOADED_FINGERPRINTING_CONTENT:
    case nsIWebProgressListener::STATE_BLOCKED_CRYPTOMINING_CONTENT:
    case nsIWebProgressListener::STATE_LOADED_CRYPTOMINING_CONTENT:
    case nsIWebProgressListener::STATE_BLOCKED_SOCIALTRACKING_CONTENT:
    case nsIWebProgressListener::STATE_LOADED_SOCIALTRACKING_CONTENT:
    case nsIWebProgressListener::STATE_COOKIES_BLOCKED_BY_PERMISSION:
    case nsIWebProgressListener::STATE_COOKIES_BLOCKED_ALL:
    case nsIWebProgressListener::STATE_COOKIES_BLOCKED_FOREIGN:
      RecordLogInternal(aOrigin, aType, blockedValue);
      break;

    case nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER:
    case nsIWebProgressListener::STATE_COOKIES_BLOCKED_SOCIALTRACKER:
      RecordLogInternal(aOrigin, aType, blockedValue, aReason,
                        aTrackingFullHashes);
      break;

    default:
      // Ignore nsIWebProgressListener::STATE_BLOCKED_UNSAFE_CONTENT;
      break;
  }

  if (!aBlocked) {
    unblocked = (events & aType) != 0;
  }

  const uint32_t oldEvents = events;
  if (blockedValue) {
    events |= aType;
  } else if (unblocked) {
    events &= ~aType;
  }

  if (events == oldEvents
#ifdef ANDROID
      // GeckoView always needs to notify about blocked trackers,
      // since the GeckoView API always needs to report the URI and
      // type of any blocked tracker. We use a platform-dependent code
      // path here because reporting this notification on desktop
      // platforms isn't necessary and doing so can have a big
      // performance cost.
      && aType != nsIWebProgressListener::STATE_BLOCKED_TRACKING_CONTENT
#endif
  ) {
    // Avoid dispatching repeated notifications when nothing has
    // changed
    return Nothing();
  }

  return Some(events);
}

void ContentBlockingLog::ReportLog(nsIPrincipal* aFirstPartyPrincipal) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aFirstPartyPrincipal);

  if (!StaticPrefs::browser_contentblocking_database_enabled()) {
    return;
  }

  if (mLog.IsEmpty()) {
    return;
  }

  nsCOMPtr<nsITrackingDBService> trackingDBService =
      do_GetService("@mozilla.org/tracking-db-service;1");
  if (NS_WARN_IF(!trackingDBService)) {
    return;
  }

  trackingDBService->RecordContentBlockingLog(Stringify());
}

void ContentBlockingLog::ReportOrigins() {
  if (!IsReportingEnabled()) {
    return;
  }
  LOG(("ContentBlockingLog::ReportOrigins [this=%p]", this));
  const bool testMode =
      StaticPrefs::telemetry_origin_telemetry_test_mode_enabled();
  OriginMetricID metricId =
      testMode ? OriginMetricID::ContentBlocking_Blocked_TestOnly
               : OriginMetricID::ContentBlocking_Blocked;
  ReportOriginSingleHash(metricId, kDummyOriginHash);

  nsTArray<HashNumber> lookupTable;
  for (const auto& originEntry : mLog) {
    if (!originEntry.mData) {
      continue;
    }

    for (const auto& logEntry : Reversed(originEntry.mData->mLogs)) {
      if ((logEntry.mType !=
               nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER &&
           logEntry.mType !=
               nsIWebProgressListener::STATE_COOKIES_BLOCKED_SOCIALTRACKER) ||
          logEntry.mTrackingFullHashes.IsEmpty()) {
        continue;
      }

      const bool isBlocked = logEntry.mBlocked;
      Maybe<StorageAccessPermissionGrantedReason> reason = logEntry.mReason;

      metricId = testMode ? OriginMetricID::ContentBlocking_Blocked_TestOnly
                          : OriginMetricID::ContentBlocking_Blocked;
      if (!isBlocked) {
        MOZ_ASSERT(reason.isSome());
        switch (reason.value()) {
          case StorageAccessPermissionGrantedReason::eStorageAccessAPI:
            metricId =
                testMode
                    ? OriginMetricID::
                          ContentBlocking_StorageAccessAPIExempt_TestOnly
                    : OriginMetricID::ContentBlocking_StorageAccessAPIExempt;
            break;
          case StorageAccessPermissionGrantedReason::
              eOpenerAfterUserInteraction:
            metricId =
                testMode
                    ? OriginMetricID::
                          ContentBlocking_OpenerAfterUserInteractionExempt_TestOnly
                    : OriginMetricID::
                          ContentBlocking_OpenerAfterUserInteractionExempt;
            break;
          case StorageAccessPermissionGrantedReason::eOpener:
            metricId =
                testMode ? OriginMetricID::ContentBlocking_OpenerExempt_TestOnly
                         : OriginMetricID::ContentBlocking_OpenerExempt;
            break;
          default:
            MOZ_ASSERT_UNREACHABLE(
                "Unknown StorageAccessPermissionGrantedReason");
        }
      }

      for (const auto& hash : logEntry.mTrackingFullHashes) {
        HashNumber key = AddToHash(HashString(hash.get(), hash.Length()),
                                   static_cast<uint32_t>(metricId));
        if (lookupTable.Contains(key)) {
          continue;
        }
        lookupTable.AppendElement(key);
        ReportOriginSingleHash(metricId, hash);
      }
      break;
    }
  }
}

}  // namespace mozilla
