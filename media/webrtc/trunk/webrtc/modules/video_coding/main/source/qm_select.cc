/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/main/source/qm_select.h"

#include <math.h>

#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/modules/video_coding/main/interface/video_coding_defines.h"
#include "webrtc/modules/video_coding/main/source/internal_defines.h"
#include "webrtc/modules/video_coding/main/source/qm_select_data.h"
#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {

// QM-METHOD class

VCMQmMethod::VCMQmMethod()
    : content_metrics_(NULL),
      width_(0),
      height_(0),
      user_frame_rate_(0.0f),
      native_width_(0),
      native_height_(0),
      native_frame_rate_(0.0f),
      image_type_(kVGA),
      framerate_level_(kFrameRateHigh),
      init_(false) {
  ResetQM();
}

VCMQmMethod::~VCMQmMethod() {
}

void VCMQmMethod::ResetQM() {
  aspect_ratio_ = 1.0f;
  motion_.Reset();
  spatial_.Reset();
  content_class_ = 0;
}

uint8_t VCMQmMethod::ComputeContentClass() {
  ComputeMotionNFD();
  ComputeSpatial();
  return content_class_ = 3 * motion_.level + spatial_.level;
}

void VCMQmMethod::UpdateContent(const VideoContentMetrics*  contentMetrics) {
  content_metrics_ = contentMetrics;
}

void VCMQmMethod::ComputeMotionNFD() {
  if (content_metrics_) {
    motion_.value = content_metrics_->motion_magnitude;
  }
  // Determine motion level.
  if (motion_.value < kLowMotionNfd) {
    motion_.level = kLow;
  } else if (motion_.value > kHighMotionNfd) {
    motion_.level  = kHigh;
  } else {
    motion_.level = kDefault;
  }
}

void VCMQmMethod::ComputeSpatial() {
  float spatial_err = 0.0;
  float spatial_err_h = 0.0;
  float spatial_err_v = 0.0;
  if (content_metrics_) {
    spatial_err =  content_metrics_->spatial_pred_err;
    spatial_err_h = content_metrics_->spatial_pred_err_h;
    spatial_err_v = content_metrics_->spatial_pred_err_v;
  }
  // Spatial measure: take average of 3 prediction errors.
  spatial_.value = (spatial_err + spatial_err_h + spatial_err_v) / 3.0f;

  // Reduce thresholds for large scenes/higher pixel correlation.
  float scale2 = image_type_ > kVGA ? kScaleTexture : 1.0;

  if (spatial_.value > scale2 * kHighTexture) {
    spatial_.level = kHigh;
  } else if (spatial_.value < scale2 * kLowTexture) {
    spatial_.level = kLow;
  } else {
    spatial_.level = kDefault;
  }
}

ImageType VCMQmMethod::GetImageType(uint16_t width,
                                    uint16_t height) {
  // Get the image type for the encoder frame size.
  uint32_t image_size = width * height;
  if (image_size == kSizeOfImageType[kQCIF]) {
    return kQCIF;
  } else if (image_size == kSizeOfImageType[kHCIF]) {
    return kHCIF;
  } else if (image_size == kSizeOfImageType[kQVGA]) {
    return kQVGA;
  } else if (image_size == kSizeOfImageType[kCIF]) {
    return kCIF;
  } else if (image_size == kSizeOfImageType[kHVGA]) {
    return kHVGA;
  } else if (image_size == kSizeOfImageType[kVGA]) {
    return kVGA;
  } else if (image_size == kSizeOfImageType[kQFULLHD]) {
    return kQFULLHD;
  } else if (image_size == kSizeOfImageType[kWHD]) {
    return kWHD;
  } else if (image_size == kSizeOfImageType[kFULLHD]) {
    return kFULLHD;
  } else {
    // No exact match, find closet one.
    return FindClosestImageType(width, height);
  }
}

ImageType VCMQmMethod::FindClosestImageType(uint16_t width, uint16_t height) {
  float size = static_cast<float>(width * height);
  float min = size;
  int isel = 0;
  for (int i = 0; i < kNumImageTypes; ++i) {
    float dist = fabs(size - kSizeOfImageType[i]);
    if (dist < min) {
      min = dist;
      isel = i;
    }
  }
  return static_cast<ImageType>(isel);
}

FrameRateLevelClass VCMQmMethod::FrameRateLevel(float avg_framerate) {
  if (avg_framerate <= kLowFrameRate) {
    return kFrameRateLow;
  } else if (avg_framerate <= kMiddleFrameRate) {
    return kFrameRateMiddle1;
  } else if (avg_framerate <= kHighFrameRate) {
     return kFrameRateMiddle2;
  } else {
    return kFrameRateHigh;
  }
}

// RESOLUTION CLASS

VCMQmResolution::VCMQmResolution()
    :  qm_(new VCMResolutionScale()) {
  Reset();
}

VCMQmResolution::~VCMQmResolution() {
  delete qm_;
}

void VCMQmResolution::ResetRates() {
  sum_target_rate_ = 0.0f;
  sum_incoming_framerate_ = 0.0f;
  sum_rate_MM_ = 0.0f;
  sum_rate_MM_sgn_ = 0.0f;
  sum_packet_loss_ = 0.0f;
  buffer_level_ = kInitBufferLevel * target_bitrate_;
  frame_cnt_ = 0;
  frame_cnt_delta_ = 0;
  low_buffer_cnt_ = 0;
  update_rate_cnt_ = 0;
}

void VCMQmResolution::ResetDownSamplingState() {
  state_dec_factor_spatial_ = 1.0;
  state_dec_factor_temporal_  = 1.0;
  for (int i = 0; i < kDownActionHistorySize; i++) {
    down_action_history_[i].spatial = kNoChangeSpatial;
    down_action_history_[i].temporal = kNoChangeTemporal;
  }
}

void VCMQmResolution::Reset() {
  target_bitrate_ = 0.0f;
  incoming_framerate_ = 0.0f;
  buffer_level_ = 0.0f;
  per_frame_bandwidth_ = 0.0f;
  avg_target_rate_ = 0.0f;
  avg_incoming_framerate_ = 0.0f;
  avg_ratio_buffer_low_ = 0.0f;
  avg_rate_mismatch_ = 0.0f;
  avg_rate_mismatch_sgn_ = 0.0f;
  avg_packet_loss_ = 0.0f;
  encoder_state_ = kStableEncoding;
  num_layers_ = 1;
  ResetRates();
  ResetDownSamplingState();
  ResetQM();
}

EncoderState VCMQmResolution::GetEncoderState() {
  return encoder_state_;
}

// Initialize state after re-initializing the encoder,
// i.e., after SetEncodingData() in mediaOpt.
int VCMQmResolution::Initialize(float bitrate,
                                float user_framerate,
                                uint16_t width,
                                uint16_t height,
                                int num_layers) {
  if (user_framerate == 0.0f || width == 0 || height == 0) {
    return VCM_PARAMETER_ERROR;
  }
  Reset();
  target_bitrate_ = bitrate;
  incoming_framerate_ = user_framerate;
  UpdateCodecParameters(user_framerate, width, height);
  native_width_ = width;
  native_height_ = height;
  native_frame_rate_ = user_framerate;
  num_layers_ = num_layers;
  // Initial buffer level.
  buffer_level_ = kInitBufferLevel * target_bitrate_;
  // Per-frame bandwidth.
  per_frame_bandwidth_ = target_bitrate_ / user_framerate;
  init_  = true;
  return VCM_OK;
}

void VCMQmResolution::UpdateCodecParameters(float frame_rate, uint16_t width,
                                            uint16_t height) {
  width_ = width;
  height_ = height;
  // |user_frame_rate| is the target frame rate for VPM frame dropper.
  user_frame_rate_ = frame_rate;
  image_type_ = GetImageType(width, height);
}

// Update rate data after every encoded frame.
void VCMQmResolution::UpdateEncodedSize(int encoded_size,
                                        FrameType encoded_frame_type) {
  frame_cnt_++;
  // Convert to Kbps.
  float encoded_size_kbits = static_cast<float>((encoded_size * 8.0) / 1000.0);

  // Update the buffer level:
  // Note this is not the actual encoder buffer level.
  // |buffer_level_| is reset to an initial value after SelectResolution is
  // called, and does not account for frame dropping by encoder or VCM.
  buffer_level_ += per_frame_bandwidth_ - encoded_size_kbits;

  // Counter for occurrences of low buffer level:
  // low/negative values means encoder is likely dropping frames.
  if (buffer_level_ <= kPercBufferThr * kInitBufferLevel * target_bitrate_) {
    low_buffer_cnt_++;
  }
}

// Update various quantities after SetTargetRates in MediaOpt.
void VCMQmResolution::UpdateRates(float target_bitrate,
                                  float encoder_sent_rate,
                                  float incoming_framerate,
                                  uint8_t packet_loss) {
  // Sum the target bitrate: this is the encoder rate from previous update
  // (~1sec), i.e, before the update for next ~1sec.
  sum_target_rate_ += target_bitrate_;
  update_rate_cnt_++;

  // Sum the received (from RTCP reports) packet loss rates.
  sum_packet_loss_ += static_cast<float>(packet_loss / 255.0);

  // Sum the sequence rate mismatch:
  // Mismatch here is based on the difference between the target rate
  // used (in previous ~1sec) and the average actual encoding rate measured
  // at previous ~1sec.
  float diff = target_bitrate_ - encoder_sent_rate;
  if (target_bitrate_ > 0.0)
    sum_rate_MM_ += fabs(diff) / target_bitrate_;
  int sgnDiff = diff > 0 ? 1 : (diff < 0 ? -1 : 0);
  // To check for consistent under(+)/over_shooting(-) of target rate.
  sum_rate_MM_sgn_ += sgnDiff;

  // Update with the current new target and frame rate:
  // these values are ones the encoder will use for the current/next ~1sec.
  target_bitrate_ =  target_bitrate;
  incoming_framerate_ = incoming_framerate;
  sum_incoming_framerate_ += incoming_framerate_;
  // Update the per_frame_bandwidth:
  // this is the per_frame_bw for the current/next ~1sec.
  per_frame_bandwidth_  = 0.0f;
  if (incoming_framerate_ > 0.0f) {
    per_frame_bandwidth_ = target_bitrate_ / incoming_framerate_;
  }
}

// Select the resolution factors: frame size and frame rate change (qm scales).
// Selection is for going down in resolution, or for going back up
// (if a previous down-sampling action was taken).

// In the current version the following constraints are imposed:
// 1) We only allow for one action, either down or up, at a given time.
// 2) The possible down-sampling actions are: spatial by 1/2x1/2, 3/4x3/4;
//    temporal/frame rate reduction by 1/2 and 2/3.
// 3) The action for going back up is the reverse of last (spatial or temporal)
//    down-sampling action. The list of down-sampling actions from the
//    Initialize() state are kept in |down_action_history_|.
// 4) The total amount of down-sampling (spatial and/or temporal) from the
//    Initialize() state (native resolution) is limited by various factors.
int VCMQmResolution::SelectResolution(VCMResolutionScale** qm) {
  if (!init_) {
    return VCM_UNINITIALIZED;
  }
  if (content_metrics_ == NULL) {
    Reset();
    *qm =  qm_;
    return VCM_OK;
  }

  // Check conditions on down-sampling state.
  assert(state_dec_factor_spatial_ >= 1.0f);
  assert(state_dec_factor_temporal_ >= 1.0f);
  assert(state_dec_factor_spatial_ <= kMaxSpatialDown);
  assert(state_dec_factor_temporal_ <= kMaxTempDown);
  assert(state_dec_factor_temporal_ * state_dec_factor_spatial_ <=
         kMaxTotalDown);

  // Compute content class for selection.
  content_class_ = ComputeContentClass();
  // Compute various rate quantities for selection.
  ComputeRatesForSelection();

  // Get the encoder state.
  ComputeEncoderState();

  // Default settings: no action.
  SetDefaultAction();
  *qm = qm_;

  // Check for going back up in resolution, if we have had some down-sampling
  // relative to native state in Initialize().
  if (down_action_history_[0].spatial != kNoChangeSpatial ||
      down_action_history_[0].temporal != kNoChangeTemporal) {
    if (GoingUpResolution()) {
      *qm = qm_;
      return VCM_OK;
    }
  }

  // Check for going down in resolution.
  if (GoingDownResolution()) {
    *qm = qm_;
    return VCM_OK;
  }
  return VCM_OK;
}

void VCMQmResolution::SetDefaultAction() {
  qm_->codec_width = width_;
  qm_->codec_height = height_;
  qm_->frame_rate = user_frame_rate_;
  qm_->change_resolution_spatial = false;
  qm_->change_resolution_temporal = false;
  qm_->spatial_width_fact = 1.0f;
  qm_->spatial_height_fact = 1.0f;
  qm_->temporal_fact = 1.0f;
  action_.spatial = kNoChangeSpatial;
  action_.temporal = kNoChangeTemporal;
}

void VCMQmResolution::ComputeRatesForSelection() {
  avg_target_rate_ = 0.0f;
  avg_incoming_framerate_ = 0.0f;
  avg_ratio_buffer_low_ = 0.0f;
  avg_rate_mismatch_ = 0.0f;
  avg_rate_mismatch_sgn_ = 0.0f;
  avg_packet_loss_ = 0.0f;
  if (frame_cnt_ > 0) {
    avg_ratio_buffer_low_ = static_cast<float>(low_buffer_cnt_) /
        static_cast<float>(frame_cnt_);
  }
  if (update_rate_cnt_ > 0) {
    avg_rate_mismatch_ = static_cast<float>(sum_rate_MM_) /
        static_cast<float>(update_rate_cnt_);
    avg_rate_mismatch_sgn_ = static_cast<float>(sum_rate_MM_sgn_) /
        static_cast<float>(update_rate_cnt_);
    avg_target_rate_ = static_cast<float>(sum_target_rate_) /
        static_cast<float>(update_rate_cnt_);
    avg_incoming_framerate_ = static_cast<float>(sum_incoming_framerate_) /
        static_cast<float>(update_rate_cnt_);
    avg_packet_loss_ =  static_cast<float>(sum_packet_loss_) /
        static_cast<float>(update_rate_cnt_);
  }
  // For selection we may want to weight some quantities more heavily
  // with the current (i.e., next ~1sec) rate values.
  avg_target_rate_ = kWeightRate * avg_target_rate_ +
      (1.0 - kWeightRate) * target_bitrate_;
  avg_incoming_framerate_ = kWeightRate * avg_incoming_framerate_ +
      (1.0 - kWeightRate) * incoming_framerate_;
  // Use base layer frame rate for temporal layers: this will favor spatial.
  assert(num_layers_ > 0);
  framerate_level_ = FrameRateLevel(
      avg_incoming_framerate_ / static_cast<float>(1 << (num_layers_ - 1)));
}

void VCMQmResolution::ComputeEncoderState() {
  // Default.
  encoder_state_ = kStableEncoding;

  // Assign stressed state if:
  // 1) occurrences of low buffer levels is high, or
  // 2) rate mis-match is high, and consistent over-shooting by encoder.
  if ((avg_ratio_buffer_low_ > kMaxBufferLow) ||
      ((avg_rate_mismatch_ > kMaxRateMisMatch) &&
          (avg_rate_mismatch_sgn_ < -kRateOverShoot))) {
    encoder_state_ = kStressedEncoding;
  }
  // Assign easy state if:
  // 1) rate mis-match is high, and
  // 2) consistent under-shooting by encoder.
  if ((avg_rate_mismatch_ > kMaxRateMisMatch) &&
      (avg_rate_mismatch_sgn_ > kRateUnderShoot)) {
    encoder_state_ = kEasyEncoding;
  }
}

bool VCMQmResolution::GoingUpResolution() {
  // For going up, we check for undoing the previous down-sampling action.

  float fac_width = kFactorWidthSpatial[down_action_history_[0].spatial];
  float fac_height = kFactorHeightSpatial[down_action_history_[0].spatial];
  float fac_temp = kFactorTemporal[down_action_history_[0].temporal];
  // For going up spatially, we allow for going up by 3/4x3/4 at each stage.
  // So if the last spatial action was 1/2x1/2 it would be undone in 2 stages.
  // Modify the fac_width/height for this case.
  if (down_action_history_[0].spatial == kOneQuarterSpatialUniform) {
    fac_width = kFactorWidthSpatial[kOneQuarterSpatialUniform] /
        kFactorWidthSpatial[kOneHalfSpatialUniform];
    fac_height = kFactorHeightSpatial[kOneQuarterSpatialUniform] /
        kFactorHeightSpatial[kOneHalfSpatialUniform];
  }

  // Check if we should go up both spatially and temporally.
  if (down_action_history_[0].spatial != kNoChangeSpatial &&
      down_action_history_[0].temporal != kNoChangeTemporal) {
    if (ConditionForGoingUp(fac_width, fac_height, fac_temp,
                            kTransRateScaleUpSpatialTemp)) {
      action_.spatial = down_action_history_[0].spatial;
      action_.temporal = down_action_history_[0].temporal;
      UpdateDownsamplingState(kUpResolution);
      return true;
    }
  }
  // Check if we should go up either spatially or temporally.
  bool selected_up_spatial = false;
  bool selected_up_temporal = false;
  if (down_action_history_[0].spatial != kNoChangeSpatial) {
    selected_up_spatial = ConditionForGoingUp(fac_width, fac_height, 1.0f,
                                              kTransRateScaleUpSpatial);
  }
  if (down_action_history_[0].temporal != kNoChangeTemporal) {
    selected_up_temporal = ConditionForGoingUp(1.0f, 1.0f, fac_temp,
                                               kTransRateScaleUpTemp);
  }
  if (selected_up_spatial && !selected_up_temporal) {
    action_.spatial = down_action_history_[0].spatial;
    action_.temporal = kNoChangeTemporal;
    UpdateDownsamplingState(kUpResolution);
    return true;
  } else if (!selected_up_spatial && selected_up_temporal) {
    action_.spatial = kNoChangeSpatial;
    action_.temporal = down_action_history_[0].temporal;
    UpdateDownsamplingState(kUpResolution);
    return true;
  } else if (selected_up_spatial && selected_up_temporal) {
    PickSpatialOrTemporal();
    UpdateDownsamplingState(kUpResolution);
    return true;
  }
  return false;
}

bool VCMQmResolution::ConditionForGoingUp(float fac_width,
                                          float fac_height,
                                          float fac_temp,
                                          float scale_fac) {
  float estimated_transition_rate_up = GetTransitionRate(fac_width, fac_height,
                                                         fac_temp, scale_fac);
  // Go back up if:
  // 1) target rate is above threshold and current encoder state is stable, or
  // 2) encoder state is easy (encoder is significantly under-shooting target).
  if (((avg_target_rate_ > estimated_transition_rate_up) &&
      (encoder_state_ == kStableEncoding)) ||
      (encoder_state_ == kEasyEncoding)) {
    return true;
  } else {
    return false;
  }
}

bool VCMQmResolution::GoingDownResolution() {
  float estimated_transition_rate_down =
      GetTransitionRate(1.0f, 1.0f, 1.0f, 1.0f);
  float max_rate = kFrameRateFac[framerate_level_] * kMaxRateQm[image_type_];
  // Resolution reduction if:
  // (1) target rate is below transition rate, or
  // (2) encoder is in stressed state and target rate below a max threshold.
  if ((avg_target_rate_ < estimated_transition_rate_down ) ||
      (encoder_state_ == kStressedEncoding && avg_target_rate_ < max_rate)) {
    // Get the down-sampling action: based on content class, and how low
    // average target rate is relative to transition rate.
    uint8_t spatial_fact =
        kSpatialAction[content_class_ +
                       9 * RateClass(estimated_transition_rate_down)];
    uint8_t temp_fact =
        kTemporalAction[content_class_ +
                        9 * RateClass(estimated_transition_rate_down)];

    switch (spatial_fact) {
      case 4: {
        action_.spatial = kOneQuarterSpatialUniform;
        break;
      }
      case 2: {
        action_.spatial = kOneHalfSpatialUniform;
        break;
      }
      case 1: {
        action_.spatial = kNoChangeSpatial;
        break;
      }
      default: {
        assert(false);
      }
    }
    switch (temp_fact) {
      case 3: {
        action_.temporal = kTwoThirdsTemporal;
        break;
      }
      case 2: {
        action_.temporal = kOneHalfTemporal;
        break;
      }
      case 1: {
        action_.temporal = kNoChangeTemporal;
        break;
      }
      default: {
        assert(false);
      }
    }
    // Only allow for one action (spatial or temporal) at a given time.
    assert(action_.temporal == kNoChangeTemporal ||
           action_.spatial == kNoChangeSpatial);

    // Adjust cases not captured in tables, mainly based on frame rate, and
    // also check for odd frame sizes.
    AdjustAction();

    // Update down-sampling state.
    if (action_.spatial != kNoChangeSpatial ||
        action_.temporal != kNoChangeTemporal) {
      UpdateDownsamplingState(kDownResolution);
      return true;
    }
  }
  return false;
}

float VCMQmResolution::GetTransitionRate(float fac_width,
                                         float fac_height,
                                         float fac_temp,
                                         float scale_fac) {
  ImageType image_type = GetImageType(
      static_cast<uint16_t>(fac_width * width_),
      static_cast<uint16_t>(fac_height * height_));

  FrameRateLevelClass framerate_level =
      FrameRateLevel(fac_temp * avg_incoming_framerate_);
  // If we are checking for going up temporally, and this is the last
  // temporal action, then use native frame rate.
  if (down_action_history_[1].temporal == kNoChangeTemporal &&
      fac_temp > 1.0f) {
    framerate_level = FrameRateLevel(native_frame_rate_);
  }

  // The maximum allowed rate below which down-sampling is allowed:
  // Nominal values based on image format (frame size and frame rate).
  float max_rate = kFrameRateFac[framerate_level] * kMaxRateQm[image_type];

  uint8_t image_class = image_type > kVGA ? 1: 0;
  uint8_t table_index = image_class * 9 + content_class_;
  // Scale factor for down-sampling transition threshold:
  // factor based on the content class and the image size.
  float scaleTransRate = kScaleTransRateQm[table_index];
  // Threshold bitrate for resolution action.
  return static_cast<float> (scale_fac * scaleTransRate * max_rate);
}

void VCMQmResolution::UpdateDownsamplingState(UpDownAction up_down) {
  if (up_down == kUpResolution) {
    qm_->spatial_width_fact = 1.0f / kFactorWidthSpatial[action_.spatial];
    qm_->spatial_height_fact = 1.0f / kFactorHeightSpatial[action_.spatial];
    // If last spatial action was 1/2x1/2, we undo it in two steps, so the
    // spatial scale factor in this first step is modified as (4.0/3.0 / 2.0).
    if (action_.spatial == kOneQuarterSpatialUniform) {
      qm_->spatial_width_fact =
          1.0f * kFactorWidthSpatial[kOneHalfSpatialUniform] /
          kFactorWidthSpatial[kOneQuarterSpatialUniform];
      qm_->spatial_height_fact =
          1.0f * kFactorHeightSpatial[kOneHalfSpatialUniform] /
          kFactorHeightSpatial[kOneQuarterSpatialUniform];
    }
    qm_->temporal_fact = 1.0f / kFactorTemporal[action_.temporal];
    RemoveLastDownAction();
  } else if (up_down == kDownResolution) {
    ConstrainAmountOfDownSampling();
    ConvertSpatialFractionalToWhole();
    qm_->spatial_width_fact = kFactorWidthSpatial[action_.spatial];
    qm_->spatial_height_fact = kFactorHeightSpatial[action_.spatial];
    qm_->temporal_fact = kFactorTemporal[action_.temporal];
    InsertLatestDownAction();
  } else {
    // This function should only be called if either the Up or Down action
    // has been selected.
    assert(false);
  }
  UpdateCodecResolution();
  state_dec_factor_spatial_ = state_dec_factor_spatial_ *
      qm_->spatial_width_fact * qm_->spatial_height_fact;
  state_dec_factor_temporal_ = state_dec_factor_temporal_ * qm_->temporal_fact;
}

void  VCMQmResolution::UpdateCodecResolution() {
  if (action_.spatial != kNoChangeSpatial) {
    qm_->change_resolution_spatial = true;
    qm_->codec_width = static_cast<uint16_t>(width_ /
                                             qm_->spatial_width_fact + 0.5f);
    qm_->codec_height = static_cast<uint16_t>(height_ /
                                              qm_->spatial_height_fact + 0.5f);
    // Size should not exceed native sizes.
    assert(qm_->codec_width <= native_width_);
    assert(qm_->codec_height <= native_height_);
    // New sizes should be multiple of 2, otherwise spatial should not have
    // been selected.
    assert(qm_->codec_width % 2 == 0);
    assert(qm_->codec_height % 2 == 0);
  }
  if (action_.temporal != kNoChangeTemporal) {
    qm_->change_resolution_temporal = true;
    // Update the frame rate based on the average incoming frame rate.
    qm_->frame_rate = avg_incoming_framerate_ / qm_->temporal_fact + 0.5f;
    if (down_action_history_[0].temporal == 0) {
      // When we undo the last temporal-down action, make sure we go back up
      // to the native frame rate. Since the incoming frame rate may
      // fluctuate over time, |avg_incoming_framerate_| scaled back up may
      // be smaller than |native_frame rate_|.
      qm_->frame_rate = native_frame_rate_;
    }
  }
}

uint8_t VCMQmResolution::RateClass(float transition_rate) {
  return avg_target_rate_ < (kFacLowRate * transition_rate) ? 0:
  (avg_target_rate_ >= transition_rate ? 2 : 1);
}

// TODO(marpan): Would be better to capture these frame rate adjustments by
// extending the table data (qm_select_data.h).
void VCMQmResolution::AdjustAction() {
  // If the spatial level is default state (neither low or high), motion level
  // is not high, and spatial action was selected, switch to 2/3 frame rate
  // reduction if the average incoming frame rate is high.
  if (spatial_.level == kDefault && motion_.level != kHigh &&
      action_.spatial != kNoChangeSpatial &&
      framerate_level_ == kFrameRateHigh) {
    action_.spatial = kNoChangeSpatial;
    action_.temporal = kTwoThirdsTemporal;
  }
  // If both motion and spatial level are low, and temporal down action was
  // selected, switch to spatial 3/4x3/4 if the frame rate is not above the
  // lower middle level (|kFrameRateMiddle1|).
  if (motion_.level == kLow && spatial_.level == kLow &&
      framerate_level_ <= kFrameRateMiddle1 &&
      action_.temporal != kNoChangeTemporal) {
    action_.spatial = kOneHalfSpatialUniform;
    action_.temporal = kNoChangeTemporal;
  }
  // If spatial action is selected, and there has been too much spatial
  // reduction already (i.e., 1/4), then switch to temporal action if the
  // average frame rate is not low.
  if (action_.spatial != kNoChangeSpatial &&
      down_action_history_[0].spatial == kOneQuarterSpatialUniform &&
      framerate_level_ != kFrameRateLow) {
    action_.spatial = kNoChangeSpatial;
    action_.temporal = kTwoThirdsTemporal;
  }
  // Never use temporal action if number of temporal layers is above 2.
  if (num_layers_ > 2) {
    if (action_.temporal !=  kNoChangeTemporal) {
      action_.spatial = kOneHalfSpatialUniform;
    }
    action_.temporal = kNoChangeTemporal;
  }
  // If spatial action was selected, we need to make sure the frame sizes
  // are multiples of two. Otherwise switch to 2/3 temporal.
  if (action_.spatial != kNoChangeSpatial &&
      !EvenFrameSize()) {
    action_.spatial = kNoChangeSpatial;
    // Only one action (spatial or temporal) is allowed at a given time, so need
    // to check whether temporal action is currently selected.
    action_.temporal = kTwoThirdsTemporal;
  }
}

void VCMQmResolution::ConvertSpatialFractionalToWhole() {
  // If 3/4 spatial is selected, check if there has been another 3/4,
  // and if so, combine them into 1/2. 1/2 scaling is more efficient than 9/16.
  // Note we define 3/4x3/4 spatial as kOneHalfSpatialUniform.
  if (action_.spatial == kOneHalfSpatialUniform) {
    bool found = false;
    int isel = kDownActionHistorySize;
    for (int i = 0; i < kDownActionHistorySize; ++i) {
      if (down_action_history_[i].spatial ==  kOneHalfSpatialUniform) {
        isel = i;
        found = true;
        break;
      }
    }
    if (found) {
       action_.spatial = kOneQuarterSpatialUniform;
       state_dec_factor_spatial_ = state_dec_factor_spatial_ /
           (kFactorWidthSpatial[kOneHalfSpatialUniform] *
            kFactorHeightSpatial[kOneHalfSpatialUniform]);
       // Check if switching to 1/2x1/2 (=1/4) spatial is allowed.
       ConstrainAmountOfDownSampling();
       if (action_.spatial == kNoChangeSpatial) {
         // Not allowed. Go back to 3/4x3/4 spatial.
         action_.spatial = kOneHalfSpatialUniform;
         state_dec_factor_spatial_ = state_dec_factor_spatial_ *
             kFactorWidthSpatial[kOneHalfSpatialUniform] *
             kFactorHeightSpatial[kOneHalfSpatialUniform];
       } else {
         // Switching is allowed. Remove 3/4x3/4 from the history, and update
         // the frame size.
         for (int i = isel; i < kDownActionHistorySize - 1; ++i) {
           down_action_history_[i].spatial =
               down_action_history_[i + 1].spatial;
         }
         width_ = width_ * kFactorWidthSpatial[kOneHalfSpatialUniform];
         height_ = height_ * kFactorHeightSpatial[kOneHalfSpatialUniform];
       }
    }
  }
}

// Returns false if the new frame sizes, under the current spatial action,
// are not multiples of two.
bool VCMQmResolution::EvenFrameSize() {
  if (action_.spatial == kOneHalfSpatialUniform) {
    if ((width_ * 3 / 4) % 2 != 0 || (height_ * 3 / 4) % 2 != 0) {
      return false;
    }
  } else if (action_.spatial == kOneQuarterSpatialUniform) {
    if ((width_ * 1 / 2) % 2 != 0 || (height_ * 1 / 2) % 2 != 0) {
      return false;
    }
  }
  return true;
}

void VCMQmResolution::InsertLatestDownAction() {
  if (action_.spatial != kNoChangeSpatial) {
    for (int i = kDownActionHistorySize - 1; i > 0; --i) {
      down_action_history_[i].spatial = down_action_history_[i - 1].spatial;
    }
    down_action_history_[0].spatial = action_.spatial;
  }
  if (action_.temporal != kNoChangeTemporal) {
    for (int i = kDownActionHistorySize - 1; i > 0; --i) {
      down_action_history_[i].temporal = down_action_history_[i - 1].temporal;
    }
    down_action_history_[0].temporal = action_.temporal;
  }
}

void VCMQmResolution::RemoveLastDownAction() {
  if (action_.spatial != kNoChangeSpatial) {
    // If the last spatial action was 1/2x1/2 we replace it with 3/4x3/4.
    if (action_.spatial == kOneQuarterSpatialUniform) {
      down_action_history_[0].spatial = kOneHalfSpatialUniform;
    } else {
      for (int i = 0; i < kDownActionHistorySize - 1; ++i) {
        down_action_history_[i].spatial = down_action_history_[i + 1].spatial;
      }
      down_action_history_[kDownActionHistorySize - 1].spatial =
          kNoChangeSpatial;
    }
  }
  if (action_.temporal != kNoChangeTemporal) {
    for (int i = 0; i < kDownActionHistorySize - 1; ++i) {
      down_action_history_[i].temporal = down_action_history_[i + 1].temporal;
    }
    down_action_history_[kDownActionHistorySize - 1].temporal =
        kNoChangeTemporal;
  }
}

void VCMQmResolution::ConstrainAmountOfDownSampling() {
  // Sanity checks on down-sampling selection:
  // override the settings for too small image size and/or frame rate.
  // Also check the limit on current down-sampling states.

  float spatial_width_fact = kFactorWidthSpatial[action_.spatial];
  float spatial_height_fact = kFactorHeightSpatial[action_.spatial];
  float temporal_fact = kFactorTemporal[action_.temporal];
  float new_dec_factor_spatial = state_dec_factor_spatial_ *
      spatial_width_fact * spatial_height_fact;
  float new_dec_factor_temp = state_dec_factor_temporal_ * temporal_fact;

  // No spatial sampling if current frame size is too small, or if the
  // amount of spatial down-sampling is above maximum spatial down-action.
  if ((width_ * height_) <= kMinImageSize ||
      new_dec_factor_spatial > kMaxSpatialDown) {
    action_.spatial = kNoChangeSpatial;
    new_dec_factor_spatial = state_dec_factor_spatial_;
  }
  // No frame rate reduction if average frame rate is below some point, or if
  // the amount of temporal down-sampling is above maximum temporal down-action.
  if (avg_incoming_framerate_ <= kMinFrameRate ||
      new_dec_factor_temp > kMaxTempDown) {
    action_.temporal = kNoChangeTemporal;
    new_dec_factor_temp = state_dec_factor_temporal_;
  }
  // Check if the total (spatial-temporal) down-action is above maximum allowed,
  // if so, disallow the current selected down-action.
  if (new_dec_factor_spatial * new_dec_factor_temp > kMaxTotalDown) {
    if (action_.spatial != kNoChangeSpatial) {
      action_.spatial = kNoChangeSpatial;
    } else if (action_.temporal != kNoChangeTemporal) {
      action_.temporal = kNoChangeTemporal;
    } else {
      // We only allow for one action (spatial or temporal) at a given time, so
      // either spatial or temporal action is selected when this function is
      // called. If the selected action is disallowed from one of the above
      // 2 prior conditions (on spatial & temporal max down-action), then this
      // condition "total down-action > |kMaxTotalDown|" would not be entered.
      assert(false);
    }
  }
}

void VCMQmResolution::PickSpatialOrTemporal() {
  // Pick the one that has had the most down-sampling thus far.
  if (state_dec_factor_spatial_ > state_dec_factor_temporal_) {
    action_.spatial = down_action_history_[0].spatial;
    action_.temporal = kNoChangeTemporal;
  } else {
    action_.spatial = kNoChangeSpatial;
    action_.temporal = down_action_history_[0].temporal;
  }
}

// TODO(marpan): Update when we allow for directional spatial down-sampling.
void VCMQmResolution::SelectSpatialDirectionMode(float transition_rate) {
  // Default is 4/3x4/3
  // For bit rates well below transitional rate, we select 2x2.
  if (avg_target_rate_ < transition_rate * kRateRedSpatial2X2) {
    qm_->spatial_width_fact = 2.0f;
    qm_->spatial_height_fact = 2.0f;
  }
  // Otherwise check prediction errors and aspect ratio.
  float spatial_err = 0.0f;
  float spatial_err_h = 0.0f;
  float spatial_err_v = 0.0f;
  if (content_metrics_) {
    spatial_err = content_metrics_->spatial_pred_err;
    spatial_err_h = content_metrics_->spatial_pred_err_h;
    spatial_err_v = content_metrics_->spatial_pred_err_v;
  }

  // Favor 1x2 if aspect_ratio is 16:9.
  if (aspect_ratio_ >= 16.0f / 9.0f) {
    // Check if 1x2 has lowest prediction error.
    if (spatial_err_h < spatial_err && spatial_err_h < spatial_err_v) {
      qm_->spatial_width_fact = 2.0f;
      qm_->spatial_height_fact = 1.0f;
    }
  }
  // Check for 4/3x4/3 selection: favor 2x2 over 1x2 and 2x1.
  if (spatial_err < spatial_err_h * (1.0f + kSpatialErr2x2VsHoriz) &&
      spatial_err < spatial_err_v * (1.0f + kSpatialErr2X2VsVert)) {
    qm_->spatial_width_fact = 4.0f / 3.0f;
    qm_->spatial_height_fact = 4.0f / 3.0f;
  }
  // Check for 2x1 selection.
  if (spatial_err_v < spatial_err_h * (1.0f - kSpatialErrVertVsHoriz) &&
      spatial_err_v < spatial_err * (1.0f - kSpatialErr2X2VsVert)) {
    qm_->spatial_width_fact = 1.0f;
    qm_->spatial_height_fact = 2.0f;
  }
}

// ROBUSTNESS CLASS

VCMQmRobustness::VCMQmRobustness() {
  Reset();
}

VCMQmRobustness::~VCMQmRobustness() {
}

void VCMQmRobustness::Reset() {
  prev_total_rate_ = 0.0f;
  prev_rtt_time_ = 0;
  prev_packet_loss_ = 0;
  prev_code_rate_delta_ = 0;
  ResetQM();
}

// Adjust the FEC rate based on the content and the network state
// (packet loss rate, total rate/bandwidth, round trip time).
// Note that packetLoss here is the filtered loss value.
float VCMQmRobustness::AdjustFecFactor(uint8_t code_rate_delta,
                                       float total_rate,
                                       float framerate,
                                       uint32_t rtt_time,
                                       uint8_t packet_loss) {
  // Default: no adjustment
  float adjust_fec =  1.0f;
  if (content_metrics_ == NULL) {
    return adjust_fec;
  }
  // Compute class state of the content.
  ComputeMotionNFD();
  ComputeSpatial();

  // TODO(marpan): Set FEC adjustment factor.

  // Keep track of previous values of network state:
  // adjustment may be also based on pattern of changes in network state.
  prev_total_rate_ = total_rate;
  prev_rtt_time_ = rtt_time;
  prev_packet_loss_ = packet_loss;
  prev_code_rate_delta_ = code_rate_delta;
  return adjust_fec;
}

// Set the UEP (unequal-protection across packets) on/off for the FEC.
bool VCMQmRobustness::SetUepProtection(uint8_t code_rate_delta,
                                       float total_rate,
                                       uint8_t packet_loss,
                                       bool frame_type) {
  // Default.
  return false;
}
}  // namespace
