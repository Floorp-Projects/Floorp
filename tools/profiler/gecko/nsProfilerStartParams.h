/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NSPROFILERSTARTPARAMS_H_
#define _NSPROFILERSTARTPARAMS_H_

#include "nsIProfiler.h"
#include "nsString.h"
#include "nsTArray.h"

class nsProfilerStartParams : public nsIProfilerStartParams
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROFILERSTARTPARAMS

  nsProfilerStartParams(uint32_t aEntries,
                        double aInterval,
                        uint32_t aFeatures,
                        const nsTArray<nsCString>& aFilters);

private:
  virtual ~nsProfilerStartParams();
  uint32_t mEntries;
  double mInterval;
  uint32_t mFeatures;
  nsTArray<nsCString> mFilters;
};

#endif
