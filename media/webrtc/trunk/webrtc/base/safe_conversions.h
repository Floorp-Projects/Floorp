/*
 *  Copyright 2014 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Borrowed from Chromium's src/base/numerics/safe_conversions.h.

#ifndef WEBRTC_BASE_SAFE_CONVERSIONS_H_
#define WEBRTC_BASE_SAFE_CONVERSIONS_H_

#include <limits>

#include "webrtc/base/common.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/safe_conversions_impl.h"

namespace rtc {

inline void Check(bool condition) {
  if (!condition) {
    LOG(LS_ERROR) << "CHECK failed.";
    Break();
    // The program should have crashed at this point.
  }
}

// Convenience function that returns true if the supplied value is in range
// for the destination type.
template <typename Dst, typename Src>
inline bool IsValueInRangeForNumericType(Src value) {
  return internal::RangeCheck<Dst>(value) == internal::TYPE_VALID;
}

// checked_cast<> is analogous to static_cast<> for numeric types,
// except that it CHECKs that the specified numeric conversion will not
// overflow or underflow. NaN source will always trigger a CHECK.
template <typename Dst, typename Src>
inline Dst checked_cast(Src value) {
  Check(IsValueInRangeForNumericType<Dst>(value));
  return static_cast<Dst>(value);
}

// saturated_cast<> is analogous to static_cast<> for numeric types, except
// that the specified numeric conversion will saturate rather than overflow or
// underflow. NaN assignment to an integral will trigger a CHECK condition.
template <typename Dst, typename Src>
inline Dst saturated_cast(Src value) {
  // Optimization for floating point values, which already saturate.
  if (std::numeric_limits<Dst>::is_iec559)
    return static_cast<Dst>(value);

  switch (internal::RangeCheck<Dst>(value)) {
    case internal::TYPE_VALID:
      return static_cast<Dst>(value);

    case internal::TYPE_UNDERFLOW:
      return std::numeric_limits<Dst>::min();

    case internal::TYPE_OVERFLOW:
      return std::numeric_limits<Dst>::max();

    // Should fail only on attempting to assign NaN to a saturated integer.
    case internal::TYPE_INVALID:
      Check(false);
      return std::numeric_limits<Dst>::max();
  }

  Check(false); // NOTREACHED();
  return static_cast<Dst>(value);
}

}  // namespace rtc

#endif  // WEBRTC_BASE_SAFE_CONVERSIONS_H_
