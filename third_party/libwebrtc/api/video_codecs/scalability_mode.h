/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_SCALABILITY_MODE_H_
#define API_VIDEO_CODECS_SCALABILITY_MODE_H_

namespace webrtc {

// Supported scalability modes. Most applications should use the
// PeerConnection-level apis where scalability mode is represented as a string.
// This list of currently recognized modes is intended for the api boundary
// between webrtc and injected encoders. Any application usage outside of
// injected encoders is strongly discouraged.
enum class ScalabilityMode {
  kL1T1,
  kL1T2,
  kL1T2h,
  kL1T3,
  kL1T3h,
  kL2T1,
  kL2T1h,
  kL2T1_KEY,
  kL2T2,
  kL2T2h,
  kL2T2_KEY,
  kL2T2_KEY_SHIFT,
  kL2T3,
  kL2T3h,
  kL2T3_KEY,
  kL3T1,
  kL3T1h,
  kL3T1_KEY,
  kL3T2,
  kL3T2h,
  kL3T2_KEY,
  kL3T3,
  kL3T3h,
  kL3T3_KEY,
  kS2T1,
  kS3T3,
};

}  // namespace webrtc
#endif  // API_VIDEO_CODECS_SCALABILITY_MODE_H_
