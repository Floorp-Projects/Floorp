/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glean_DistributionData_h
#define mozilla_glean_DistributionData_h

#include "nsTHashMap.h"

namespace mozilla::glean {

struct DistributionData final {
  uint64_t sum;
  nsTHashMap<nsUint64HashKey, uint64_t> values;

  /**
   * Create distribution data from the buckets, counts and sum,
   * as returned by `fog_*_distribution_test_get_value`.
   */
  DistributionData(const nsTArray<uint64_t>& aBuckets,
                   const nsTArray<uint64_t>& aCounts, uint64_t aSum)
      : sum(aSum) {
    for (size_t i = 0; i < aBuckets.Length(); ++i) {
      this->values.InsertOrUpdate(aBuckets[i], aCounts[i]);
    }
  }
};

}  // namespace mozilla::glean

#endif /* mozilla_glean_DistributionData_h */
