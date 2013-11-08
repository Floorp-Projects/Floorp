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

enum {
  kBitrateMaxFrameSamples = 60
};
enum {
  kBitrateAverageWinMs = 1000
};

struct EncodedFrameSample {
  EncodedFrameSample(int size_bytes,
                     uint32_t timestamp,
                     int64_t time_complete_ms)
      : size_bytes(size_bytes),
        timestamp(timestamp),
        time_complete_ms(time_complete_ms) {}

  uint32_t size_bytes;
  uint32_t timestamp;
  int64_t time_complete_ms;
};

class MediaOptimization {
 public:
  MediaOptimization(int32_t id, Clock* clock);
  ~MediaOptimization(void);

  // Resets the Media Optimization module.
  int32_t Reset();

  // Sets target rates for the encoder given the channel parameters.
  // Inputs:  target bitrate - the encoder target bitrate in bits/s.
  //          fraction_lost - packet loss rate in % in the network.
  //          round_trip_time_ms - round trip time in milliseconds.
  //          min_bit_rate - the bit rate of the end-point with lowest rate.
  //          max_bit_rate - the bit rate of the end-point with highest rate.
  uint32_t SetTargetRates(uint32_t target_bitrate,
                          uint8_t fraction_lost,
                          uint32_t round_trip_time_ms);

  // Informs media optimization of initial encoding state.
  int32_t SetEncodingData(VideoCodecType send_codec_type,
                          int32_t max_bit_rate,
                          uint32_t frame_rate,
                          uint32_t bit_rate,
                          uint16_t width,
                          uint16_t height,
                          int num_temporal_layers);

  // Enables protection method.
  void EnableProtectionMethod(bool enable, VCMProtectionMethodEnum method);

  // Returns weather or not protection method is enabled.
  bool IsProtectionMethodEnabled(VCMProtectionMethodEnum method);

  // Returns the actual input frame rate.
  uint32_t InputFrameRate();

  // Returns the actual sent frame rate.
  uint32_t SentFrameRate();

  // Returns the actual sent bit rate.
  uint32_t SentBitRate();

  // Informs Media Optimization of encoding output: Length and frame type.
  int32_t UpdateWithEncodedData(int encoded_length,
                                uint32_t timestamp,
                                FrameType encoded_frame_type);

  // Registers a protection callback to be used to inform the user about the
  // protection methods used.
  int32_t RegisterProtectionCallback(
      VCMProtectionCallback* protection_callback);

  // Registers a quality settings callback to be used to inform VPM/user.
  int32_t RegisterVideoQMCallback(VCMQMSettingsCallback* video_qmsettings);

  void EnableFrameDropper(bool enable);

  bool DropFrame();

  // Returns the number of key/delta frames encoded.
  int32_t SentFrameCount(VCMFrameCount* frame_count) const;

  // Updates incoming frame rate value.
  void UpdateIncomingFrameRate();

  // Update content metric data.
  void UpdateContentData(const VideoContentMetrics* content_metrics);

  // Computes new Quality Mode.
  int32_t SelectQuality();

  // Accessors and mutators.
  int32_t max_bit_rate() const { return max_bit_rate_; }
  void set_max_payload_size(int32_t mtu) { max_payload_size_ = mtu; }

 private:
  typedef std::list<EncodedFrameSample> FrameSampleList;
  enum {
    kFrameCountHistorySize = 90
  };
  enum {
    kFrameHistoryWinMs = 2000
  };

  // Updates protection callback with protection settings.
  int UpdateProtectionCallback(VCMProtectionMethod* selected_method,
                               uint32_t* total_video_rate_bps,
                               uint32_t* nack_overhead_rate_bps,
                               uint32_t* fec_overhead_rate_bps);

  void PurgeOldFrameSamples(int64_t now_ms);
  void UpdateSentBitrate(int64_t now_ms);
  void UpdateSentFramerate();

  // Verifies if QM settings differ from default, i.e. if an update is required.
  // Computes actual values, as will be sent to the encoder.
  bool QMUpdate(VCMResolutionScale* qm);

  // Checks if we should make a QM change. Return true if yes, false otherwise.
  bool CheckStatusForQMchange();

  void ProcessIncomingFrameRate(int64_t now);

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
  uint32_t target_bit_rate_;
  float incoming_frame_rate_;
  int64_t incoming_frame_times_[kFrameCountHistorySize];
  bool enable_qm_;
  VCMProtectionCallback* video_protection_callback_;
  VCMQMSettingsCallback* video_qmsettings_callback_;
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
};  // End of MediaOptimization class declaration.

}  // namespace media_optimization
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_MAIN_SOURCE_MEDIA_OPTIMIZATION_H_
