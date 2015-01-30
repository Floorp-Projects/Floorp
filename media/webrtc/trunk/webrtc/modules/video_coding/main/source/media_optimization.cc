/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/main/source/media_optimization.h"

#include "webrtc/modules/video_coding/main/source/content_metrics_processing.h"
#include "webrtc/modules/video_coding/main/source/qm_select.h"
#include "webrtc/modules/video_coding/utility/include/frame_dropper.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/logging.h"

namespace webrtc {
namespace media_optimization {
namespace {
void UpdateProtectionCallback(
    VCMProtectionMethod* selected_method,
    uint32_t* video_rate_bps,
    uint32_t* nack_overhead_rate_bps,
    uint32_t* fec_overhead_rate_bps,
    VCMProtectionCallback* video_protection_callback) {
  FecProtectionParams delta_fec_params;
  FecProtectionParams key_fec_params;
  // Get the FEC code rate for Key frames (set to 0 when NA).
  key_fec_params.fec_rate = selected_method->RequiredProtectionFactorK();

  // Get the FEC code rate for Delta frames (set to 0 when NA).
  delta_fec_params.fec_rate = selected_method->RequiredProtectionFactorD();

  // Get the FEC-UEP protection status for Key frames: UEP on/off.
  key_fec_params.use_uep_protection = selected_method->RequiredUepProtectionK();

  // Get the FEC-UEP protection status for Delta frames: UEP on/off.
  delta_fec_params.use_uep_protection =
      selected_method->RequiredUepProtectionD();

  // The RTP module currently requires the same |max_fec_frames| for both
  // key and delta frames.
  delta_fec_params.max_fec_frames = selected_method->MaxFramesFec();
  key_fec_params.max_fec_frames = selected_method->MaxFramesFec();

  // Set the FEC packet mask type. |kFecMaskBursty| is more effective for
  // consecutive losses and little/no packet re-ordering. As we currently
  // do not have feedback data on the degree of correlated losses and packet
  // re-ordering, we keep default setting to |kFecMaskRandom| for now.
  delta_fec_params.fec_mask_type = kFecMaskRandom;
  key_fec_params.fec_mask_type = kFecMaskRandom;

  // TODO(Marco): Pass FEC protection values per layer.
  video_protection_callback->ProtectionRequest(&delta_fec_params,
                                               &key_fec_params,
                                               video_rate_bps,
                                               nack_overhead_rate_bps,
                                               fec_overhead_rate_bps);
}
}  // namespace

struct MediaOptimization::EncodedFrameSample {
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

MediaOptimization::MediaOptimization(Clock* clock)
    : crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
      clock_(clock),
      max_bit_rate_(0),
      send_codec_type_(kVideoCodecUnknown),
      codec_width_(0),
      codec_height_(0),
      user_frame_rate_(0),
      frame_dropper_(new FrameDropper),
      loss_prot_logic_(
          new VCMLossProtectionLogic(clock_->TimeInMilliseconds())),
      fraction_lost_(0),
      send_statistics_zero_encode_(0),
      max_payload_size_(1460),
      target_bit_rate_(0),
      incoming_frame_rate_(0),
      enable_qm_(false),
      encoded_frame_samples_(),
      avg_sent_bit_rate_bps_(0),
      avg_sent_framerate_(0),
      key_frame_cnt_(0),
      delta_frame_cnt_(0),
      content_(new VCMContentMetricsProcessing()),
      qm_resolution_(new VCMQmResolution()),
      last_qm_update_time_(0),
      last_change_time_(0),
      num_layers_(0),
      suspension_enabled_(false),
      video_suspended_(false),
      suspension_threshold_bps_(0),
      suspension_window_bps_(0),
      loadstate_(kLoadNormal) {
  memset(send_statistics_, 0, sizeof(send_statistics_));
  memset(incoming_frame_times_, -1, sizeof(incoming_frame_times_));
}

MediaOptimization::~MediaOptimization(void) {
  loss_prot_logic_->Release();
}

void MediaOptimization::Reset() {
  CriticalSectionScoped lock(crit_sect_.get());
  SetEncodingData(
      kVideoCodecUnknown, 0, 0, 0, 0, 0, 1, 0, max_payload_size_);
  memset(incoming_frame_times_, -1, sizeof(incoming_frame_times_));
  incoming_frame_rate_ = 0.0;
  frame_dropper_->Reset();
  loss_prot_logic_->Reset(clock_->TimeInMilliseconds());
  frame_dropper_->SetRates(0, 0);
  content_->Reset();
  qm_resolution_->Reset();
  loss_prot_logic_->UpdateFrameRate(incoming_frame_rate_);
  loss_prot_logic_->Reset(clock_->TimeInMilliseconds());
  send_statistics_zero_encode_ = 0;
  target_bit_rate_ = 0;
  codec_width_ = 0;
  codec_height_ = 0;
  min_width_ = 0;
  min_height_ = 0;
  user_frame_rate_ = 0;
  key_frame_cnt_ = 0;
  delta_frame_cnt_ = 0;
  last_qm_update_time_ = 0;
  last_change_time_ = 0;
  encoded_frame_samples_.clear();
  avg_sent_bit_rate_bps_ = 0;
  num_layers_ = 1;
}

// Euclid's algorithm
// on arm, binary may be faster, but we do this rarely
static int GreatestCommonDenominator(int a, int b) {
  while (b != 0) {
    int t = b;
    b = a % b;
    a = t;
  }
  return a;
}

void MediaOptimization::SetEncodingData(VideoCodecType send_codec_type,
                                        int32_t max_bit_rate,    // in bits/s
                                        uint32_t frame_rate,     // in fps*1000
                                        uint32_t target_bitrate, // in bits/s
                                        uint16_t width,
                                        uint16_t height,
                                        uint8_t  divisor,
                                        int num_layers,
                                        int32_t mtu) {
  CriticalSectionScoped lock(crit_sect_.get());
  SetEncodingDataInternal(send_codec_type,
                          max_bit_rate,
                          frame_rate,
                          target_bitrate,
                          width,
                          height,
                          divisor,
                          num_layers,
                          mtu);
}

void MediaOptimization::SetEncodingDataInternal(VideoCodecType send_codec_type,
                                                int32_t max_bit_rate,    // in bits/s
                                                uint32_t frame_rate,     // in fps*1000
                                                uint32_t target_bitrate, // in bits/s
                                                uint16_t width,
                                                uint16_t height,
                                                uint8_t  divisor,
                                                int num_layers,
                                                int32_t mtu) {
  // Everything codec specific should be reset here since this means the codec
  // has changed. If native dimension values have changed, then either user
  // initiated change, or QM initiated change. Will be able to determine only
  // after the processing of the first frame.
  last_change_time_ = clock_->TimeInMilliseconds();
  content_->Reset();
  content_->UpdateFrameRate(static_cast<float>(frame_rate) / 1000.0f);

  max_bit_rate_ = max_bit_rate;
  send_codec_type_ = send_codec_type;
  target_bit_rate_ = target_bitrate;
  float target_bitrate_kbps = static_cast<float>(target_bitrate) / 1000.0f;
  loss_prot_logic_->UpdateBitRate(target_bitrate_kbps);
  loss_prot_logic_->UpdateFrameRate(static_cast<float>(frame_rate) / 1000.0f);
  loss_prot_logic_->UpdateFrameSize(width, height);
  loss_prot_logic_->UpdateNumLayers(num_layers);
  frame_dropper_->Reset();
  frame_dropper_->SetRates(target_bitrate_kbps, static_cast<float>(frame_rate) / 1000.0f);
  user_frame_rate_ = static_cast<float>(frame_rate)/1000.0f;
  codec_width_ = width;
  codec_height_ = height;
  int gcd = GreatestCommonDenominator(codec_width_, codec_height_);
  min_width_ = gcd ? (codec_width_/gcd * divisor) : 0;
  min_height_ = gcd ? (codec_height_/gcd * divisor) : 0;
  num_layers_ = (num_layers <= 1) ? 1 : num_layers;  // Can also be zero.
  max_payload_size_ = mtu;
  qm_resolution_->Initialize(target_bitrate_kbps,
                             user_frame_rate_,
                             codec_width_,
                             codec_height_,
                             num_layers_);
}

uint32_t MediaOptimization::SetTargetRates(
    uint32_t target_bitrate,
    uint8_t fraction_lost,
    uint32_t round_trip_time_ms,
    VCMProtectionCallback* protection_callback,
    VCMQMSettingsCallback* qmsettings_callback) {
  LOG(LS_INFO) << "SetTargetRates: " <<  target_bitrate << " bps " << fraction_lost
               << "% loss " << round_trip_time_ms << "ms RTT";

  CriticalSectionScoped lock(crit_sect_.get());
  // TODO(holmer): Consider putting this threshold only on the video bitrate,
  // and not on protection.
  if (max_bit_rate_ > 0 &&
      target_bitrate > static_cast<uint32_t>(max_bit_rate_)) {
    target_bitrate = max_bit_rate_;
  }
  VCMProtectionMethod* selected_method = loss_prot_logic_->SelectedMethod();
  float target_bitrate_kbps = static_cast<float>(target_bitrate) / 1000.0f;
  loss_prot_logic_->UpdateBitRate(target_bitrate_kbps);
  loss_prot_logic_->UpdateRtt(round_trip_time_ms);
  loss_prot_logic_->UpdateResidualPacketLoss(static_cast<float>(fraction_lost));

  // Get frame rate for encoder: this is the actual/sent frame rate.
  float actual_frame_rate = SentFrameRateInternal();

  // Sanity check.
  if (actual_frame_rate < 1.0) {
    actual_frame_rate = 1.0;
  }

  // Update frame rate for the loss protection logic class: frame rate should
  // be the actual/sent rate.
  loss_prot_logic_->UpdateFrameRate(actual_frame_rate);

  fraction_lost_ = fraction_lost;

  // Returns the filtered packet loss, used for the protection setting.
  // The filtered loss may be the received loss (no filter), or some
  // filtered value (average or max window filter).
  // Use max window filter for now.
  FilterPacketLossMode filter_mode = kMaxFilter;
  uint8_t packet_loss_enc = loss_prot_logic_->FilteredLoss(
      clock_->TimeInMilliseconds(), filter_mode, fraction_lost);

  // For now use the filtered loss for computing the robustness settings.
  loss_prot_logic_->UpdateFilteredLossPr(packet_loss_enc);

  // Rate cost of the protection methods.
  uint32_t protection_overhead_bps = 0;

  // Update protection settings, when applicable.
  float sent_video_rate_kbps = 0.0f;
  if (selected_method) {
    // Update protection method with content metrics.
    selected_method->UpdateContentMetrics(content_->ShortTermAvgData());

    // Update method will compute the robustness settings for the given
    // protection method and the overhead cost
    // the protection method is set by the user via SetVideoProtection.
    loss_prot_logic_->UpdateMethod();

    // Update protection callback with protection settings.
    uint32_t sent_video_rate_bps = 0;
    uint32_t sent_nack_rate_bps = 0;
    uint32_t sent_fec_rate_bps = 0;
    // Get the bit cost of protection method, based on the amount of
    // overhead data actually transmitted (including headers) the last
    // second.
    if (protection_callback) {
      UpdateProtectionCallback(selected_method,
                               &sent_video_rate_bps,
                               &sent_nack_rate_bps,
                               &sent_fec_rate_bps,
                               protection_callback);
    }
    uint32_t sent_total_rate_bps =
        sent_video_rate_bps + sent_nack_rate_bps + sent_fec_rate_bps;
    // Estimate the overhead costs of the next second as staying the same
    // wrt the source bitrate.
    if (sent_total_rate_bps > 0) {
      protection_overhead_bps = static_cast<uint32_t>(
          target_bitrate *
              static_cast<double>(sent_nack_rate_bps + sent_fec_rate_bps) /
              sent_total_rate_bps +
          0.5);
    }
    // Cap the overhead estimate to 50%.
    if (protection_overhead_bps > target_bitrate / 2)
      protection_overhead_bps = target_bitrate / 2;

    // Get the effective packet loss for encoder ER when applicable. Should be
    // passed to encoder via fraction_lost.
    packet_loss_enc = selected_method->RequiredPacketLossER();
    sent_video_rate_kbps = static_cast<float>(sent_video_rate_bps) / 1000.0f;
  }

  // Source coding rate: total rate - protection overhead.
  target_bit_rate_ = target_bitrate - protection_overhead_bps;

  // Update encoding rates following protection settings.
  float target_video_bitrate_kbps =
      static_cast<float>(target_bit_rate_) / 1000.0f;
  frame_dropper_->SetRates(target_video_bitrate_kbps, incoming_frame_rate_);

  if (enable_qm_ && qmsettings_callback) {
  LOG(LS_INFO) << "SetTargetRates/enable_qm: " <<  target_video_bitrate_kbps
               << " bps, " << sent_video_rate_kbps << " kbps, " << incoming_frame_rate_
               << " fps, " << fraction_lost << " loss";

    // Update QM with rates.
    qm_resolution_->UpdateRates(target_video_bitrate_kbps,
                                sent_video_rate_kbps,
                                incoming_frame_rate_,
                                fraction_lost_);
    // Check for QM selection.
    bool select_qm = CheckStatusForQMchange();
    if (select_qm) {
      SelectQuality(qmsettings_callback);
    }
    // Reset the short-term averaged content data.
    content_->ResetShortTermAvgData();
  }

  CheckSuspendConditions();

  return target_bit_rate_;
}

void MediaOptimization::EnableProtectionMethod(bool enable,
                                               VCMProtectionMethodEnum method) {
  CriticalSectionScoped lock(crit_sect_.get());
  bool updated = false;
  if (enable) {
    updated = loss_prot_logic_->SetMethod(method);
  } else {
    loss_prot_logic_->RemoveMethod(method);
  }
  if (updated) {
    loss_prot_logic_->UpdateMethod();
  }
}

uint32_t MediaOptimization::InputFrameRate() {
  CriticalSectionScoped lock(crit_sect_.get());
  return InputFrameRateInternal();
}

uint32_t MediaOptimization::InputFrameRateInternal() {
  ProcessIncomingFrameRate(clock_->TimeInMilliseconds());
  return uint32_t(incoming_frame_rate_ + 0.5f);
}

uint32_t MediaOptimization::SentFrameRate() {
  CriticalSectionScoped lock(crit_sect_.get());
  return SentFrameRateInternal();
}

uint32_t MediaOptimization::SentFrameRateInternal() {
  PurgeOldFrameSamples(clock_->TimeInMilliseconds());
  UpdateSentFramerate();
  return avg_sent_framerate_;
}

uint32_t MediaOptimization::SentBitRate() {
  CriticalSectionScoped lock(crit_sect_.get());
  const int64_t now_ms = clock_->TimeInMilliseconds();
  PurgeOldFrameSamples(now_ms);
  UpdateSentBitrate(now_ms);
  return avg_sent_bit_rate_bps_;
}

VCMFrameCount MediaOptimization::SentFrameCount() {
  CriticalSectionScoped lock(crit_sect_.get());
  VCMFrameCount count;
  count.numDeltaFrames = delta_frame_cnt_;
  count.numKeyFrames = key_frame_cnt_;
  return count;
}

int32_t MediaOptimization::UpdateWithEncodedData(int encoded_length,
                                                 uint32_t timestamp,
                                                 FrameType encoded_frame_type) {
  CriticalSectionScoped lock(crit_sect_.get());
  const int64_t now_ms = clock_->TimeInMilliseconds();
  bool same_frame;
  PurgeOldFrameSamples(now_ms);
  if (encoded_frame_samples_.size() > 0 &&
      encoded_frame_samples_.back().timestamp == timestamp) {
    // Frames having the same timestamp are generated from the same input
    // frame. We don't want to double count them, but only increment the
    // size_bytes.
    encoded_frame_samples_.back().size_bytes += encoded_length;
    encoded_frame_samples_.back().time_complete_ms = now_ms;
    same_frame = true;
  } else {
    encoded_frame_samples_.push_back(
        EncodedFrameSample(encoded_length, timestamp, now_ms));
    same_frame = false;
  }
  UpdateSentBitrate(now_ms);
  UpdateSentFramerate();
  if (encoded_length > 0) {
    const bool delta_frame = (encoded_frame_type != kVideoFrameKey);

    // XXX TODO(jesup): if same_frame is true, we should be considering it a single
    // frame here.
    frame_dropper_->Fill(encoded_length, delta_frame);
    if (max_payload_size_ > 0 && encoded_length > 0) {
      const float min_packets_per_frame =
          encoded_length / static_cast<float>(max_payload_size_);
      if (delta_frame) {
        loss_prot_logic_->UpdatePacketsPerFrame(min_packets_per_frame,
                                                clock_->TimeInMilliseconds());
      } else {
        loss_prot_logic_->UpdatePacketsPerFrameKey(
            min_packets_per_frame, clock_->TimeInMilliseconds());
      }

      if (enable_qm_) {
        // Update quality select with encoded length.
        qm_resolution_->UpdateEncodedSize(encoded_length, encoded_frame_type);
      }
    }
    if (!delta_frame && encoded_length > 0) {
      loss_prot_logic_->UpdateKeyFrameSize(static_cast<float>(encoded_length));
    }

    // Updating counters.
    if (!same_frame) {
      if (delta_frame) {
        delta_frame_cnt_++;
      } else {
        key_frame_cnt_++;
      }
    }
  }

  return VCM_OK;
}

void MediaOptimization::EnableQM(bool enable) {
  CriticalSectionScoped lock(crit_sect_.get());
  enable_qm_ = enable;
}

void MediaOptimization::EnableFrameDropper(bool enable) {
  CriticalSectionScoped lock(crit_sect_.get());
  frame_dropper_->Enable(enable);
}

void MediaOptimization::SuspendBelowMinBitrate(int threshold_bps,
                                               int window_bps) {
  CriticalSectionScoped lock(crit_sect_.get());
  assert(threshold_bps > 0 && window_bps >= 0);
  suspension_threshold_bps_ = threshold_bps;
  suspension_window_bps_ = window_bps;
  suspension_enabled_ = true;
  video_suspended_ = false;
}

bool MediaOptimization::IsVideoSuspended() const {
  CriticalSectionScoped lock(crit_sect_.get());
  return video_suspended_;
}

bool MediaOptimization::DropFrame() {
  CriticalSectionScoped lock(crit_sect_.get());
  UpdateIncomingFrameRate();
  // Leak appropriate number of bytes.
  frame_dropper_->Leak((uint32_t)(InputFrameRateInternal() + 0.5f));
  if (video_suspended_) {
    return true;  // Drop all frames when muted.
  }
  return frame_dropper_->DropFrame();
}

void MediaOptimization::UpdateContentData(
    const VideoContentMetrics* content_metrics) {
  CriticalSectionScoped lock(crit_sect_.get());
  // Updating content metrics.
  if (content_metrics == NULL) {
    // Disable QM if metrics are NULL.
    enable_qm_ = false;
    qm_resolution_->Reset();
  } else {
    content_->UpdateContentData(content_metrics);
  }
}

void MediaOptimization::UpdateIncomingFrameRate() {
  int64_t now = clock_->TimeInMilliseconds();
  if (incoming_frame_times_[0] == 0) {
    // No shifting if this is the first time.
  } else {
    // Shift all times one step.
    for (int32_t i = (kFrameCountHistorySize - 2); i >= 0; i--) {
      incoming_frame_times_[i + 1] = incoming_frame_times_[i];
    }
  }
  incoming_frame_times_[0] = now;
  ProcessIncomingFrameRate(now);
}

int32_t MediaOptimization::SelectQuality(
    VCMQMSettingsCallback* video_qmsettings_callback) {
  // Reset quantities for QM select.
  qm_resolution_->ResetQM();

  // Update QM will long-term averaged content metrics.
  qm_resolution_->UpdateContent(content_->LongTermAvgData());

  // Update QM with current CPU load
  qm_resolution_->SetCPULoadState(loadstate_);

  // Select quality mode.
  VCMResolutionScale* qm = NULL;
  int32_t ret = qm_resolution_->SelectResolution(&qm);
  if (ret < 0) {
    return ret;
  }

  // Check for updates to spatial/temporal modes.
  QMUpdate(qm, video_qmsettings_callback);

  // Reset all the rate and related frame counters quantities.
  qm_resolution_->ResetRates();

  // Reset counters.
  last_qm_update_time_ = clock_->TimeInMilliseconds();

  // Reset content metrics.
  content_->Reset();

  return VCM_OK;
}

void MediaOptimization::PurgeOldFrameSamples(int64_t now_ms) {
  while (!encoded_frame_samples_.empty()) {
    if (now_ms - encoded_frame_samples_.front().time_complete_ms >
        kBitrateAverageWinMs) {
      encoded_frame_samples_.pop_front();
    } else {
      break;
    }
  }
}

void MediaOptimization::UpdateSentBitrate(int64_t now_ms) {
  if (encoded_frame_samples_.empty()) {
    avg_sent_bit_rate_bps_ = 0;
    return;
  }
  int framesize_sum = 0;
  for (FrameSampleList::iterator it = encoded_frame_samples_.begin();
       it != encoded_frame_samples_.end();
       ++it) {
    framesize_sum += it->size_bytes;
  }
  float denom = static_cast<float>(
      now_ms - encoded_frame_samples_.front().time_complete_ms);
  if (denom >= 1.0f) {
    avg_sent_bit_rate_bps_ =
        static_cast<uint32_t>(framesize_sum * 8.0f * 1000.0f / denom + 0.5f);
  } else {
    avg_sent_bit_rate_bps_ = framesize_sum * 8;
  }
}

void MediaOptimization::UpdateSentFramerate() {
  if (encoded_frame_samples_.size() <= 1) {
    avg_sent_framerate_ = encoded_frame_samples_.size();
    return;
  }
  int denom = encoded_frame_samples_.back().timestamp -
              encoded_frame_samples_.front().timestamp;
  if (denom > 0) {
    avg_sent_framerate_ =
        (90000 * (encoded_frame_samples_.size() - 1) + denom / 2) / denom;
  } else {
    avg_sent_framerate_ = encoded_frame_samples_.size();
  }
}

bool MediaOptimization::QMUpdate(
    VCMResolutionScale* qm,
    VCMQMSettingsCallback* video_qmsettings_callback) {
  // Check for no change.
  if (!qm->change_resolution_spatial && !qm->change_resolution_temporal) {
    return false;
  }

  // Check for change in frame rate.
  if (qm->change_resolution_temporal) {
    incoming_frame_rate_ = qm->frame_rate;
    // Reset frame rate estimate.
    memset(incoming_frame_times_, -1, sizeof(incoming_frame_times_));
  }

  // Check for change in frame size.
  if (qm->change_resolution_spatial) {
    codec_width_ = qm->codec_width;
    codec_height_ = qm->codec_height;
  }
  // handle codec limitations on input resolutions
  if (codec_width_ % min_width_ != 0 || codec_height_ % min_height_ != 0) {
    // XXX find a better algorithm for selecting sizes
    // This uses the GCD to find options that meet alignment requirements
    // (divisor) and at the same time retain the aspect ratio exactly.
    codec_width_ = ((codec_width_ + min_width_-1)/min_width_)*min_width_;
    codec_height_ = ((codec_height_ + min_height_-1)/min_height_)*min_height_;

    // to avoid confusion later
    qm->codec_width = codec_width_;
   qm->codec_height = codec_height_;
  }


  LOG(LS_INFO) << "Media optimizer requests the video resolution to be changed "
              "to " << qm->codec_width << " (" << codec_width_ << ") x " 
               << qm->codec_height << " (" << codec_height_ << ") @ "
               << qm->frame_rate;

  // Update VPM with new target frame rate and frame size.
  // Note: use |qm->frame_rate| instead of |_incoming_frame_rate| for updating
  // target frame rate in VPM frame dropper. The quantity |_incoming_frame_rate|
  // will vary/fluctuate, and since we don't want to change the state of the
  // VPM frame dropper, unless a temporal action was selected, we use the
  // quantity |qm->frame_rate| for updating.
  video_qmsettings_callback->SetVideoQMSettings(
      qm->frame_rate, codec_width_, codec_height_);
  content_->UpdateFrameRate(qm->frame_rate);
  qm_resolution_->UpdateCodecParameters(
      qm->frame_rate, codec_width_, codec_height_);
  return true;
}

// Check timing constraints and look for significant change in:
// (1) scene content,
// (2) target bit rate.
bool MediaOptimization::CheckStatusForQMchange() {
  bool status = true;

  // Check that we do not call QMSelect too often, and that we waited some time
  // (to sample the metrics) from the event last_change_time
  // last_change_time is the time where user changed the size/rate/frame rate
  // (via SetEncodingData).
  int64_t now = clock_->TimeInMilliseconds();
  if ((now - last_qm_update_time_) < kQmMinIntervalMs ||
      (now - last_change_time_) < kQmMinIntervalMs) {
    status = false;
  }

  return status;
}

// Allowing VCM to keep track of incoming frame rate.
void MediaOptimization::ProcessIncomingFrameRate(int64_t now) {
  int32_t num = 0;
  int32_t nr_of_frames = 0;
  for (num = 1; num < (kFrameCountHistorySize - 1); ++num) {
    if (incoming_frame_times_[num] <= 0 ||
        // don't use data older than 2 s
        now - incoming_frame_times_[num] > kFrameHistoryWinMs) {
      break;
    } else {
      nr_of_frames++;
    }
  }
  if (num > 1) {
    const int64_t diff = now - incoming_frame_times_[num - 1];
    incoming_frame_rate_ = 1.0;
    if (diff > 0) {
      incoming_frame_rate_ = nr_of_frames * 1000.0f / static_cast<float>(diff);
    }
  }
}

void MediaOptimization::SetCPULoadState(CPULoadState state) {
    loadstate_ = state;
}

void MediaOptimization::CheckSuspendConditions() {
  // Check conditions for SuspendBelowMinBitrate. |target_bit_rate_| is in bps.
  if (suspension_enabled_) {
    if (!video_suspended_) {
      // Check if we just went below the threshold.
      if (target_bit_rate_ < suspension_threshold_bps_) {
        video_suspended_ = true;
      }
    } else {
      // Video is already suspended. Check if we just went over the threshold
      // with a margin.
      if (target_bit_rate_ >
          suspension_threshold_bps_ + suspension_window_bps_) {
        video_suspended_ = false;
      }
    }
  }
}

}  // namespace media_optimization
}  // namespace webrtc
