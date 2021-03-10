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
};

}  // namespace mozilla::glean

#endif /* mozilla_glean_DistributionData_h */
