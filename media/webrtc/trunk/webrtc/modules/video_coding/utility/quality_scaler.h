/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_UTILITY_QUALITY_SCALER_H_
#define WEBRTC_MODULES_VIDEO_CODING_UTILITY_QUALITY_SCALER_H_

#include <list>

#include "webrtc/common_video/libyuv/include/scaler.h"

namespace webrtc {
class QualityScaler {
 public:
  struct Resolution {
    int width;
    int height;
  };

  QualityScaler();
  void Init(int max_qp);

  void ReportFramerate(int framerate);
  void ReportEncodedFrame(int qp);
  void ReportDroppedFrame();

  Resolution GetScaledResolution(const I420VideoFrame& frame);
  const I420VideoFrame& GetScaledFrame(const I420VideoFrame& frame);

 private:
  class MovingAverage {
   public:
    MovingAverage();
    void AddSample(int sample);
    bool GetAverage(size_t num_samples, int* average);
    void Reset();

   private:
    int sum_;
    std::list<int> samples_;
  };

  void AdjustScale(bool up);
  void ClearSamples();

  Scaler scaler_;
  I420VideoFrame scaled_frame_;

  size_t num_samples_;
  int low_qp_threshold_;
  MovingAverage average_qp_;
  MovingAverage framedrop_percent_;

  int downscale_shift_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_UTILITY_QUALITY_SCALER_H_
