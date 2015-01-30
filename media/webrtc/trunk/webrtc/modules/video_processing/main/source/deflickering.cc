/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_processing/main/source/deflickering.h"

#include <math.h>
#include <stdlib.h>

#include "webrtc/common_audio/signal_processing/include/signal_processing_library.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/sort.h"

namespace webrtc {

// Detection constants
// (Q4) Maximum allowed deviation for detection.
enum { kFrequencyDeviation = 39 };
// (Q4) Minimum frequency that can be detected.
enum { kMinFrequencyToDetect = 32 };
// Number of flickers before we accept detection
enum { kNumFlickerBeforeDetect = 2 };
enum { kmean_valueScaling = 4 };  // (Q4) In power of 2
// Dead-zone region in terms of pixel values
enum { kZeroCrossingDeadzone = 10 };
// Deflickering constants.
// Compute the quantiles over 1 / DownsamplingFactor of the image.
enum { kDownsamplingFactor = 8 };
enum { kLog2OfDownsamplingFactor = 3 };

// To generate in Matlab:
// >> probUW16 = round(2^11 *
//     [0.05,0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9,0.95,0.97]);
// >> fprintf('%d, ', probUW16)
// Resolution reduced to avoid overflow when multiplying with the
// (potentially) large number of pixels.
const uint16_t VPMDeflickering::prob_uw16_[kNumProbs] = {102, 205, 410, 614,
    819, 1024, 1229, 1434, 1638, 1843, 1946, 1987}; // <Q11>

// To generate in Matlab:
// >> numQuants = 14; maxOnlyLength = 5;
// >> weightUW16 = round(2^15 *
//    [linspace(0.5, 1.0, numQuants - maxOnlyLength)]);
// >> fprintf('%d, %d,\n ', weightUW16);
const uint16_t VPMDeflickering::weight_uw16_[kNumQuants - kMaxOnlyLength] =
    {16384, 18432, 20480, 22528, 24576, 26624, 28672, 30720, 32768}; // <Q15>

VPMDeflickering::VPMDeflickering()
    : id_(0) {
  Reset();
}

VPMDeflickering::~VPMDeflickering() {}

int32_t VPMDeflickering::ChangeUniqueId(const int32_t id) {
  id_ = id;
  return 0;
}

void VPMDeflickering::Reset() {
  mean_buffer_length_ = 0;
  detection_state_ = 0;
  frame_rate_ = 0;

  memset(mean_buffer_, 0, sizeof(int32_t) * kMeanBufferLength);
  memset(timestamp_buffer_, 0, sizeof(int32_t) * kMeanBufferLength);

  // Initialize the history with a uniformly distributed histogram.
  quant_hist_uw8_[0][0] = 0;
  quant_hist_uw8_[0][kNumQuants - 1] = 255;
  for (int32_t i = 0; i < kNumProbs; i++) {
    // Unsigned round. <Q0>
    quant_hist_uw8_[0][i + 1] = static_cast<uint8_t>(
        (prob_uw16_[i] * 255 + (1 << 10)) >> 11);
  }

  for (int32_t i = 1; i < kFrameHistory_size; i++) {
    memcpy(quant_hist_uw8_[i], quant_hist_uw8_[0],
           sizeof(uint8_t) * kNumQuants);
  }
}

int32_t VPMDeflickering::ProcessFrame(I420VideoFrame* frame,
    VideoProcessingModule::FrameStats* stats) {
  assert(frame);
  uint32_t frame_memory;
  uint8_t quant_uw8[kNumQuants];
  uint8_t maxquant_uw8[kNumQuants];
  uint8_t minquant_uw8[kNumQuants];
  uint16_t target_quant_uw16[kNumQuants];
  uint16_t increment_uw16;
  uint8_t map_uw8[256];

  uint16_t tmp_uw16;
  uint32_t tmp_uw32;
  int width = frame->width();
  int height = frame->height();

  if (frame->IsZeroSize()) {
    return VPM_GENERAL_ERROR;
  }

  // Stricter height check due to subsampling size calculation below.
  if (height < 2) {
    LOG(LS_ERROR) << "Invalid frame size.";
    return VPM_GENERAL_ERROR;
  }

  if (!VideoProcessingModule::ValidFrameStats(*stats)) {
    return VPM_GENERAL_ERROR;
  }

  if (PreDetection(frame->timestamp(), *stats) == -1) return VPM_GENERAL_ERROR;

  // Flicker detection
  int32_t det_flicker = DetectFlicker();
  if (det_flicker < 0) {
    return VPM_GENERAL_ERROR;
  } else if (det_flicker != 1) {
    return 0;
  }

  // Size of luminance component.
  const uint32_t y_size = height * width;

  const uint32_t y_sub_size = width * (((height - 1) >>
      kLog2OfDownsamplingFactor) + 1);
  uint8_t* y_sorted = new uint8_t[y_sub_size];
  uint32_t sort_row_idx = 0;
  for (int i = 0; i < height; i += kDownsamplingFactor) {
    memcpy(y_sorted + sort_row_idx * width,
        frame->buffer(kYPlane) + i * width, width);
    sort_row_idx++;
  }

  webrtc::Sort(y_sorted, y_sub_size, webrtc::TYPE_UWord8);

  uint32_t prob_idx_uw32 = 0;
  quant_uw8[0] = 0;
  quant_uw8[kNumQuants - 1] = 255;

  // Ensure we won't get an overflow below.
  // In practice, the number of subsampled pixels will not become this large.
  if (y_sub_size > (1 << 21) - 1) {
    LOG(LS_ERROR) << "Subsampled number of pixels too large.";
    return -1;
  }

  for (int32_t i = 0; i < kNumProbs; i++) {
    // <Q0>.
    prob_idx_uw32 = WEBRTC_SPL_UMUL_32_16(y_sub_size, prob_uw16_[i]) >> 11;
    quant_uw8[i + 1] = y_sorted[prob_idx_uw32];
  }

  delete [] y_sorted;
  y_sorted = NULL;

  // Shift history for new frame.
  memmove(quant_hist_uw8_[1], quant_hist_uw8_[0],
      (kFrameHistory_size - 1) * kNumQuants * sizeof(uint8_t));
  // Store current frame in history.
  memcpy(quant_hist_uw8_[0], quant_uw8, kNumQuants * sizeof(uint8_t));

  // We use a frame memory equal to the ceiling of half the frame rate to
  // ensure we capture an entire period of flicker.
  frame_memory = (frame_rate_ + (1 << 5)) >> 5;  // Unsigned ceiling. <Q0>
                                                 // frame_rate_ in Q4.
  if (frame_memory > kFrameHistory_size) {
    frame_memory = kFrameHistory_size;
  }

  // Get maximum and minimum.
  for (int32_t i = 0; i < kNumQuants; i++) {
    maxquant_uw8[i] = 0;
    minquant_uw8[i] = 255;
    for (uint32_t j = 0; j < frame_memory; j++) {
      if (quant_hist_uw8_[j][i] > maxquant_uw8[i]) {
        maxquant_uw8[i] = quant_hist_uw8_[j][i];
      }

      if (quant_hist_uw8_[j][i] < minquant_uw8[i]) {
        minquant_uw8[i] = quant_hist_uw8_[j][i];
      }
    }
  }

  // Get target quantiles.
  for (int32_t i = 0; i < kNumQuants - kMaxOnlyLength; i++) {
    // target = w * maxquant_uw8 + (1 - w) * minquant_uw8
    // Weights w = |weight_uw16_| are in Q15, hence the final output has to be
    // right shifted by 8 to end up in Q7.
    target_quant_uw16[i] = static_cast<uint16_t>((
        weight_uw16_[i] * maxquant_uw8[i] +
        ((1 << 15) - weight_uw16_[i]) * minquant_uw8[i]) >> 8);  // <Q7>
  }

  for (int32_t i = kNumQuants - kMaxOnlyLength; i < kNumQuants; i++) {
    target_quant_uw16[i] = ((uint16_t)maxquant_uw8[i]) << 7;
  }

  // Compute the map from input to output pixels.
  uint16_t mapUW16;  // <Q7>
  for (int32_t i = 1; i < kNumQuants; i++) {
    // As quant and targetQuant are limited to UWord8, it's safe to use Q7 here.
    tmp_uw32 = static_cast<uint32_t>(target_quant_uw16[i] -
        target_quant_uw16[i - 1]);
    tmp_uw16 = static_cast<uint16_t>(quant_uw8[i] - quant_uw8[i - 1]);  // <Q0>

    if (tmp_uw16 > 0) {
      increment_uw16 = static_cast<uint16_t>(WebRtcSpl_DivU32U16(tmp_uw32,
          tmp_uw16)); // <Q7>
    } else {
      // The value is irrelevant; the loop below will only iterate once.
      increment_uw16 = 0;
    }

    mapUW16 = target_quant_uw16[i - 1];
    for (uint32_t j = quant_uw8[i - 1]; j < (uint32_t)(quant_uw8[i] + 1); j++) {
      // Unsigned round. <Q0>
      map_uw8[j] = (uint8_t)((mapUW16 + (1 << 6)) >> 7);
      mapUW16 += increment_uw16;
    }
  }

  // Map to the output frame.
  uint8_t* buffer = frame->buffer(kYPlane);
  for (uint32_t i = 0; i < y_size; i++) {
    buffer[i] = map_uw8[buffer[i]];
  }

  // Frame was altered, so reset stats.
  VideoProcessingModule::ClearFrameStats(stats);

  return VPM_OK;
}

/**
   Performs some pre-detection operations. Must be called before
   DetectFlicker().

   \param[in] timestamp Timestamp of the current frame.
   \param[in] stats     Statistics of the current frame.

   \return 0: Success\n
           2: Detection not possible due to flickering frequency too close to
              zero.\n
          -1: Error
*/
int32_t VPMDeflickering::PreDetection(const uint32_t timestamp,
  const VideoProcessingModule::FrameStats& stats) {
  int32_t mean_val;  // Mean value of frame (Q4)
  uint32_t frame_rate = 0;
  int32_t meanBufferLength;  // Temp variable.

  mean_val = ((stats.sum << kmean_valueScaling) / stats.num_pixels);
  // Update mean value buffer.
  // This should be done even though we might end up in an unreliable detection.
  memmove(mean_buffer_ + 1, mean_buffer_,
      (kMeanBufferLength - 1) * sizeof(int32_t));
  mean_buffer_[0] = mean_val;

  // Update timestamp buffer.
  // This should be done even though we might end up in an unreliable detection.
  memmove(timestamp_buffer_ + 1, timestamp_buffer_, (kMeanBufferLength - 1) *
      sizeof(uint32_t));
  timestamp_buffer_[0] = timestamp;

/* Compute current frame rate (Q4) */
  if (timestamp_buffer_[kMeanBufferLength - 1] != 0) {
    frame_rate = ((90000 << 4) * (kMeanBufferLength - 1));
    frame_rate /=
        (timestamp_buffer_[0] - timestamp_buffer_[kMeanBufferLength - 1]);
  } else if (timestamp_buffer_[1] != 0) {
    frame_rate = (90000 << 4) / (timestamp_buffer_[0] - timestamp_buffer_[1]);
  }

  /* Determine required size of mean value buffer (mean_buffer_length_) */
  if (frame_rate == 0) {
    meanBufferLength = 1;
  } else {
    meanBufferLength =
        (kNumFlickerBeforeDetect * frame_rate) / kMinFrequencyToDetect;
  }
  /* Sanity check of buffer length */
  if (meanBufferLength >= kMeanBufferLength) {
    /* Too long buffer. The flickering frequency is too close to zero, which
     * makes the estimation unreliable.
     */
    mean_buffer_length_ = 0;
    return 2;
  }
  mean_buffer_length_ = meanBufferLength;

  if ((timestamp_buffer_[mean_buffer_length_ - 1] != 0) &&
      (mean_buffer_length_ != 1)) {
    frame_rate = ((90000 << 4) * (mean_buffer_length_ - 1));
    frame_rate /=
        (timestamp_buffer_[0] - timestamp_buffer_[mean_buffer_length_ - 1]);
  } else if (timestamp_buffer_[1] != 0) {
    frame_rate = (90000 << 4) / (timestamp_buffer_[0] - timestamp_buffer_[1]);
  }
  frame_rate_ = frame_rate;

  return VPM_OK;
}

/**
   This function detects flicker in the video stream. As a side effect the
   mean value buffer is updated with the new mean value.

   \return 0: No flickering detected\n
           1: Flickering detected\n
           2: Detection not possible due to unreliable frequency interval
          -1: Error
*/
int32_t VPMDeflickering::DetectFlicker() {
  uint32_t  i;
  int32_t  freqEst;       // (Q4) Frequency estimate to base detection upon
  int32_t  ret_val = -1;

  /* Sanity check for mean_buffer_length_ */
  if (mean_buffer_length_ < 2) {
    /* Not possible to estimate frequency */
    return(2);
  }
  // Count zero crossings with a dead zone to be robust against noise. If the
  // noise std is 2 pixel this corresponds to about 95% confidence interval.
  int32_t deadzone = (kZeroCrossingDeadzone << kmean_valueScaling);  // Q4
  int32_t meanOfBuffer = 0;  // Mean value of mean value buffer.
  int32_t numZeros     = 0;  // Number of zeros that cross the dead-zone.
  int32_t cntState     = 0;  // State variable for zero crossing regions.
  int32_t cntStateOld  = 0;  // Previous state for zero crossing regions.

  for (i = 0; i < mean_buffer_length_; i++) {
    meanOfBuffer += mean_buffer_[i];
  }
  meanOfBuffer += (mean_buffer_length_ >> 1);  // Rounding, not truncation.
  meanOfBuffer /= mean_buffer_length_;

  // Count zero crossings.
  cntStateOld = (mean_buffer_[0] >= (meanOfBuffer + deadzone));
  cntStateOld -= (mean_buffer_[0] <= (meanOfBuffer - deadzone));
  for (i = 1; i < mean_buffer_length_; i++) {
    cntState = (mean_buffer_[i] >= (meanOfBuffer + deadzone));
    cntState -= (mean_buffer_[i] <= (meanOfBuffer - deadzone));
    if (cntStateOld == 0) {
      cntStateOld = -cntState;
    }
    if (((cntState + cntStateOld) == 0) && (cntState != 0)) {
      numZeros++;
      cntStateOld = cntState;
    }
  }
  // END count zero crossings.

  /* Frequency estimation according to:
  * freqEst = numZeros * frame_rate / 2 / mean_buffer_length_;
  *
  * Resolution is set to Q4
  */
  freqEst = ((numZeros * 90000) << 3);
  freqEst /=
      (timestamp_buffer_[0] - timestamp_buffer_[mean_buffer_length_ - 1]);

  /* Translate frequency estimate to regions close to 100 and 120 Hz */
  uint8_t freqState = 0;  // Current translation state;
                          // (0) Not in interval,
                          // (1) Within valid interval,
                          // (2) Out of range
  int32_t freqAlias = freqEst;
  if (freqEst > kMinFrequencyToDetect) {
    uint8_t aliasState = 1;
    while(freqState == 0) {
      /* Increase frequency */
      freqAlias += (aliasState * frame_rate_);
      freqAlias += ((freqEst << 1) * (1 - (aliasState << 1)));
      /* Compute state */
      freqState = (abs(freqAlias - (100 << 4)) <= kFrequencyDeviation);
      freqState += (abs(freqAlias - (120 << 4)) <= kFrequencyDeviation);
      freqState += 2 * (freqAlias > ((120 << 4) + kFrequencyDeviation));
      /* Switch alias state */
      aliasState++;
      aliasState &= 0x01;
    }
  }
  /* Is frequency estimate within detection region? */
  if (freqState == 1) {
    ret_val = 1;
  } else if (freqState == 0) {
    ret_val = 2;
  } else {
    ret_val = 0;
  }
  return ret_val;
}

}  // namespace webrtc
