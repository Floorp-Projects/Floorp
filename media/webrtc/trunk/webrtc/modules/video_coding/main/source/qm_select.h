/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_QM_SELECT_H_
#define WEBRTC_MODULES_VIDEO_CODING_QM_SELECT_H_

#include "webrtc/common_types.h"
#include "webrtc/typedefs.h"

/******************************************************/
/* Quality Modes: Resolution and Robustness settings  */
/******************************************************/

namespace webrtc {
struct VideoContentMetrics;

struct VCMResolutionScale {
  VCMResolutionScale()
      : codec_width(640),
        codec_height(480),
        frame_rate(30.0f),
        spatial_width_fact(1.0f),
        spatial_height_fact(1.0f),
        temporal_fact(1.0f),
        change_resolution_spatial(false),
        change_resolution_temporal(false) {
  }
  uint16_t codec_width;
  uint16_t codec_height;
  float frame_rate;
  float spatial_width_fact;
  float spatial_height_fact;
  float temporal_fact;
  bool change_resolution_spatial;
  bool change_resolution_temporal;
};

// Other possibilities:
// aspect 1.333*
// kQQVGA = 160x120
// k???     192x144
// k???     256x192 (good step between 320x240 and 160x120)
enum ImageType {
  kQCIF = 0,            // 176x144
  kHCIF,                // 264x216 = half(~3/4x3/4) CIF.
  kQVGA,                // 320x240 = quarter VGA.
  kCIF,                 // 352x288
  kHVGA,                // 480x360 = half(~3/4x3/4) VGA.
  kVGA,                 // 640x480
  kQFULLHD,             // 960x540 = quarter FULLHD, and half(~3/4x3/4) WHD.
  kWHD,                 // 1280x720
  kFULLHD,              // 1920x1080
  kNumImageTypes
};

const uint32_t kSizeOfImageType[kNumImageTypes] =
{ 25344, 57024, 76800, 101376, 172800, 307200, 518400, 921600, 2073600 };

enum FrameRateLevelClass {
  kFrameRateLow,
  kFrameRateMiddle1,
  kFrameRateMiddle2,
  kFrameRateHigh
};

enum ContentLevelClass {
  kLow,
  kHigh,
  kDefault
};

struct VCMContFeature {
  VCMContFeature()
      : value(0.0f),
        level(kDefault) {
  }
  void Reset() {
    value = 0.0f;
    level = kDefault;
  }
  float value;
  ContentLevelClass level;
};

enum UpDownAction {
  kUpResolution,
  kDownResolution
};

enum SpatialAction {
  kNoChangeSpatial,
  kOneHalfSpatialUniform,        // 3/4 x 3/4: 9/6 ~1/2 pixel reduction.
  kOneQuarterSpatialUniform,     // 1/2 x 1/2: 1/4 pixel reduction.
  kNumModesSpatial
};

enum TemporalAction {
  kNoChangeTemporal,
  kTwoThirdsTemporal,     // 2/3 frame rate reduction
  kOneHalfTemporal,       // 1/2 frame rate reduction
  kNumModesTemporal
};

struct ResolutionAction {
  ResolutionAction()
      : spatial(kNoChangeSpatial),
        temporal(kNoChangeTemporal) {
  }
  SpatialAction spatial;
  TemporalAction temporal;
};

// Down-sampling factors for spatial (width and height), and temporal.
const float kFactorWidthSpatial[kNumModesSpatial] =
    { 1.0f, 4.0f / 3.0f, 2.0f };

const float kFactorHeightSpatial[kNumModesSpatial] =
    { 1.0f, 4.0f / 3.0f, 2.0f };

const float kFactorTemporal[kNumModesTemporal] =
    { 1.0f, 1.5f, 2.0f };

enum EncoderState {
  kStableEncoding,    // Low rate mis-match, stable buffer levels.
  kStressedEncoding,  // Significant over-shooting of target rate,
                      // Buffer under-flow, etc.
  kEasyEncoding       // Significant under-shooting of target rate.
};

// QmMethod class: main class for resolution and robustness settings

class VCMQmMethod {
 public:
  VCMQmMethod();
  virtual ~VCMQmMethod();

  // Reset values
  void ResetQM();
  virtual void Reset() = 0;

  // Compute content class.
  uint8_t ComputeContentClass();

  // Update with the content metrics.
  void UpdateContent(const VideoContentMetrics* content_metrics);

  // Compute spatial texture magnitude and level.
  // Spatial texture is a spatial prediction error measure.
  void ComputeSpatial();

  // Compute motion magnitude and level for NFD metric.
  // NFD is normalized frame difference (normalized by spatial variance).
  void ComputeMotionNFD();

  // Get the imageType (CIF, VGA, HD, etc) for the system width/height.
  ImageType GetImageType(uint16_t width, uint16_t height);

  // Return the closest image type.
  ImageType FindClosestImageType(uint16_t width, uint16_t height);

  // Get the frame rate level.
  FrameRateLevelClass FrameRateLevel(float frame_rate);

 protected:
  // Content Data.
  const VideoContentMetrics* content_metrics_;

  // Encoder frame sizes and native frame sizes.
  uint16_t width_;
  uint16_t height_;
  float user_frame_rate_;
  uint16_t native_width_;
  uint16_t native_height_;
  float native_frame_rate_;
  float aspect_ratio_;
  // Image type and frame rate leve, for the current encoder resolution.
  ImageType image_type_;
  FrameRateLevelClass framerate_level_;
  // Content class data.
  VCMContFeature motion_;
  VCMContFeature spatial_;
  uint8_t content_class_;
  bool init_;
};

// Resolution settings class

class VCMQmResolution : public VCMQmMethod {
 public:
  VCMQmResolution();
  virtual ~VCMQmResolution();

  // Reset all quantities.
  virtual void Reset();

  // Reset rate quantities and counters after every SelectResolution() call.
  void ResetRates();

  // Reset down-sampling state.
  void ResetDownSamplingState();

  // Get the encoder state.
  EncoderState GetEncoderState();

  // Initialize after SetEncodingData in media_opt.
  int Initialize(float bitrate,
                 float user_framerate,
                 uint16_t width,
                 uint16_t height,
                 int num_layers);

  // Update the encoder frame size.
  void UpdateCodecParameters(float frame_rate, uint16_t width, uint16_t height);

  // Update with actual bit rate (size of the latest encoded frame)
  // and frame type, after every encoded frame.
  void UpdateEncodedSize(int encoded_size,
                         FrameType encoded_frame_type);

  // Update with new target bitrate, actual encoder sent rate, frame_rate,
  // loss rate: every ~1 sec from SetTargetRates in media_opt.
  void UpdateRates(float target_bitrate,
                   float encoder_sent_rate,
                   float incoming_framerate,
                   uint8_t packet_loss);

  // Extract ST (spatio-temporal) resolution action.
  // Inputs: qm: Reference to the quality modes pointer.
  // Output: the spatial and/or temporal scale change.
  int SelectResolution(VCMResolutionScale** qm);

  // Update with current system load
  void SetCPULoadState(CPULoadState state);

 private:
  // Set the default resolution action.
  void SetDefaultAction();

  // Compute rates for the selection of down-sampling action.
  void ComputeRatesForSelection();

  // Compute the encoder state.
  void ComputeEncoderState();

  // Return true if the action is to go back up in resolution.
  bool GoingUpResolution();

  // Return true if the action is to go down in resolution.
  bool GoingDownResolution();

  // Check the condition for going up in resolution by the scale factors:
  // |facWidth|, |facHeight|, |facTemp|.
  // |scaleFac| is a scale factor for the transition rate.
  bool ConditionForGoingUp(float fac_width,
                           float fac_height,
                           float fac_temp,
                           float scale_fac);

  // Get the bitrate threshold for the resolution action.
  // The case |facWidth|=|facHeight|=|facTemp|==1 is for down-sampling action.
  // |scaleFac| is a scale factor for the transition rate.
  float GetTransitionRate(float fac_width,
                          float fac_height,
                          float fac_temp,
                          float scale_fac);

  // Update the down-sampling state.
  void UpdateDownsamplingState(UpDownAction up_down);

  // Update the codec frame size and frame rate.
  void UpdateCodecResolution();

  // Return a state based on average target rate relative transition rate.
  uint8_t RateClass(float transition_rate);

  // Adjust the action selected from the table.
  void AdjustAction();

  // Covert 2 stages of 3/4 (=9/16) spatial decimation to 1/2.
  void ConvertSpatialFractionalToWhole();

  // Insert latest down-sampling action into the history list.
  void InsertLatestDownAction();

  // Remove the last (first element) down-sampling action from the list.
  void RemoveLastDownAction();

  // Check constraints on the amount of down-sampling allowed.
  void ConstrainAmountOfDownSampling();

  // For going up in resolution: pick spatial or temporal action,
  // if both actions were separately selected.
  void PickSpatialOrTemporal();

  // Select the directional (1x2 or 2x1) spatial down-sampling action.
  void SelectSpatialDirectionMode(float transition_rate);

  enum { kDownActionHistorySize = 10};

  VCMResolutionScale* qm_;
  // Encoder rate control parameters.
  float target_bitrate_;
  float incoming_framerate_;
  float per_frame_bandwidth_;
  float buffer_level_;

  // Data accumulated every ~1sec from MediaOpt.
  float sum_target_rate_;
  float sum_incoming_framerate_;
  float sum_rate_MM_;
  float sum_rate_MM_sgn_;
  float sum_packet_loss_;
  // Counters.
  uint32_t frame_cnt_;
  uint32_t frame_cnt_delta_;
  uint32_t update_rate_cnt_;
  uint32_t low_buffer_cnt_;

  // Resolution state parameters.
  float state_dec_factor_spatial_;
  float state_dec_factor_temporal_;

  // Quantities used for selection.
  float avg_target_rate_;
  float avg_incoming_framerate_;
  float avg_ratio_buffer_low_;
  float avg_rate_mismatch_;
  float avg_rate_mismatch_sgn_;
  float avg_packet_loss_;
  EncoderState encoder_state_;
  ResolutionAction action_;
  // Short history of the down-sampling actions from the Initialize() state.
  // This is needed for going up in resolution. Since the total amount of
  // down-sampling actions are constrained, the length of the list need not be
  // large: i.e., (4/3) ^{kDownActionHistorySize} <= kMaxDownSample.
  ResolutionAction down_action_history_[kDownActionHistorySize];
  int num_layers_;
  CPULoadState loadstate_;
};

// Robustness settings class.

class VCMQmRobustness : public VCMQmMethod {
 public:
  VCMQmRobustness();
  ~VCMQmRobustness();

  virtual void Reset();

  // Adjust FEC rate based on content: every ~1 sec from SetTargetRates.
  // Returns an adjustment factor.
  float AdjustFecFactor(uint8_t code_rate_delta,
                        float total_rate,
                        float framerate,
                        uint32_t rtt_time,
                        uint8_t packet_loss);

  // Set the UEP protection on/off.
  bool SetUepProtection(uint8_t code_rate_delta,
                        float total_rate,
                        uint8_t packet_loss,
                        bool frame_type);

 private:
  // Previous state of network parameters.
  float prev_total_rate_;
  uint32_t prev_rtt_time_;
  uint8_t prev_packet_loss_;
  uint8_t prev_code_rate_delta_;
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_VIDEO_CODING_QM_SELECT_H_
