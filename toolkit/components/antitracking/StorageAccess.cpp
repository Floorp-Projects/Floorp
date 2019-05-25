/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/StorageAccess.h"
#include "nsICookieService.h"
#include "nsICookieSettings.h"
#include "nsIWebProgressListener.h"

namespace mozilla {

bool ShouldPartitionStorage(nsContentUtils::StorageAccess aAccess) {
  return aAccess == nsContentUtils::StorageAccess::ePartitionTrackersOrDeny ||
         aAccess == nsContentUtils::StorageAccess::ePartitionForeignOrDeny;
}

bool ShouldPartitionStorage(uint32_t aRejectedReason) {
  return aRejectedReason ==
             nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER ||
         aRejectedReason ==
             nsIWebProgressListener::STATE_COOKIES_PARTITIONED_FOREIGN;
}

bool StoragePartitioningEnabled(nsContentUtils::StorageAccess aAccess,
                                nsICookieSettings* aCookieSettings) {
  if (aAccess == nsContentUtils::StorageAccess::ePartitionTrackersOrDeny) {
    return aCookieSettings->GetCookieBehavior() ==
               nsICookieService::BEHAVIOR_REJECT_TRACKER &&
           StaticPrefs::privacy_storagePrincipal_enabledForTrackers();
  }
  if (aAccess == nsContentUtils::StorageAccess::ePartitionForeignOrDeny) {
    return aCookieSettings->GetCookieBehavior() ==
           nsICookieService::BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN;
  }
  return false;
}

bool StoragePartitioningEnabled(uint32_t aRejectedReason,
                                nsICookieSettings* aCookieSettings) {
  if (aRejectedReason ==
      nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER) {
    return aCookieSettings->GetCookieBehavior() ==
               nsICookieService::BEHAVIOR_REJECT_TRACKER &&
           StaticPrefs::privacy_storagePrincipal_enabledForTrackers();
  }
  if (aRejectedReason ==
      nsIWebProgressListener::STATE_COOKIES_PARTITIONED_FOREIGN) {
    return aCookieSettings->GetCookieBehavior() ==
           nsICookieService::BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN;
  }
  return false;
}

}  // namespace mozilla
