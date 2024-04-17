/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BounceTrackingRecord.h"
#include "mozilla/Logging.h"
#include "nsPrintfCString.h"

namespace mozilla {

extern LazyLogModule gBounceTrackingProtectionLog;

void BounceTrackingRecord::SetInitialHost(const nsACString& aHost) {
  mInitialHost = aHost;
}

const nsACString& BounceTrackingRecord::GetInitialHost() const {
  return mInitialHost;
}

void BounceTrackingRecord::SetFinalHost(const nsACString& aHost) {
  mFinalHost = aHost;
}

const nsACString& BounceTrackingRecord::GetFinalHost() const {
  return mFinalHost;
}

void BounceTrackingRecord::AddBounceHost(const nsACString& aHost) {
  MOZ_ASSERT(!aHost.IsEmpty());

  mBounceHosts.Insert(aHost);
  MOZ_LOG(gBounceTrackingProtectionLog, LogLevel::Debug,
          ("%s: %s", __FUNCTION__, Describe().get()));
}

// static
nsCString BounceTrackingRecord::DescribeSet(const nsTHashSet<nsCString>& set) {
  nsAutoCString setStr;

  setStr.AppendLiteral("[");

  if (!set.IsEmpty()) {
    for (const nsACString& host : set) {
      setStr.Append(host);
      setStr.AppendLiteral(",");
    }
    setStr.Truncate(setStr.Length() - 1);
  }

  setStr.AppendLiteral("]");

  return std::move(setStr);
}

void BounceTrackingRecord::AddStorageAccessHost(const nsACString& aHost) {
  MOZ_ASSERT(!aHost.IsEmpty());

  mStorageAccessHosts.Insert(aHost);
}

const nsTHashSet<nsCString>& BounceTrackingRecord::GetBounceHosts() const {
  return mBounceHosts;
}

const nsTHashSet<nsCString>& BounceTrackingRecord::GetStorageAccessHosts()
    const {
  return mStorageAccessHosts;
}

nsCString BounceTrackingRecord::Describe() {
  return nsPrintfCString(
      "{mInitialHost:%s, mFinalHost:%s, mBounceHosts:%s, "
      "mStorageAccessHosts:%s}",
      mInitialHost.get(), mFinalHost.get(), DescribeSet(mBounceHosts).get(),
      DescribeSet(mStorageAccessHosts).get());
}

}  // namespace mozilla
