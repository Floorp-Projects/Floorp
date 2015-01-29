/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_COMMON_AUDIO_WINDOW_GENERATOR_H_
#define WEBRTC_COMMON_AUDIO_WINDOW_GENERATOR_H_

#include "webrtc/base/constructormagic.h"

namespace webrtc {

// Helper class with generators for various signal transform windows.
class WindowGenerator {
 public:
  static void Hanning(int length, float* window);
  static void KaiserBesselDerived(float alpha, int length, float* window);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(WindowGenerator);
};

}  // namespace webrtc

#endif  // WEBRTC_COMMON_AUDIO_WINDOW_GENERATOR_H_

