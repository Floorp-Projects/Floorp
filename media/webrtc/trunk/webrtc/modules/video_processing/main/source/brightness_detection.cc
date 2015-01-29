/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_processing/main/interface/video_processing.h"
#include "webrtc/modules/video_processing/main/source/brightness_detection.h"

#include <math.h>

namespace webrtc {

VPMBrightnessDetection::VPMBrightnessDetection() :
    id_(0) {
  Reset();
}

VPMBrightnessDetection::~VPMBrightnessDetection() {}

int32_t VPMBrightnessDetection::ChangeUniqueId(const int32_t id) {
  id_ = id;
  return VPM_OK;
}

void VPMBrightnessDetection::Reset() {
  frame_cnt_bright_ = 0;
  frame_cnt_dark_ = 0;
}

int32_t VPMBrightnessDetection::ProcessFrame(
    const I420VideoFrame& frame,
    const VideoProcessingModule::FrameStats& stats) {
  if (frame.IsZeroSize()) {
    return VPM_PARAMETER_ERROR;
  }
  int width = frame.width();
  int height = frame.height();

  if (!VideoProcessingModule::ValidFrameStats(stats)) {
    return VPM_PARAMETER_ERROR;
  }

  const uint8_t frame_cnt_alarm = 2;

  // Get proportion in lowest bins.
  uint8_t low_th = 20;
  float prop_low = 0;
  for (uint32_t i = 0; i < low_th; i++) {
    prop_low += stats.hist[i];
  }
  prop_low /= stats.num_pixels;

  // Get proportion in highest bins.
  unsigned char high_th = 230;
  float prop_high = 0;
  for (uint32_t i = high_th; i < 256; i++) {
    prop_high += stats.hist[i];
  }
  prop_high /= stats.num_pixels;

  if (prop_high < 0.4) {
    if (stats.mean < 90 || stats.mean > 170) {
      // Standard deviation of Y
      const uint8_t* buffer = frame.buffer(kYPlane);
      float std_y = 0;
      for (int h = 0; h < height; h += (1 << stats.subSamplHeight)) {
        int row = h*width;
        for (int w = 0; w < width; w += (1 << stats.subSamplWidth)) {
          std_y += (buffer[w + row] - stats.mean) * (buffer[w + row] -
              stats.mean);
        }
      }
      std_y = sqrt(std_y / stats.num_pixels);

      // Get percentiles.
      uint32_t sum = 0;
      uint32_t median_y = 140;
      uint32_t perc05 = 0;
      uint32_t perc95 = 255;
      float pos_perc05 = stats.num_pixels * 0.05f;
      float pos_median = stats.num_pixels * 0.5f;
      float posPerc95 = stats.num_pixels * 0.95f;
      for (uint32_t i = 0; i < 256; i++) {
        sum += stats.hist[i];
        if (sum < pos_perc05) perc05 = i;     // 5th perc.
        if (sum < pos_median) median_y = i;    // 50th perc.
        if (sum < posPerc95)
          perc95 = i;     // 95th perc.
        else
          break;
      }

        // Check if image is too dark
        if ((std_y < 55) && (perc05 < 50))  {
          if (median_y < 60 || stats.mean < 80 ||  perc95 < 130 ||
              prop_low > 0.20) {
            frame_cnt_dark_++;
          } else {
            frame_cnt_dark_ = 0;
          }
        } else {
          frame_cnt_dark_ = 0;
        }

        // Check if image is too bright
        if ((std_y < 52) && (perc95 > 200) && (median_y > 160)) {
          if (median_y > 185 || stats.mean > 185 || perc05 > 140 ||
              prop_high > 0.25) {
            frame_cnt_bright_++;
          } else {
            frame_cnt_bright_ = 0;
          }
        } else {
          frame_cnt_bright_ = 0;
        }
    } else {
      frame_cnt_dark_ = 0;
      frame_cnt_bright_ = 0;
    }
  } else {
    frame_cnt_bright_++;
    frame_cnt_dark_ = 0;
  }

  if (frame_cnt_dark_ > frame_cnt_alarm) {
    return VideoProcessingModule::kDarkWarning;
  } else if (frame_cnt_bright_ > frame_cnt_alarm) {
    return VideoProcessingModule::kBrightWarning;
  } else {
    return VideoProcessingModule::kNoWarning;
  }
}

}  // namespace webrtc
