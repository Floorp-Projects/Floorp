/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_processing/main/source/denoising.h"
#include "webrtc/system_wrappers/interface/trace.h"

#include <string.h>

namespace webrtc {
// Down-sampling in time (unit: number of frames)
enum { kSubsamplingTime = 0 };
// Sub-sampling in width (unit: power of 2.
enum { kSubsamplingWidth = 0 };
// Sub-sampling in height (unit: power of 2)
enum { kSubsamplingHeight = 0 };
// (Q8) De-noising filter parameter
enum { kDenoiseFiltParam = 179 };
// (Q8) 1 - filter parameter
enum { kDenoiseFiltParamRec = 77 };
// (Q8) De-noising threshold level
enum { kDenoiseThreshold = 19200 };

VPMDenoising::VPMDenoising()
    : id_(0),
      moment1_(NULL),
      moment2_(NULL) {
  Reset();
}

VPMDenoising::~VPMDenoising() {
  if (moment1_) {
    delete [] moment1_;
    moment1_ = NULL;
}

  if (moment2_) {
    delete [] moment2_;
    moment2_ = NULL;
  }
}

int32_t VPMDenoising::ChangeUniqueId(const int32_t id) {
  id_ = id;
  return VPM_OK;
}

void VPMDenoising::Reset() {
  frame_size_ = 0;
  denoise_frame_cnt_ = 0;

  if (moment1_) {
    delete [] moment1_;
    moment1_ = NULL;
  }

  if (moment2_) {
    delete [] moment2_;
    moment2_ = NULL;
  }
}

int32_t VPMDenoising::ProcessFrame(I420VideoFrame* frame) {
  assert(frame);
  int32_t thevar;
  int k;
  int jsub, ksub;
  int32_t diff0;
  uint32_t tmp_moment1;
  uint32_t tmp_moment2;
  uint32_t tmp;
  int32_t  num_pixels_changed = 0;

  if (frame->IsZeroSize()) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoPreocessing, id_,
                 "zero size frame");
    return VPM_GENERAL_ERROR;
  }

  int width = frame->width();
  int height = frame->height();

  /* Size of luminance component */
  const uint32_t y_size  = height * width;

  /* Initialization */
  if (y_size != frame_size_) {
    delete [] moment1_;
    moment1_ = NULL;

    delete [] moment2_;
    moment2_ = NULL;
  }
  frame_size_ = y_size;

  if (!moment1_) {
    moment1_ = new uint32_t[y_size];
    memset(moment1_, 0, sizeof(uint32_t)*y_size);
  }

  if (!moment2_) {
    moment2_ = new uint32_t[y_size];
    memset(moment2_, 0, sizeof(uint32_t)*y_size);
  }

  /* Apply de-noising on each pixel, but update variance sub-sampled */
  uint8_t* buffer = frame->buffer(kYPlane);
  for (int i = 0; i < height; i++) {  // Collect over height
    k = i * width;
    ksub = ((i >> kSubsamplingHeight) << kSubsamplingHeight) * width;
    for (int j = 0; j < width; j++) {  // Collect over width
      jsub = ((j >> kSubsamplingWidth) << kSubsamplingWidth);
      /* Update mean value for every pixel and every frame */
      tmp_moment1 = moment1_[k + j];
      tmp_moment1 *= kDenoiseFiltParam;  // Q16
      tmp_moment1 += ((kDenoiseFiltParamRec * ((uint32_t)buffer[k + j])) << 8);
      tmp_moment1 >>= 8;  // Q8
      moment1_[k + j] = tmp_moment1;

      tmp_moment2 = moment2_[ksub + jsub];
      if ((ksub == k) && (jsub == j) && (denoise_frame_cnt_ == 0)) {
        tmp = ((uint32_t)buffer[k + j] *
              (uint32_t)buffer[k + j]);
        tmp_moment2 *= kDenoiseFiltParam;  // Q16
        tmp_moment2 += ((kDenoiseFiltParamRec * tmp) << 8);
        tmp_moment2 >>= 8;  // Q8
      }
       moment2_[k + j] = tmp_moment2;
      /* Current event = deviation from mean value */
      diff0 = ((int32_t)buffer[k + j] << 8) - moment1_[k + j];
      /* Recent events = variance (variations over time) */
      thevar = moment2_[k + j];
      thevar -= ((moment1_[k + j] * moment1_[k + j]) >> 8);
      // De-noising criteria, i.e., when should we replace a pixel by its mean.
      // 1) recent events are minor.
      // 2) current events are minor.
      if ((thevar < kDenoiseThreshold)
          && ((diff0 * diff0 >> 8) < kDenoiseThreshold)) {
        // Replace with mean.
        buffer[k + j] = (uint8_t)(moment1_[k + j] >> 8);
        num_pixels_changed++;
      }
    }
  }

  denoise_frame_cnt_++;
  if (denoise_frame_cnt_ > kSubsamplingTime)
    denoise_frame_cnt_ = 0;

  return num_pixels_changed;
}

}  // namespace
