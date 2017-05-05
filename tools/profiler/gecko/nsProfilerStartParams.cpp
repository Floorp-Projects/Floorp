/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsProfilerStartParams.h"

NS_IMPL_ISUPPORTS(nsProfilerStartParams, nsIProfilerStartParams)

nsProfilerStartParams::nsProfilerStartParams(uint32_t aEntries,
                                             double aInterval,
                                             uint32_t aFeatures,
                                             const nsTArray<nsCString>& aFilters) :
  mEntries(aEntries),
  mInterval(aInterval),
  mFeatures(aFeatures),
  mFilters(aFilters)
{
}

nsProfilerStartParams::~nsProfilerStartParams()
{
}

NS_IMETHODIMP
nsProfilerStartParams::GetEntries(uint32_t* aEntries)
{
  NS_ENSURE_ARG_POINTER(aEntries);
  *aEntries = mEntries;
  return NS_OK;
}

NS_IMETHODIMP
nsProfilerStartParams::GetInterval(double* aInterval)
{
  NS_ENSURE_ARG_POINTER(aInterval);
  *aInterval = mInterval;
  return NS_OK;
}

NS_IMETHODIMP
nsProfilerStartParams::GetFeatures(uint32_t* aFeatures)
{
  NS_ENSURE_ARG_POINTER(aFeatures);
  *aFeatures = mFeatures;
  return NS_OK;
}

const nsTArray<nsCString>&
nsProfilerStartParams::GetFilters()
{
  return mFilters;
}
