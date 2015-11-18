/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_BEAMFORMER_ARRAY_UTIL_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_BEAMFORMER_ARRAY_UTIL_H_

#include <cmath>

namespace webrtc {

// Coordinates in meters.
template<typename T>
struct CartesianPoint {
  CartesianPoint(T x, T y, T z) {
    c[0] = x;
    c[1] = y;
    c[2] = z;
  }
  T x() const {
    return c[0];
  }
  T y() const {
    return c[1];
  }
  T z() const {
    return c[2];
  }
  T c[3];
};

typedef CartesianPoint<float> Point;

template<typename T>
float Distance(CartesianPoint<T> a, CartesianPoint<T> b) {
  return std::sqrt((a.x() - b.x()) * (a.x() - b.x()) +
                   (a.y() - b.y()) * (a.y() - b.y()) +
                   (a.z() - b.z()) * (a.z() - b.z()));
}

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_BEAMFORMER_ARRAY_UTIL_H_
