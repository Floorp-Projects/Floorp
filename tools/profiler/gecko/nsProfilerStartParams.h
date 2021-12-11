/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NSPROFILERSTARTPARAMS_H_
#define _NSPROFILERSTARTPARAMS_H_

#include "nsIProfiler.h"
#include "nsString.h"
#include "nsTArray.h"

class nsProfilerStartParams : public nsIProfilerStartParams {
 public:
  // This class can be used on multiple threads. For example, it's used for the
  // observer notification from profiler_start, which can run on any thread but
  // posts the notification to the main thread.
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIPROFILERSTARTPARAMS

  nsProfilerStartParams(uint32_t aEntries,
                        const mozilla::Maybe<double>& aDuration,
                        double aInterval, uint32_t aFeatures,
                        nsTArray<nsCString>&& aFilters, uint64_t aActiveTabID);

 private:
  virtual ~nsProfilerStartParams();
  uint32_t mEntries;
  mozilla::Maybe<double> mDuration;
  double mInterval;
  uint32_t mFeatures;
  nsTArray<nsCString> mFilters;
  uint64_t mActiveTabID;
};

#endif
