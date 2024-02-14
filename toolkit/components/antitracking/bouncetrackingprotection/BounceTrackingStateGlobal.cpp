/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BounceTrackingStateGlobal.h"
#include "BounceTrackingProtectionStorage.h"
#include "ErrorList.h"
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

  mUserActivation.InsertOrUpdate(aSiteHost, aTime);

  if (aSkipStorage || !ShouldPersistToDisk()) {
    return NS_OK;
  }

  // Write the change to storage.
  NS_ENSURE_TRUE(mStorage, NS_ERROR_FAILURE);
  return mStorage->UpdateEntry(
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
  return mStorage->DeleteEntry(mOriginAttributes, aSiteHost);
}

nsresult BounceTrackingStateGlobal::ClearUserActivationBefore(PRTime aTime) {
  NS_ENSURE_TRUE(aTime > 0, NS_ERROR_INVALID_ARG);

  for (auto iter = mUserActivation.Iter(); !iter.Done(); iter.Next()) {
    if (iter.Data() < aTime) {
      iter.Remove();
      MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
              ("%s: Remove user activation for %s", __FUNCTION__,
               PromiseFlatCString(iter.Key()).get()));
    }
  }

  if (!ShouldPersistToDisk()) {
    return NS_OK;
  }

  // Write the change to storage.
  NS_ENSURE_TRUE(mStorage, NS_ERROR_FAILURE);
  return mStorage->DeleteEntriesBefore(
      mOriginAttributes, aTime,
      Some(BounceTrackingProtectionStorage::EntryType::UserActivation));
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
  return mStorage->UpdateEntry(
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
      mStorage->DeleteEntry(mOriginAttributes, siteHost);
    }
  }

  return NS_OK;
}

}  // namespace mozilla
