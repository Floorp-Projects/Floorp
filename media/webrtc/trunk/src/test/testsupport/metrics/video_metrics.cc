/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testsupport/metrics/video_metrics.h"

#include <algorithm> // min_element, max_element
#include <cassert>
#include <cstdio>

#include "common_video/libyuv/include/webrtc_libyuv.h"

namespace webrtc {
namespace test {

// Used for calculating min and max values
static bool LessForFrameResultValue (const FrameResult& s1,
                                     const FrameResult& s2) {
    return s1.value < s2.value;
}

enum VideoMetricsType { kPSNR, kSSIM, kBoth };

// Calculates metrics for a frame and adds statistics to the result for it.
void CalculateFrame(VideoMetricsType video_metrics_type,
                    uint8_t* ref,
                    uint8_t* test,
                    int width,
                    int height,
                    int frame_number,
                    QualityMetricsResult* result) {
  FrameResult frame_result = {0, 0};
  frame_result.frame_number = frame_number;
  switch (video_metrics_type) {
    case kPSNR:
      frame_result.value = I420PSNR(ref, test, width, height);
      break;
    case kSSIM:
      frame_result.value = I420SSIM(ref, test, width, height);
      break;
    default:
      assert(false);
  }
  result->frames.push_back(frame_result);
}

// Calculates average, min and max values for the supplied struct, if non-NULL.
void CalculateStats(QualityMetricsResult* result) {
  if (result == NULL || result->frames.size() == 0) {
    return;
  }
  // Calculate average
  std::vector<FrameResult>::iterator iter;
  double metrics_values_sum = 0.0;
  for (iter = result->frames.begin(); iter != result->frames.end(); ++iter) {
    metrics_values_sum += iter->value;
  }
  result->average = metrics_values_sum / result->frames.size();

  // Calculate min/max statistics
  iter = std::min_element(result->frames.begin(), result->frames.end(),
                     LessForFrameResultValue);
  result->min = iter->value;
  result->min_frame_number = iter->frame_number;
  iter = std::max_element(result->frames.begin(), result->frames.end(),
                     LessForFrameResultValue);
  result->max = iter->value;
  result->max_frame_number = iter->frame_number;
}

// Single method that handles all combinations of video metrics calculation, to
// minimize code duplication. Either psnr_result or ssim_result may be NULL,
// depending on which VideoMetricsType is targeted.
int CalculateMetrics(VideoMetricsType video_metrics_type,
                     const char* ref_filename,
                     const char* test_filename,
                     int width,
                     int height,
                     QualityMetricsResult* psnr_result,
                     QualityMetricsResult* ssim_result) {
  assert(ref_filename != NULL);
  assert(test_filename != NULL);
  assert(width > 0);
  assert(height > 0);

  FILE* ref_fp = fopen(ref_filename, "rb");
  if (ref_fp == NULL) {
    // cannot open reference file
    fprintf(stderr, "Cannot open file %s\n", ref_filename);
    return -1;
  }
  FILE* test_fp = fopen(test_filename, "rb");
  if (test_fp == NULL) {
    // cannot open test file
    fprintf(stderr, "Cannot open file %s\n", test_filename);
    fclose(ref_fp);
    return -2;
  }
  int frame_number = 0;

  // Allocating size for one I420 frame.
  const int frame_length = 3 * width * height >> 1;
  uint8_t* ref = new uint8_t[frame_length];
  uint8_t* test = new uint8_t[frame_length];

  int ref_bytes = fread(ref, 1, frame_length, ref_fp);
  int test_bytes = fread(test, 1, frame_length, test_fp);
  while (ref_bytes == frame_length && test_bytes == frame_length) {
    switch (video_metrics_type) {
      case kPSNR:
        CalculateFrame(kPSNR, ref, test, width, height, frame_number,
                       psnr_result);
        break;
      case kSSIM:
        CalculateFrame(kSSIM, ref, test, width, height, frame_number,
                       ssim_result);
        break;
      case kBoth:
        CalculateFrame(kPSNR, ref, test, width, height, frame_number,
                       psnr_result);
        CalculateFrame(kSSIM, ref, test, width, height, frame_number,
                       ssim_result);
        break;
    }
    frame_number++;
    ref_bytes = fread(ref, 1, frame_length, ref_fp);
    test_bytes = fread(test, 1, frame_length, test_fp);
  }
  int return_code = 0;
  if (frame_number == 0) {
    fprintf(stderr, "Tried to measure video metrics from empty files "
            "(reference file: %s  test file: %s)\n", ref_filename,
            test_filename);
    return_code = -3;
  } else {
    CalculateStats(psnr_result);
    CalculateStats(ssim_result);
  }
  delete [] ref;
  delete [] test;
  fclose(ref_fp);
  fclose(test_fp);
  return return_code;
}

int I420MetricsFromFiles(const char* ref_filename,
                         const char* test_filename,
                         int width,
                         int height,
                         QualityMetricsResult* psnr_result,
                         QualityMetricsResult* ssim_result) {
  assert(psnr_result != NULL);
  assert(ssim_result != NULL);
  return CalculateMetrics(kBoth, ref_filename, test_filename, width, height,
                          psnr_result, ssim_result);
}

int I420PSNRFromFiles(const char* ref_filename,
                      const char* test_filename,
                      int width,
                      int height,
                      QualityMetricsResult* result) {
  assert(result != NULL);
  return CalculateMetrics(kPSNR, ref_filename, test_filename, width, height,
                          result, NULL);
}

int I420SSIMFromFiles(const char* ref_filename,
                      const char* test_filename,
                      int width,
                      int height,
                      QualityMetricsResult* result) {
  assert(result != NULL);
  return CalculateMetrics(kSSIM, ref_filename, test_filename, width, height,
                          NULL, result);
}

}  // namespace test
}  // namespace webrtc
