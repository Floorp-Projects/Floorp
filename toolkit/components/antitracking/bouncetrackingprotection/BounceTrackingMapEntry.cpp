/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BounceTrackingMapEntry.h"

namespace mozilla {

NS_IMPL_ISUPPORTS(BounceTrackingMapEntry, nsIBounceTrackingMapEntry);

NS_IMETHODIMP BounceTrackingMapEntry::GetSiteHost(nsACString& aSiteHost) {
  aSiteHost = mSiteHost;
  return NS_OK;
}
NS_IMETHODIMP BounceTrackingMapEntry::GetTimeStamp(int64_t* aTimeStamp) {
  *aTimeStamp = mTimeStamp;
  return NS_OK;
}

}  // namespace mozilla
