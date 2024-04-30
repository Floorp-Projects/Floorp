/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BounceTrackingStateGlobal.h"
#include "BounceTrackingProtectionStorage.h"
#include "ErrorList.h"
#include "mozilla/Assertions.h"
#include "mozilla/Logging.h"
#include "nsIPrincipal.h"

namespace mozilla {

NS_IMPL_CYCLE_COLLECTION(BounceTrackingStateGlobal);

extern LazyLogModule gBounceTrackingProtectionLog;

BounceTrackingStateGlobal::BounceTrackingStateGlobal(
    BounceTrackingProtectionStorage* aStorage, const OriginAttributes& aAttrs)
    : mStorage(aStorage), mOriginAttributes(aAttrs) {
  MOZ_ASSERT(aStorage);
}

bool BounceTrackingStateGlobal::HasUserActivation(
    const nsACString& aSiteHost) const {
  return mUserActivation.Contains(aSiteHost);
}

nsresult BounceTrackingStateGlobal::RecordUserActivation(
    const nsACString& aSiteHost, PRTime aTime, bool aSkipStorage) {
  NS_ENSURE_TRUE(aSiteHost.Length(), NS_ERROR_INVALID_ARG);
  NS_ENSURE_TRUE(aTime > 0, NS_ERROR_INVALID_ARG);

  // A site must only be in one of the maps at a time.
  bool hasRemoved = mBounceTrackers.Remove(aSiteHost);

  if (hasRemoved) {
    MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
            ("%s: Removed bounce tracking candidate due to user activation: %s",
             __FUNCTION__, PromiseFlatCString(aSiteHost).get()));
  }

  // Make sure we don't overwrite an existing, more recent user activation. This
  // is only relevant for callers that pass in a timestamp that isn't PR_Now(),
  // e.g. when importing user activation data.
  Maybe<PRTime> existingUserActivation = mUserActivation.MaybeGet(aSiteHost);
  if (existingUserActivation.isSome() &&
      existingUserActivation.value() >= aTime) {
    MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
            ("%s: Skip: A more recent user activation "
             "already exists for %s",
             __FUNCTION__, PromiseFlatCString(aSiteHost).get()));
    return NS_OK;
  }

  mUserActivation.InsertOrUpdate(aSiteHost, aTime);

  if (aSkipStorage || !ShouldPersistToDisk()) {
    return NS_OK;
  }

  // Write the change to storage.
  NS_ENSURE_TRUE(mStorage, NS_ERROR_FAILURE);
  return mStorage->UpdateDBEntry(
      mOriginAttributes, aSiteHost,
      BounceTrackingProtectionStorage::EntryType::UserActivation, aTime);
}

nsresult BounceTrackingStateGlobal::TestRemoveUserActivation(
    const nsACString& aSiteHost) {
  bool hasRemoved = mUserActivation.Remove(aSiteHost);

  // Avoid potentially removing a bounce tracking entry if there is no user
  // activation entry.
  if (!hasRemoved) {
    return NS_OK;
  }

  if (!ShouldPersistToDisk()) {
    return NS_OK;
  }

  // Write the change to storage.
  NS_ENSURE_TRUE(mStorage, NS_ERROR_FAILURE);
  return mStorage->DeleteDBEntries(&mOriginAttributes, aSiteHost);
}

nsresult BounceTrackingStateGlobal::ClearUserActivationBefore(PRTime aTime) {
  return ClearByTimeRange(
      0, Some(aTime),
      Some(BounceTrackingProtectionStorage::EntryType::UserActivation));
}

nsresult BounceTrackingStateGlobal::ClearSiteHost(const nsACString& aSiteHost,
                                                  bool aSkipStorage) {
  NS_ENSURE_TRUE(aSiteHost.Length(), NS_ERROR_INVALID_ARG);

  bool removedUserActivation = mUserActivation.Remove(aSiteHost);
  bool removedBounceTracker = mBounceTrackers.Remove(aSiteHost);
  if (removedUserActivation || removedBounceTracker) {
    MOZ_ASSERT(removedUserActivation != removedBounceTracker,
               "A site must only be in one of the maps at a time.");
  }

  if (aSkipStorage || !ShouldPersistToDisk()) {
    return NS_OK;
  }

  NS_ENSURE_TRUE(mStorage, NS_ERROR_FAILURE);
  return mStorage->DeleteDBEntries(&mOriginAttributes, aSiteHost);
}

nsresult BounceTrackingStateGlobal::ClearByTimeRange(
    PRTime aFrom, Maybe<PRTime> aTo,
    Maybe<BounceTrackingProtectionStorage::EntryType> aEntryType,
    bool aSkipStorage) {
  NS_ENSURE_ARG_MIN(aFrom, 0);
  NS_ENSURE_TRUE(!aTo || aTo.value() > aFrom, NS_ERROR_INVALID_ARG);

  MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
          ("%s: Clearing user activations by time range from %" PRIu64
           " to %" PRIu64 " %s",
           __FUNCTION__, aFrom, aTo.valueOr(0), Describe().get()));

  // Clear in memory user activation data.
  if (aEntryType.isNothing() ||
      aEntryType.value() ==
          BounceTrackingProtectionStorage::EntryType::UserActivation) {
    for (auto iter = mUserActivation.Iter(); !iter.Done(); iter.Next()) {
      if (iter.Data() >= aFrom &&
          (aTo.isNothing() || iter.Data() <= aTo.value())) {
        MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
                ("%s: Remove user activation for %s", __FUNCTION__,
                 PromiseFlatCString(iter.Key()).get()));
        iter.Remove();
      }
    }
  }

  // Clear in memory bounce tracker data.
  if (aEntryType.isNothing() ||
      aEntryType.value() ==
          BounceTrackingProtectionStorage::EntryType::BounceTracker) {
    for (auto iter = mBounceTrackers.Iter(); !iter.Done(); iter.Next()) {
      if (iter.Data() >= aFrom &&
          (aTo.isNothing() || iter.Data() <= aTo.value())) {
        MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
                ("%s: Remove bouncer tracker for %s", __FUNCTION__,
                 PromiseFlatCString(iter.Key()).get()));
        iter.Remove();
      }
    }
  }

  if (aSkipStorage || !ShouldPersistToDisk()) {
    return NS_OK;
  }

  // Write the change to storage.
  NS_ENSURE_TRUE(mStorage, NS_ERROR_FAILURE);
  return mStorage->DeleteDBEntriesInTimeRange(&mOriginAttributes, aFrom, aTo,
                                              aEntryType);
}

bool BounceTrackingStateGlobal::HasBounceTracker(
    const nsACString& aSiteHost) const {
  return mBounceTrackers.Contains(aSiteHost);
}

nsresult BounceTrackingStateGlobal::RecordBounceTracker(
    const nsACString& aSiteHost, PRTime aTime, bool aSkipStorage) {
  NS_ENSURE_TRUE(aSiteHost.Length(), NS_ERROR_INVALID_ARG);
  NS_ENSURE_TRUE(aTime > 0, NS_ERROR_INVALID_ARG);

  // Can not record a bounce tracker if the site has a user activation.
  NS_ENSURE_TRUE(!mUserActivation.Contains(aSiteHost), NS_ERROR_FAILURE);
  mBounceTrackers.InsertOrUpdate(aSiteHost, aTime);

  if (aSkipStorage || !ShouldPersistToDisk()) {
    return NS_OK;
  }

  // Write the change to storage.
  NS_ENSURE_TRUE(mStorage, NS_ERROR_FAILURE);
  return mStorage->UpdateDBEntry(
      mOriginAttributes, aSiteHost,
      BounceTrackingProtectionStorage::EntryType::BounceTracker, aTime);
}

nsresult BounceTrackingStateGlobal::RemoveBounceTrackers(
    const nsTArray<nsCString>& aSiteHosts) {
  for (const nsCString& siteHost : aSiteHosts) {
    mBounceTrackers.Remove(siteHost);

    // TODO: Create a bulk delete query.
    if (ShouldPersistToDisk()) {
      NS_ENSURE_TRUE(mStorage, NS_ERROR_FAILURE);
      nsresult rv = mStorage->DeleteDBEntries(&mOriginAttributes, siteHost);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

// static
nsCString BounceTrackingStateGlobal::DescribeMap(
    const nsTHashMap<nsCStringHashKey, PRTime>& aMap) {
  nsAutoCString mapStr;

  for (auto iter = aMap.ConstIter(); !iter.Done(); iter.Next()) {
    mapStr.Append(nsPrintfCString("{ %s: %" PRIu64 " }, ",
                                  PromiseFlatCString(iter.Key()).get(),
                                  iter.Data()));
  }

  return std::move(mapStr);
}

nsCString BounceTrackingStateGlobal::Describe() {
  nsAutoCString originAttributeSuffix;
  mOriginAttributes.CreateSuffix(originAttributeSuffix);
  return nsPrintfCString(
      "{ mOriginAttributes: %s, mUserActivation: %s, mBounceTrackers: %s }",
      originAttributeSuffix.get(), DescribeMap(mUserActivation).get(),
      DescribeMap(mBounceTrackers).get());
}

}  // namespace mozilla
