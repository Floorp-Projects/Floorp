/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_BounceTrackingRecord_h
#define mozilla_BounceTrackingRecord_h

#include "nsISupports.h"
#include "nsStringFwd.h"
#include "nsCycleCollectionParticipant.h"
#include "nsTHashSet.h"

namespace mozilla {

namespace dom {
class CanonicalBrowsingContext;
}

// Stores per-tab data relevant to bounce tracking protection for every extended
// navigation.
class BounceTrackingRecord final {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(BounceTrackingRecord);
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(BounceTrackingRecord);

  void SetInitialHost(const nsACString& aHost);

  const nsACString& GetInitialHost();

  void SetFinalHost(const nsACString& aHost);

  const nsACString& GetFinalHost();

  void AddBounceHost(const nsACString& aHost);

  void AddStorageAccessHost(const nsACString& aHost);

  const nsTHashSet<nsCString>& GetBounceHosts();

  const nsTHashSet<nsCString>& GetStorageAccessHosts();

  // Create a string that describes this record. Used for logging.
  nsCString Describe();

 private:
  ~BounceTrackingRecord() = default;

  // A site's host. The initiator site of the current extended navigation.
  nsAutoCString mInitialHost;

  // A site's host or null. The destination of the current extended navigation.
  // Updated after every document load.
  nsAutoCString mFinalHost;

  // A set of sites' hosts. All server-side and client-side redirects hit during
  // this extended navigation.
  nsTHashSet<nsCString> mBounceHosts;

  // A set of sites' hosts. All sites which accessed storage during this
  // extended navigation.
  nsTHashSet<nsCString> mStorageAccessHosts;

  // Create a comma-delimited string that describes a string set. Used for
  // logging.
  static nsCString DescribeSet(const nsTHashSet<nsCString>& set);
};

}  // namespace mozilla

#endif
