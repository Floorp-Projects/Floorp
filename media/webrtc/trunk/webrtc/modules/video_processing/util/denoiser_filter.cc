/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/checks.h"
#include "webrtc/modules/video_processing/util/denoiser_filter.h"
#include "webrtc/modules/video_processing/util/denoiser_filter_c.h"
#include "webrtc/modules/video_processing/util/denoiser_filter_neon.h"
#include "webrtc/modules/video_processing/util/denoiser_filter_sse2.h"
#include "webrtc/system_wrappers/include/cpu_features_wrapper.h"

namespace webrtc {

const int kMotionMagnitudeThreshold = 8 * 3;
const int kSumDiffThreshold = 16 * 16 * 2;
const int kSumDiffThresholdHigh = 600;

rtc::scoped_ptr<DenoiserFilter> DenoiserFilter::Create(
    bool runtime_cpu_detection) {
  rtc::scoped_ptr<DenoiserFilter> filter;

  if (runtime_cpu_detection) {
// If we know the minimum architecture at compile time, avoid CPU detection.
#if defined(WEBRTC_ARCH_X86_FAMILY)
    // x86 CPU detection required.
    if (WebRtc_GetCPUInfo(kSSE2)) {
      filter.reset(new DenoiserFilterSSE2());
    } else {
      filter.reset(new DenoiserFilterC());
    }
#elif defined(WEBRTC_DETECT_NEON)
    if (WebRtc_GetCPUFeaturesARM() & kCPUFeatureNEON) {
      filter.reset(new DenoiserFilterNEON());
    } else {
      filter.reset(new DenoiserFilterC());
    }
#else
    filter.reset(new DenoiserFilterC());
#endif
  } else {
    filter.reset(new DenoiserFilterC());
  }

  RTC_DCHECK(filter.get() != nullptr);
  return filter;
}

}  // namespace webrtc
