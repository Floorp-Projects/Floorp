/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_BounceTrackingMapEntry_h
#define mozilla_BounceTrackingMapEntry_h

#include "nsIBounceTrackingMapEntry.h"
#include "nsString.h"

namespace mozilla {

/**
 * Represents an entry in the global bounce tracker or user activation map.
 */
class BounceTrackingMapEntry final : public nsIBounceTrackingMapEntry {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIBOUNCETRACKINGMAPENTRY

  BounceTrackingMapEntry(const nsACString& aSiteHost, PRTime aTimeStamp)
      : mSiteHost(aSiteHost), mTimeStamp(aTimeStamp) {}

 private:
  ~BounceTrackingMapEntry() = default;

  nsAutoCString mSiteHost;
  PRTime mTimeStamp;
};

}  // namespace mozilla

#endif
