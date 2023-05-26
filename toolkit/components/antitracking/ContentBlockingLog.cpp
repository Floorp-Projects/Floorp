/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AntiTrackingLog.h"
#include "ContentBlockingLog.h"

#include "nsIEffectiveTLDService.h"
#include "nsITrackingDBService.h"
#include "nsIWebProgressListener.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "nsTArray.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/Preferences.h"
#include "mozilla/RandomNum.h"
#include "mozilla/ReverseIterator.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozilla/StaticPrefs_telemetry.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Telemetry.h"
#include "mozilla/XorShift128PlusRNG.h"

namespace mozilla {

namespace {

StaticAutoPtr<nsCString> gEmailWebAppDomainsPref;
static constexpr char kEmailWebAppDomainPrefName[] =
    "privacy.trackingprotection.emailtracking.webapp.domains";

void EmailWebAppDomainPrefChangeCallback(const char* aPrefName, void*) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!strcmp(aPrefName, kEmailWebAppDomainPrefName));
  MOZ_ASSERT(gEmailWebAppDomainsPref);

  Preferences::GetCString(kEmailWebAppDomainPrefName, *gEmailWebAppDomainsPref);
}

}  // namespace

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
    case nsIWebProgressListener::STATE_BLOCKED_EMAILTRACKING_CONTENT:
    case nsIWebProgressListener::STATE_LOADED_EMAILTRACKING_LEVEL_1_CONTENT:
    case nsIWebProgressListener::STATE_LOADED_EMAILTRACKING_LEVEL_2_CONTENT:
      RecordLogInternal(aOrigin, aType, blockedValue);
      break;

    case nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER:
    case nsIWebProgressListener::STATE_COOKIES_BLOCKED_SOCIALTRACKER:
      RecordLogInternal(aOrigin, aType, blockedValue, aReason,
                        aTrackingFullHashes);
      break;

    case nsIWebProgressListener::STATE_REPLACED_FINGERPRINTING_CONTENT:
    case nsIWebProgressListener::STATE_ALLOWED_FINGERPRINTING_CONTENT:
    case nsIWebProgressListener::STATE_REPLACED_TRACKING_CONTENT:
    case nsIWebProgressListener::STATE_ALLOWED_TRACKING_CONTENT:
      RecordLogInternal(aOrigin, aType, blockedValue);
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

void ContentBlockingLog::ReportEmailTrackingLog(
    nsIPrincipal* aFirstPartyPrincipal) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aFirstPartyPrincipal);

  // We don't need to report if the first party is not a content.
  if (!BasePrincipal::Cast(aFirstPartyPrincipal)->IsContentPrincipal()) {
    return;
  }

  nsCOMPtr<nsIEffectiveTLDService> tldService =
      do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID);

  if (!tldService) {
    return;
  }

  nsTHashtable<nsCStringHashKey> level1SiteSet;
  nsTHashtable<nsCStringHashKey> level2SiteSet;

  for (const auto& originEntry : mLog) {
    if (!originEntry.mData) {
      continue;
    }

    bool isLevel1EmailTracker = false;
    bool isLevel2EmailTracker = false;

    for (const auto& logEntry : Reversed(originEntry.mData->mLogs)) {
      // Check if the email tracking related event had been filed for the given
      // origin entry. Note that we currently only block level 1 email trackers,
      // so blocking event represents the page has embedded a level 1 tracker.
      if (logEntry.mType ==
          nsIWebProgressListener::STATE_LOADED_EMAILTRACKING_LEVEL_2_CONTENT) {
        isLevel2EmailTracker = true;
        break;
      }

      if (logEntry.mType ==
              nsIWebProgressListener::STATE_BLOCKED_EMAILTRACKING_CONTENT ||
          logEntry.mType == nsIWebProgressListener::
                                STATE_LOADED_EMAILTRACKING_LEVEL_1_CONTENT) {
        isLevel1EmailTracker = true;
        break;
      }
    }

    if (isLevel1EmailTracker || isLevel2EmailTracker) {
      nsCOMPtr<nsIURI> uri;
      nsresult rv = NS_NewURI(getter_AddRefs(uri), originEntry.mOrigin);

      if (NS_FAILED(rv)) {
        continue;
      }

      nsAutoCString baseDomain;
      rv = tldService->GetBaseDomain(uri, 0, baseDomain);

      if (NS_FAILED(rv)) {
        continue;
      }

      if (isLevel1EmailTracker) {
        Unused << level1SiteSet.EnsureInserted(baseDomain);
      } else {
        Unused << level2SiteSet.EnsureInserted(baseDomain);
      }
    }
  }

  // Cache the email webapp domains pref value and register the callback
  // function to update the cached value when the pref changes.
  if (!gEmailWebAppDomainsPref) {
    gEmailWebAppDomainsPref = new nsCString();

    Preferences::RegisterCallbackAndCall(EmailWebAppDomainPrefChangeCallback,
                                         kEmailWebAppDomainPrefName);
    RunOnShutdown([]() {
      Preferences::UnregisterCallback(EmailWebAppDomainPrefChangeCallback,
                                      kEmailWebAppDomainPrefName);
      gEmailWebAppDomainsPref = nullptr;
    });
  }

  bool isTopEmailWebApp =
      aFirstPartyPrincipal->IsURIInList(*gEmailWebAppDomainsPref);
  uint32_t level1Count = level1SiteSet.Count();
  uint32_t level2Count = level2SiteSet.Count();

  Telemetry::Accumulate(
      Telemetry::EMAIL_TRACKER_EMBEDDED_PER_TAB,
      isTopEmailWebApp ? "base_emailapp"_ns : "base_normal"_ns, level1Count);
  Telemetry::Accumulate(
      Telemetry::EMAIL_TRACKER_EMBEDDED_PER_TAB,
      isTopEmailWebApp ? "content_emailapp"_ns : "content_normal"_ns,
      level2Count);
  Telemetry::Accumulate(Telemetry::EMAIL_TRACKER_EMBEDDED_PER_TAB,
                        isTopEmailWebApp ? "all_emailapp"_ns : "all_normal"_ns,
                        level1Count + level2Count);
}

}  // namespace mozilla
