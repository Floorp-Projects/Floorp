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

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/modules/video_coding/main/interface/video_coding.h"
#include "webrtc/modules/video_coding/main/source/media_opt_util.h"
#include "webrtc/modules/video_coding/main/source/qm_select.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"

namespace webrtc {

// Forward declarations.
class Clock;
class FrameDropper;
class VCMContentMetricsProcessing;

namespace media_optimization {

class MediaOptimization {
 public:
  explicit MediaOptimization(Clock* clock);
  ~MediaOptimization();

  // TODO(andresp): Can Reset and SetEncodingData be done at construction time
  // only?
  void Reset();

  // Informs media optimization of initial encoding state.
  void SetEncodingData(VideoCodecType send_codec_type,
                       int32_t max_bit_rate,    // in bits/s
                       uint32_t target_bitrate, // in bits/s
                       uint16_t width,
                       uint16_t height,
                       uint32_t frame_rate,     // in fps*1000
                       uint8_t divisor,
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
                          int64_t round_trip_time_ms,
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

  // Informs Media Optimization of encoded output.
  int32_t UpdateWithEncodedData(const EncodedImage& encoded_image);

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

  void UpdateIncomingFrameRate() EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);
  void PurgeOldFrameSamples(int64_t now_ms)
      EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);
  void UpdateSentBitrate(int64_t now_ms) EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);
  void UpdateSentFramerate() EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);

  // Computes new Quality Mode.
  int32_t SelectQuality(VCMQMSettingsCallback* qmsettings_callback)
      EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);

  // Verifies if QM settings differ from default, i.e. if an update is required.
  // Computes actual values, as will be sent to the encoder.
  bool QMUpdate(VCMResolutionScale* qm,
                VCMQMSettingsCallback* qmsettings_callback)
      EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);

  // Checks if we should make a QM change. Return true if yes, false otherwise.
  bool CheckStatusForQMchange() EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);

  void ProcessIncomingFrameRate(int64_t now)
      EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);

  // Checks conditions for suspending the video. The method compares
  // |target_bit_rate_| with the threshold values for suspension, and changes
  // the state of |video_suspended_| accordingly.
  void CheckSuspendConditions() EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);

  void SetEncodingDataInternal(VideoCodecType send_codec_type,
                               int32_t max_bit_rate,    // in bits/s
                               uint32_t target_bitrate, // in bits/s
                               uint16_t width,
                               uint16_t height,
                               uint32_t frame_rate,     // in fps*1000
                               uint8_t  divisor,
                               int num_temporal_layers,
                               int32_t mtu)
      EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);

  uint32_t InputFrameRateInternal() EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);

  uint32_t SentFrameRateInternal() EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);

  // Protect all members.
  rtc::scoped_ptr<CriticalSectionWrapper> crit_sect_;

  Clock* clock_ GUARDED_BY(crit_sect_);
  int32_t max_bit_rate_ GUARDED_BY(crit_sect_);
  VideoCodecType send_codec_type_ GUARDED_BY(crit_sect_);
  uint16_t codec_width_ GUARDED_BY(crit_sect_);
  uint16_t codec_height_ GUARDED_BY(crit_sect_);
  uint16_t min_width_ GUARDED_BY(crit_sect_);
  uint16_t min_height_  GUARDED_BY(crit_sect_);
  float user_frame_rate_ GUARDED_BY(crit_sect_);
  rtc::scoped_ptr<FrameDropper> frame_dropper_ GUARDED_BY(crit_sect_);
  rtc::scoped_ptr<VCMLossProtectionLogic> loss_prot_logic_
      GUARDED_BY(crit_sect_);
  uint8_t fraction_lost_ GUARDED_BY(crit_sect_);
  uint32_t send_statistics_[4] GUARDED_BY(crit_sect_);
  uint32_t send_statistics_zero_encode_ GUARDED_BY(crit_sect_);
  int32_t max_payload_size_ GUARDED_BY(crit_sect_);
  int target_bit_rate_ GUARDED_BY(crit_sect_);
  float incoming_frame_rate_ GUARDED_BY(crit_sect_);
  int64_t incoming_frame_times_[kFrameCountHistorySize] GUARDED_BY(crit_sect_);
  bool enable_qm_ GUARDED_BY(crit_sect_);
  std::list<EncodedFrameSample> encoded_frame_samples_ GUARDED_BY(crit_sect_);
  uint32_t avg_sent_bit_rate_bps_ GUARDED_BY(crit_sect_);
  uint32_t avg_sent_framerate_ GUARDED_BY(crit_sect_);
  uint32_t key_frame_cnt_ GUARDED_BY(crit_sect_);
  uint32_t delta_frame_cnt_ GUARDED_BY(crit_sect_);
  rtc::scoped_ptr<VCMContentMetricsProcessing> content_ GUARDED_BY(crit_sect_);
  rtc::scoped_ptr<VCMQmResolution> qm_resolution_ GUARDED_BY(crit_sect_);
  int64_t last_qm_update_time_ GUARDED_BY(crit_sect_);
  int64_t last_change_time_ GUARDED_BY(crit_sect_);  // Content/user triggered.
  int num_layers_ GUARDED_BY(crit_sect_);
  bool suspension_enabled_ GUARDED_BY(crit_sect_);
  bool video_suspended_ GUARDED_BY(crit_sect_);
  int suspension_threshold_bps_ GUARDED_BY(crit_sect_);
  int suspension_window_bps_ GUARDED_BY(crit_sect_);
  CPULoadState loadstate_ GUARDED_BY(crit_sect_);
};
}  // namespace media_optimization
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_MAIN_SOURCE_MEDIA_OPTIMIZATION_H_
