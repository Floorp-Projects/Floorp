/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsProfilerStartParams.h"

NS_IMPL_ISUPPORTS(nsProfilerStartParams, nsIProfilerStartParams)

nsProfilerStartParams::nsProfilerStartParams(uint32_t aEntries,
                                             double aInterval,
                                             const nsTArray<nsCString>& aFeatures,
                                             const nsTArray<nsCString>& aThreadFilterNames) :
  mEntries(aEntries),
  mInterval(aInterval),
  mFeatures(aFeatures),
  mThreadFilterNames(aThreadFilterNames)
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
nsProfilerStartParams::SetEntries(uint32_t aEntries)
{
  NS_ENSURE_ARG(aEntries);
  mEntries = aEntries;
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
nsProfilerStartParams::SetInterval(double aInterval)
{
  NS_ENSURE_ARG(aInterval);
  mInterval = aInterval;
  return NS_OK;
}

const nsTArray<nsCString>&
nsProfilerStartParams::GetFeatures()
{
  return mFeatures;
}

const nsTArray<nsCString>&
nsProfilerStartParams::GetThreadFilterNames()
{
  return mThreadFilterNames;
}
