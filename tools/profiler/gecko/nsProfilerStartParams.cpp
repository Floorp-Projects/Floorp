/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsProfilerStartParams.h"
#include "ipc/IPCMessageUtils.h"

NS_IMPL_ISUPPORTS(nsProfilerStartParams, nsIProfilerStartParams)

nsProfilerStartParams::nsProfilerStartParams(
    uint32_t aEntries, const mozilla::Maybe<double>& aDuration,
    double aInterval, uint32_t aFeatures, nsTArray<nsCString>&& aFilters)
    : mEntries(aEntries),
      mDuration(aDuration),
      mInterval(aInterval),
      mFeatures(aFeatures),
      mFilters(std::move(aFilters)) {}

nsProfilerStartParams::~nsProfilerStartParams() {}

NS_IMETHODIMP
nsProfilerStartParams::GetEntries(uint32_t* aEntries) {
  NS_ENSURE_ARG_POINTER(aEntries);
  *aEntries = mEntries;
  return NS_OK;
}

NS_IMETHODIMP
nsProfilerStartParams::GetDuration(double* aDuration) {
  NS_ENSURE_ARG_POINTER(aDuration);
  if (mDuration) {
    *aDuration = *mDuration;
  } else {
    *aDuration = 0;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsProfilerStartParams::GetInterval(double* aInterval) {
  NS_ENSURE_ARG_POINTER(aInterval);
  *aInterval = mInterval;
  return NS_OK;
}

NS_IMETHODIMP
nsProfilerStartParams::GetFeatures(uint32_t* aFeatures) {
  NS_ENSURE_ARG_POINTER(aFeatures);
  *aFeatures = mFeatures;
  return NS_OK;
}

const nsTArray<nsCString>& nsProfilerStartParams::GetFilters() {
  return mFilters;
}
