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
#include "nsRFPService.h"
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
    const nsTArray<nsCString>& aTrackingFullHashes,
    const Maybe<ContentBlockingNotifier::CanvasFingerprinter>&
        aCanvasFingerprinter,
    const Maybe<bool> aCanvasFingerprinterKnownText) {
  MOZ_ASSERT(XRE_IsParentProcess());

  uint32_t events = GetContentBlockingEventsInLog();

  bool blockedValue = aBlocked;
  bool unblocked = false;
  OriginEntry* entry;

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
      Unused << RecordLogInternal(aOrigin, aType, blockedValue);
      break;

    case nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER:
    case nsIWebProgressListener::STATE_COOKIES_BLOCKED_SOCIALTRACKER:
      Unused << RecordLogInternal(aOrigin, aType, blockedValue, aReason,
                                  aTrackingFullHashes);
      break;

    case nsIWebProgressListener::STATE_REPLACED_FINGERPRINTING_CONTENT:
    case nsIWebProgressListener::STATE_ALLOWED_FINGERPRINTING_CONTENT:
    case nsIWebProgressListener::STATE_REPLACED_TRACKING_CONTENT:
    case nsIWebProgressListener::STATE_ALLOWED_TRACKING_CONTENT:
      Unused << RecordLogInternal(aOrigin, aType, blockedValue, aReason,
                                  aTrackingFullHashes);
      break;
    case nsIWebProgressListener::STATE_ALLOWED_FONT_FINGERPRINTING:
      MOZ_ASSERT(!aBlocked,
                 "We don't expected to see blocked "
                 "STATE_ALLOWED_FONT_FINGERPRINTING");
      entry = RecordLogInternal(aOrigin, aType, blockedValue);

      // Replace the flag using the suspicious fingerprinting event so that we
      // can report the event if we detect suspicious fingerprinting.
      aType = nsIWebProgressListener::STATE_BLOCKED_SUSPICIOUS_FINGERPRINTING;

      // Report blocking if we detect suspicious fingerprinting activity.
      if (entry && entry->mData->mHasSuspiciousFingerprintingActivity) {
        blockedValue = true;
      }
      break;

    case nsIWebProgressListener::STATE_ALLOWED_CANVAS_FINGERPRINTING:
      MOZ_ASSERT(!aBlocked,
                 "We don't expected to see blocked "
                 "STATE_ALLOWED_CANVAS_FINGERPRINTING");
      entry = RecordLogInternal(aOrigin, aType, blockedValue, Nothing(), {},
                                aCanvasFingerprinter,
                                aCanvasFingerprinterKnownText);

      // Replace the flag using the suspicious fingerprinting event so that we
      // can report the event if we detect suspicious fingerprinting.
      aType = nsIWebProgressListener::STATE_BLOCKED_SUSPICIOUS_FINGERPRINTING;

      // Report blocking if we detect suspicious fingerprinting activity.
      if (entry && entry->mData->mHasSuspiciousFingerprintingActivity) {
        blockedValue = true;
      }
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

void ContentBlockingLog::ReportCanvasFingerprintingLog(
    nsIPrincipal* aFirstPartyPrincipal) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aFirstPartyPrincipal);

  // We don't need to report if the first party is not a content.
  if (!BasePrincipal::Cast(aFirstPartyPrincipal)->IsContentPrincipal()) {
    return;
  }

  bool hasCanvasFingerprinter = false;
  bool canvasFingerprinterKnownText = false;
  Maybe<ContentBlockingNotifier::CanvasFingerprinter> canvasFingerprinter;
  for (const auto& originEntry : mLog) {
    if (!originEntry.mData) {
      continue;
    }

    for (const auto& logEntry : Reversed(originEntry.mData->mLogs)) {
      if (logEntry.mType !=
          nsIWebProgressListener::STATE_ALLOWED_CANVAS_FINGERPRINTING) {
        continue;
      }

      // Select the log entry with the highest fingerprinting likelihood,
      // that primarily means preferring those with a FingerprinterKnownText.
      if (!hasCanvasFingerprinter ||
          (!canvasFingerprinterKnownText &&
           *logEntry.mCanvasFingerprinterKnownText) ||
          (!canvasFingerprinterKnownText && canvasFingerprinter.isNothing() &&
           logEntry.mCanvasFingerprinter.isSome())) {
        hasCanvasFingerprinter = true;
        canvasFingerprinterKnownText = *logEntry.mCanvasFingerprinterKnownText;
        canvasFingerprinter = logEntry.mCanvasFingerprinter;
      }
    }
  }

  if (!hasCanvasFingerprinter) {
    Telemetry::Accumulate(Telemetry::CANVAS_FINGERPRINTING_PER_TAB,
                          "unknown"_ns, 0);
  } else {
    int32_t fingerprinter =
        canvasFingerprinter.isSome() ? (*canvasFingerprinter + 1) : 0;
    Telemetry::Accumulate(
        Telemetry::CANVAS_FINGERPRINTING_PER_TAB,
        canvasFingerprinterKnownText ? "known_text"_ns : "unknown"_ns,
        fingerprinter);
  }
}

void ContentBlockingLog::ReportFontFingerprintingLog(
    nsIPrincipal* aFirstPartyPrincipal) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aFirstPartyPrincipal);

  // We don't need to report if the first party is not a content.
  if (!BasePrincipal::Cast(aFirstPartyPrincipal)->IsContentPrincipal()) {
    return;
  }

  bool hasFontFingerprinter = false;
  for (const auto& originEntry : mLog) {
    if (!originEntry.mData) {
      continue;
    }

    for (const auto& logEntry : originEntry.mData->mLogs) {
      if (logEntry.mType !=
          nsIWebProgressListener::STATE_ALLOWED_FONT_FINGERPRINTING) {
        continue;
      }

      hasFontFingerprinter = true;
    }

    if (hasFontFingerprinter) {
      break;
    }
  }

  Telemetry::Accumulate(Telemetry::FONT_FINGERPRINTING_PER_TAB,
                        hasFontFingerprinter);
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

ContentBlockingLog::OriginEntry* ContentBlockingLog::RecordLogInternal(
    const nsACString& aOrigin, uint32_t aType, bool aBlocked,
    const Maybe<ContentBlockingNotifier::StorageAccessPermissionGrantedReason>&
        aReason,
    const nsTArray<nsCString>& aTrackingFullHashes,
    const Maybe<ContentBlockingNotifier::CanvasFingerprinter>&
        aCanvasFingerprinter,
    const Maybe<bool> aCanvasFingerprinterKnownText) {
  DebugOnly<bool> isCookiesBlockedTracker =
      aType == nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER ||
      aType == nsIWebProgressListener::STATE_COOKIES_BLOCKED_SOCIALTRACKER;
  MOZ_ASSERT_IF(aBlocked, aReason.isNothing());
  MOZ_ASSERT_IF(!isCookiesBlockedTracker, aReason.isNothing());
  MOZ_ASSERT_IF(isCookiesBlockedTracker && !aBlocked, aReason.isSome());

  if (aOrigin.IsVoid()) {
    return nullptr;
  }
  auto index = mLog.IndexOf(aOrigin, 0, Comparator());
  if (index != OriginDataTable::NoIndex) {
    OriginEntry& entry = mLog[index];
    if (!entry.mData) {
      return nullptr;
    }

    if (RecordLogEntryInCustomField(aType, entry, aBlocked)) {
      return &entry;
    }
    if (!entry.mData->mLogs.IsEmpty()) {
      auto& last = entry.mData->mLogs.LastElement();
      if (last.mType == aType && last.mBlocked == aBlocked &&
          last.mCanvasFingerprinter == aCanvasFingerprinter &&
          last.mCanvasFingerprinterKnownText == aCanvasFingerprinterKnownText) {
        ++last.mRepeatCount;
        // Don't record recorded events.  This helps compress our log.
        // We don't care about if the the reason is the same, just keep the
        // first one.
        // Note: {aReason, aTrackingFullHashes} are not compared here and we
        // simply keep the first for the reason, and merge hashes to make sure
        // they can be correctly recorded.
        for (const auto& hash : aTrackingFullHashes) {
          if (!last.mTrackingFullHashes.Contains(hash)) {
            last.mTrackingFullHashes.AppendElement(hash);
          }
        }
        return &entry;
      }
    }
    if (entry.mData->mLogs.Length() ==
        std::max(1u, StaticPrefs::browser_contentblocking_originlog_length())) {
      // Cap the size at the maximum length adjustable by the pref
      entry.mData->mLogs.RemoveElementAt(0);
    }
    entry.mData->mLogs.AppendElement(
        LogEntry{aType, 1u, aBlocked, aReason, aTrackingFullHashes.Clone(),
                 aCanvasFingerprinter, aCanvasFingerprinterKnownText});

    // Check suspicious fingerprinting activities if the origin hasn't already
    // been marked.
    // TODO(Bug 1864909): Moving the suspicious fingerprinting detection call
    // out of here.
    if ((aType == nsIWebProgressListener::STATE_ALLOWED_CANVAS_FINGERPRINTING ||
         aType == nsIWebProgressListener::STATE_ALLOWED_FONT_FINGERPRINTING) &&
        !entry.mData->mHasSuspiciousFingerprintingActivity &&
        nsRFPService::CheckSuspiciousFingerprintingActivity(
            entry.mData->mLogs)) {
      entry.mData->mHasSuspiciousFingerprintingActivity = true;
    }
    return &entry;
  }

  // The entry has not been found.
  OriginEntry* entry = mLog.AppendElement();
  if (NS_WARN_IF(!entry || !entry->mData)) {
    return nullptr;
  }

  entry->mOrigin = aOrigin;

  if (aType == nsIWebProgressListener::STATE_LOADED_LEVEL_1_TRACKING_CONTENT) {
    entry->mData->mHasLevel1TrackingContentLoaded = aBlocked;
  } else if (aType ==
             nsIWebProgressListener::STATE_LOADED_LEVEL_2_TRACKING_CONTENT) {
    entry->mData->mHasLevel2TrackingContentLoaded = aBlocked;
  } else if (aType == nsIWebProgressListener::STATE_COOKIES_LOADED) {
    MOZ_ASSERT(entry->mData->mHasCookiesLoaded.isNothing());
    entry->mData->mHasCookiesLoaded.emplace(aBlocked);
  } else if (aType == nsIWebProgressListener::STATE_COOKIES_LOADED_TRACKER) {
    MOZ_ASSERT(entry->mData->mHasTrackerCookiesLoaded.isNothing());
    entry->mData->mHasTrackerCookiesLoaded.emplace(aBlocked);
  } else if (aType ==
             nsIWebProgressListener::STATE_COOKIES_LOADED_SOCIALTRACKER) {
    MOZ_ASSERT(entry->mData->mHasSocialTrackerCookiesLoaded.isNothing());
    entry->mData->mHasSocialTrackerCookiesLoaded.emplace(aBlocked);
  } else {
    entry->mData->mLogs.AppendElement(
        LogEntry{aType, 1u, aBlocked, aReason, aTrackingFullHashes.Clone(),
                 aCanvasFingerprinter, aCanvasFingerprinterKnownText});

    // Check suspicious fingerprinting activities if the origin hasn't been
    // marked.
    // TODO(Bug 1864909): Moving the suspicious fingerprinting detection call
    // out of here.
    if ((aType == nsIWebProgressListener::STATE_ALLOWED_CANVAS_FINGERPRINTING ||
         aType == nsIWebProgressListener::STATE_ALLOWED_FONT_FINGERPRINTING) &&
        nsRFPService::CheckSuspiciousFingerprintingActivity(
            entry->mData->mLogs)) {
      entry->mData->mHasSuspiciousFingerprintingActivity = true;
    }
  }

  return entry;
}

}  // namespace mozilla
