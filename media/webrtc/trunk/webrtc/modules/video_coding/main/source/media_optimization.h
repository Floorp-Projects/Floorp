/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_MAIN_SOURCE_MEDIA_OPTIMIZATION_H_
#define WEBRTC_MODULES_VIDEO_CODING_MAIN_SOURCE_MEDIA_OPTIMIZATION_H_

#include <list>

#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/modules/video_coding/main/interface/video_coding.h"
#include "webrtc/modules/video_coding/main/source/media_opt_util.h"
#include "webrtc/modules/video_coding/main/source/qm_select.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {

// Forward declarations.
class Clock;
class FrameDropper;
class VCMContentMetricsProcessing;

namespace media_optimization {

// TODO(andresp): Make thread safe.
class MediaOptimization {
 public:
  MediaOptimization(int32_t id, Clock* clock);
  ~MediaOptimization();

  // TODO(andresp): Can Reset and SetEncodingData be done at construction time
  // only?
  void Reset();

  // Informs media optimization of initial encoding state.
  void SetEncodingData(VideoCodecType send_codec_type,
                       int32_t max_bit_rate,
                       uint32_t frame_rate,
                       uint32_t bit_rate,
                       uint16_t width,
                       uint16_t height,
                       int num_temporal_layers,
                       int32_t mtu);

  // Sets target rates for the encoder given the channel parameters.
  // Inputs:  target bitrate - the encoder target bitrate in bits/s.
  //          fraction_lost - packet loss rate in % in the network.
  //          round_trip_time_ms - round trip time in milliseconds.
  //          min_bit_rate - the bit rate of the end-point with lowest rate.
  //          max_bit_rate - the bit rate of the end-point with highest rate.
  // TODO(andresp): Find if the callbacks can be triggered only after releasing
  // an internal critical section.
  uint32_t SetTargetRates(uint32_t target_bitrate,
                          uint8_t fraction_lost,
                          uint32_t round_trip_time_ms,
                          VCMProtectionCallback* protection_callback,
                          VCMQMSettingsCallback* qmsettings_callback);

  void EnableProtectionMethod(bool enable, VCMProtectionMethodEnum method);
  void EnableQM(bool enable);
  void EnableFrameDropper(bool enable);

  // Lets the sender suspend video when the rate drops below
  // |threshold_bps|, and turns back on when the rate goes back up above
  // |threshold_bps| + |window_bps|.
  void SuspendBelowMinBitrate(int threshold_bps, int window_bps);
  bool IsVideoSuspended() const;

  bool DropFrame();

  void UpdateContentData(const VideoContentMetrics* content_metrics);

  // Informs Media Optimization of encoding output: Length and frame type.
  int32_t UpdateWithEncodedData(int encoded_length,
                                uint32_t timestamp,
                                FrameType encoded_frame_type);

  // Informs Media Optimization of CPU Load state
  void SetCPULoadState(CPULoadState state);

  uint32_t InputFrameRate();
  uint32_t SentFrameRate();
  uint32_t SentBitRate();
  VCMFrameCount SentFrameCount();

 private:
  enum {
    kFrameCountHistorySize = 90
  };
  enum {
    kFrameHistoryWinMs = 2000
  };
  enum {
    kBitrateAverageWinMs = 1000
  };

  struct EncodedFrameSample;
  typedef std::list<EncodedFrameSample> FrameSampleList;

  void UpdateIncomingFrameRate();
  void PurgeOldFrameSamples(int64_t now_ms);
  void UpdateSentBitrate(int64_t now_ms);
  void UpdateSentFramerate();

  // Computes new Quality Mode.
  int32_t SelectQuality(VCMQMSettingsCallback* qmsettings_callback);

  // Verifies if QM settings differ from default, i.e. if an update is required.
  // Computes actual values, as will be sent to the encoder.
  bool QMUpdate(VCMResolutionScale* qm,
                VCMQMSettingsCallback* qmsettings_callback);

  // Checks if we should make a QM change. Return true if yes, false otherwise.
  bool CheckStatusForQMchange();

  void ProcessIncomingFrameRate(int64_t now);

  // Checks conditions for suspending the video. The method compares
  // |target_bit_rate_| with the threshold values for suspension, and changes
  // the state of |video_suspended_| accordingly.
  void CheckSuspendConditions();

  int32_t id_;
  Clock* clock_;
  int32_t max_bit_rate_;
  VideoCodecType send_codec_type_;
  uint16_t codec_width_;
  uint16_t codec_height_;
  float user_frame_rate_;
  scoped_ptr<FrameDropper> frame_dropper_;
  scoped_ptr<VCMLossProtectionLogic> loss_prot_logic_;
  uint8_t fraction_lost_;
  uint32_t send_statistics_[4];
  uint32_t send_statistics_zero_encode_;
  int32_t max_payload_size_;
  int target_bit_rate_;
  float incoming_frame_rate_;
  int64_t incoming_frame_times_[kFrameCountHistorySize];
  bool enable_qm_;
  std::list<EncodedFrameSample> encoded_frame_samples_;
  uint32_t avg_sent_bit_rate_bps_;
  uint32_t avg_sent_framerate_;
  uint32_t key_frame_cnt_;
  uint32_t delta_frame_cnt_;
  scoped_ptr<VCMContentMetricsProcessing> content_;
  scoped_ptr<VCMQmResolution> qm_resolution_;
  int64_t last_qm_update_time_;
  int64_t last_change_time_;  // Content/user triggered.
  int num_layers_;
  bool suspension_enabled_;
  bool video_suspended_;
  int suspension_threshold_bps_;
  int suspension_window_bps_;
  CPULoadState loadstate_;
};
}  // namespace media_optimization
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_MAIN_SOURCE_MEDIA_OPTIMIZATION_H_
