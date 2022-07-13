/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/pacing/pacing_controller.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "absl/strings/match.h"
#include "modules/pacing/bitrate_prober.h"
#include "modules/pacing/interval_budget.h"
#include "rtc_base/checks.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace {
// Time limit in milliseconds between packet bursts.
constexpr TimeDelta kDefaultMinPacketLimit = TimeDelta::Millis(5);
constexpr TimeDelta kCongestedPacketInterval = TimeDelta::Millis(500);
// TODO(sprang): Consider dropping this limit.
// The maximum debt level, in terms of time, capped when sending packets.
constexpr TimeDelta kMaxDebtInTime = TimeDelta::Millis(500);
constexpr TimeDelta kMaxElapsedTime = TimeDelta::Seconds(2);

// Upper cap on process interval, in case process has not been called in a long
// time. Applies only to periodic mode.
constexpr TimeDelta kMaxProcessingInterval = TimeDelta::Millis(30);

constexpr int kFirstPriority = 0;

bool IsDisabled(const FieldTrialsView& field_trials, absl::string_view key) {
  return absl::StartsWith(field_trials.Lookup(key), "Disabled");
}

bool IsEnabled(const FieldTrialsView& field_trials, absl::string_view key) {
  return absl::StartsWith(field_trials.Lookup(key), "Enabled");
}

TimeDelta GetDynamicPaddingTarget(const FieldTrialsView& field_trials) {
  FieldTrialParameter<TimeDelta> padding_target("timedelta",
                                                TimeDelta::Millis(5));
  ParseFieldTrial({&padding_target},
                  field_trials.Lookup("WebRTC-Pacer-DynamicPaddingTarget"));
  return padding_target.Get();
}

int GetPriorityForType(RtpPacketMediaType type) {
  // Lower number takes priority over higher.
  switch (type) {
    case RtpPacketMediaType::kAudio:
      // Audio is always prioritized over other packet types.
      return kFirstPriority + 1;
    case RtpPacketMediaType::kRetransmission:
      // Send retransmissions before new media.
      return kFirstPriority + 2;
    case RtpPacketMediaType::kVideo:
    case RtpPacketMediaType::kForwardErrorCorrection:
      // Video has "normal" priority, in the old speak.
      // Send redundancy concurrently to video. If it is delayed it might have a
      // lower chance of being useful.
      return kFirstPriority + 3;
    case RtpPacketMediaType::kPadding:
      // Packets that are in themselves likely useless, only sent to keep the
      // BWE high.
      return kFirstPriority + 4;
  }
  RTC_CHECK_NOTREACHED();
}

}  // namespace

const TimeDelta PacingController::kMaxExpectedQueueLength =
    TimeDelta::Millis(2000);
const float PacingController::kDefaultPaceMultiplier = 2.5f;
const TimeDelta PacingController::kPausedProcessInterval =
    kCongestedPacketInterval;
const TimeDelta PacingController::kMinSleepTime = TimeDelta::Millis(1);
const TimeDelta PacingController::kMaxEarlyProbeProcessing =
    TimeDelta::Millis(1);

PacingController::PacingController(Clock* clock,
                                   PacketSender* packet_sender,
                                   const FieldTrialsView& field_trials,
                                   ProcessMode mode)
    : mode_(mode),
      clock_(clock),
      packet_sender_(packet_sender),
      field_trials_(field_trials),
      drain_large_queues_(
          !IsDisabled(field_trials_, "WebRTC-Pacer-DrainQueue")),
      send_padding_if_silent_(
          IsEnabled(field_trials_, "WebRTC-Pacer-PadInSilence")),
      pace_audio_(IsEnabled(field_trials_, "WebRTC-Pacer-BlockAudio")),
      ignore_transport_overhead_(
          IsEnabled(field_trials_, "WebRTC-Pacer-IgnoreTransportOverhead")),
      padding_target_duration_(GetDynamicPaddingTarget(field_trials_)),
      min_packet_limit_(kDefaultMinPacketLimit),
      transport_overhead_per_packet_(DataSize::Zero()),
      last_timestamp_(clock_->CurrentTime()),
      paused_(false),
      media_budget_(0),
      padding_budget_(0),
      media_debt_(DataSize::Zero()),
      padding_debt_(DataSize::Zero()),
      media_rate_(DataRate::Zero()),
      padding_rate_(DataRate::Zero()),
      prober_(field_trials_),
      probing_send_failure_(false),
      pacing_bitrate_(DataRate::Zero()),
      last_process_time_(clock->CurrentTime()),
      last_send_time_(last_process_time_),
      packet_queue_(last_process_time_),
      packet_counter_(0),
      congested_(false),
      queue_time_limit_(kMaxExpectedQueueLength),
      account_for_audio_(false),
      include_overhead_(false) {
  if (!drain_large_queues_) {
    RTC_LOG(LS_WARNING) << "Pacer queues will not be drained,"
                           "pushback experiment must be enabled.";
  }
  FieldTrialParameter<int> min_packet_limit_ms("", min_packet_limit_.ms());
  ParseFieldTrial({&min_packet_limit_ms},
                  field_trials_.Lookup("WebRTC-Pacer-MinPacketLimitMs"));
  min_packet_limit_ = TimeDelta::Millis(min_packet_limit_ms.Get());
  UpdateBudgetWithElapsedTime(min_packet_limit_);
}

PacingController::~PacingController() = default;

void PacingController::CreateProbeCluster(DataRate bitrate, int cluster_id) {
  prober_.CreateProbeCluster(bitrate, CurrentTime(), cluster_id);
}

void PacingController::Pause() {
  if (!paused_)
    RTC_LOG(LS_INFO) << "PacedSender paused.";
  paused_ = true;
  packet_queue_.SetPauseState(true, CurrentTime());
}

void PacingController::Resume() {
  if (paused_)
    RTC_LOG(LS_INFO) << "PacedSender resumed.";
  paused_ = false;
  packet_queue_.SetPauseState(false, CurrentTime());
}

bool PacingController::IsPaused() const {
  return paused_;
}

void PacingController::SetCongested(bool congested) {
  if (congested_ && !congested) {
    UpdateBudgetWithElapsedTime(UpdateTimeAndGetElapsed(CurrentTime()));
  }
  congested_ = congested;
}

bool PacingController::IsProbing() const {
  return prober_.is_probing();
}

Timestamp PacingController::CurrentTime() const {
  Timestamp time = clock_->CurrentTime();
  if (time < last_timestamp_) {
    RTC_LOG(LS_WARNING)
        << "Non-monotonic clock behavior observed. Previous timestamp: "
        << last_timestamp_.ms() << ", new timestamp: " << time.ms();
    RTC_DCHECK_GE(time, last_timestamp_);
    time = last_timestamp_;
  }
  last_timestamp_ = time;
  return time;
}

void PacingController::SetProbingEnabled(bool enabled) {
  RTC_CHECK_EQ(0, packet_counter_);
  prober_.SetEnabled(enabled);
}

void PacingController::SetPacingRates(DataRate pacing_rate,
                                      DataRate padding_rate) {
  static constexpr DataRate kMaxRate = DataRate::KilobitsPerSec(100'000);
  RTC_CHECK_GT(pacing_rate, DataRate::Zero());
  RTC_CHECK_GE(padding_rate, DataRate::Zero());
  if (pacing_rate > kMaxRate || padding_rate > kMaxRate) {
    RTC_LOG(LS_WARNING) << "Very high pacing rates ( > " << kMaxRate.kbps()
                        << " kbps) configured: pacing = " << pacing_rate.kbps()
                        << " kbps, padding = " << padding_rate.kbps()
                        << " kbps.";
  }
  media_rate_ = pacing_rate;
  padding_rate_ = padding_rate;
  pacing_bitrate_ = pacing_rate;
  media_budget_.set_target_rate_kbps(pacing_rate.kbps());
  padding_budget_.set_target_rate_kbps(padding_rate.kbps());

  RTC_LOG(LS_VERBOSE) << "bwe:pacer_updated pacing_kbps="
                      << pacing_bitrate_.kbps()
                      << " padding_budget_kbps=" << padding_rate.kbps();
}

void PacingController::EnqueuePacket(std::unique_ptr<RtpPacketToSend> packet) {
  RTC_DCHECK(pacing_bitrate_ > DataRate::Zero())
      << "SetPacingRate must be called before InsertPacket.";
  RTC_CHECK(packet->packet_type());
  // Get priority first and store in temporary, to avoid chance of object being
  // moved before GetPriorityForType() being called.
  const int priority = GetPriorityForType(*packet->packet_type());
  EnqueuePacketInternal(std::move(packet), priority);
}

void PacingController::SetAccountForAudioPackets(bool account_for_audio) {
  account_for_audio_ = account_for_audio;
}

void PacingController::SetIncludeOverhead() {
  include_overhead_ = true;
  packet_queue_.SetIncludeOverhead();
}

void PacingController::SetTransportOverhead(DataSize overhead_per_packet) {
  if (ignore_transport_overhead_)
    return;
  transport_overhead_per_packet_ = overhead_per_packet;
  packet_queue_.SetTransportOverhead(overhead_per_packet);
}

TimeDelta PacingController::ExpectedQueueTime() const {
  RTC_DCHECK_GT(pacing_bitrate_, DataRate::Zero());
  return TimeDelta::Millis(
      (QueueSizeData().bytes() * 8 * rtc::kNumMillisecsPerSec) /
      pacing_bitrate_.bps());
}

size_t PacingController::QueueSizePackets() const {
  return packet_queue_.SizeInPackets();
}

DataSize PacingController::QueueSizeData() const {
  return packet_queue_.Size();
}

DataSize PacingController::CurrentBufferLevel() const {
  return std::max(media_debt_, padding_debt_);
}

absl::optional<Timestamp> PacingController::FirstSentPacketTime() const {
  return first_sent_packet_time_;
}

Timestamp PacingController::OldestPacketEnqueueTime() const {
  return packet_queue_.OldestEnqueueTime();
}

void PacingController::EnqueuePacketInternal(
    std::unique_ptr<RtpPacketToSend> packet,
    int priority) {
  prober_.OnIncomingPacket(DataSize::Bytes(packet->payload_size()));

  Timestamp now = CurrentTime();

  if (mode_ == ProcessMode::kDynamic && packet_queue_.Empty()) {
    // If queue is empty, we need to "fast-forward" the last process time,
    // so that we don't use passed time as budget for sending the first new
    // packet.
    Timestamp target_process_time = now;
    Timestamp next_send_time = NextSendTime();
    if (next_send_time.IsFinite()) {
      // There was already a valid planned send time, such as a keep-alive.
      // Use that as last process time only if it's prior to now.
      target_process_time = std::min(now, next_send_time);
    }
    UpdateBudgetWithElapsedTime(UpdateTimeAndGetElapsed(target_process_time));
  }
  packet_queue_.Push(priority, now, packet_counter_++, std::move(packet));
}

TimeDelta PacingController::UpdateTimeAndGetElapsed(Timestamp now) {
  // If no previous processing, or last process was "in the future" because of
  // early probe processing, then there is no elapsed time to add budget for.
  if (last_process_time_.IsMinusInfinity() || now < last_process_time_) {
    return TimeDelta::Zero();
  }
  TimeDelta elapsed_time = now - last_process_time_;
  last_process_time_ = now;
  if (elapsed_time > kMaxElapsedTime) {
    RTC_LOG(LS_WARNING) << "Elapsed time (" << elapsed_time.ms()
                        << " ms) longer than expected, limiting to "
                        << kMaxElapsedTime.ms();
    elapsed_time = kMaxElapsedTime;
  }
  return elapsed_time;
}

bool PacingController::ShouldSendKeepalive(Timestamp now) const {
  if (send_padding_if_silent_ || paused_ || congested_ ||
      packet_counter_ == 0) {
    // We send a padding packet every 500 ms to ensure we won't get stuck in
    // congested state due to no feedback being received.
    if (now - last_send_time_ >= kCongestedPacketInterval) {
      return true;
    }
  }
  return false;
}

Timestamp PacingController::NextSendTime() const {
  const Timestamp now = CurrentTime();
  Timestamp next_send_time = Timestamp::PlusInfinity();

  if (paused_) {
    return last_send_time_ + kPausedProcessInterval;
  }

  // If probing is active, that always takes priority.
  if (prober_.is_probing() && !probing_send_failure_) {
    Timestamp probe_time = prober_.NextProbeTime(now);
    if (!probe_time.IsPlusInfinity()) {
      return probe_time.IsMinusInfinity() ? now : probe_time;
    }
  }

  if (mode_ == ProcessMode::kPeriodic) {
    // In periodic non-probing mode, we just have a fixed interval.
    return last_process_time_ + min_packet_limit_;
  }

  // In dynamic mode, figure out when the next packet should be sent,
  // given the current conditions.

  // Not pacing audio, if leading packet is audio its target send
  // time is the time at which it was enqueued.
  absl::optional<Timestamp> unpaced_audio_time =
      pace_audio_ ? absl::nullopt
                  : packet_queue_.LeadingAudioPacketEnqueueTime();
  if (unpaced_audio_time) {
    return *unpaced_audio_time;
  }

  if (congested_ || packet_counter_ == 0) {
    // We need to at least send keep-alive packets with some interval.
    return last_send_time_ + kCongestedPacketInterval;
  }

  if (media_rate_ > DataRate::Zero() && !packet_queue_.Empty()) {
    // Check how long until we can send the next media packet.
    next_send_time = last_process_time_ + media_debt_ / media_rate_;
  } else if (padding_rate_ > DataRate::Zero() && packet_queue_.Empty()) {
    // If we _don't_ have pending packets, check how long until we have
    // bandwidth for padding packets. Both media and padding debts must
    // have been drained to do this.
    RTC_DCHECK_GT(media_rate_, DataRate::Zero());
    TimeDelta drain_time =
        std::max(media_debt_ / media_rate_, padding_debt_ / padding_rate_);

    if (drain_time.IsZero() &&
        (!media_debt_.IsZero() || !padding_debt_.IsZero())) {
      // We have a non-zero debt, but drain time is smaller than tick size of
      // TimeDelta, round it up to the smallest possible non-zero delta.
      drain_time = TimeDelta::Micros(1);
    }
    next_send_time = last_process_time_ + drain_time;
  } else {
    // Nothing to do.
    next_send_time = last_process_time_ + kPausedProcessInterval;
  }

  if (send_padding_if_silent_) {
    next_send_time =
        std::min(next_send_time, last_send_time_ + kPausedProcessInterval);
  }

  return next_send_time;
}

void PacingController::ProcessPackets() {
  Timestamp now = CurrentTime();
  Timestamp target_send_time = now;

  if (ShouldSendKeepalive(now)) {
    DataSize keepalive_data_sent = DataSize::Zero();
    // We can not send padding unless a normal packet has first been sent. If
    // we do, timestamps get messed up.
    if (packet_counter_ > 0) {
      std::vector<std::unique_ptr<RtpPacketToSend>> keepalive_packets =
          packet_sender_->GeneratePadding(DataSize::Bytes(1));
      for (auto& packet : keepalive_packets) {
        keepalive_data_sent +=
            DataSize::Bytes(packet->payload_size() + packet->padding_size());
        packet_sender_->SendPacket(std::move(packet), PacedPacketInfo());
        for (auto& packet : packet_sender_->FetchFec()) {
          EnqueuePacket(std::move(packet));
        }
      }
    }
    OnPacketSent(RtpPacketMediaType::kPadding, keepalive_data_sent, now);
  }

  if (paused_) {
    return;
  }

  if (mode_ == ProcessMode::kDynamic) {
    TimeDelta early_execute_margin =
        prober_.is_probing() ? kMaxEarlyProbeProcessing : TimeDelta::Zero();

    target_send_time = NextSendTime();
    if (now + early_execute_margin < target_send_time) {
      // We are too early, but if queue is empty still allow draining some debt.
      // Probing is allowed to be sent up to kMinSleepTime early.
      UpdateBudgetWithElapsedTime(UpdateTimeAndGetElapsed(now));
      return;
    }
  }

  TimeDelta elapsed_time = UpdateTimeAndGetElapsed(target_send_time);

  if (elapsed_time > TimeDelta::Zero()) {
    DataRate target_rate = pacing_bitrate_;
    DataSize queue_size_data = packet_queue_.Size();
    if (queue_size_data > DataSize::Zero()) {
      // Assuming equal size packets and input/output rate, the average packet
      // has avg_time_left_ms left to get queue_size_bytes out of the queue, if
      // time constraint shall be met. Determine bitrate needed for that.
      packet_queue_.UpdateQueueTime(now);
      if (drain_large_queues_) {
        TimeDelta avg_time_left =
            std::max(TimeDelta::Millis(1),
                     queue_time_limit_ - packet_queue_.AverageQueueTime());
        DataRate min_rate_needed = queue_size_data / avg_time_left;
        if (min_rate_needed > target_rate) {
          target_rate = min_rate_needed;
          RTC_LOG(LS_VERBOSE) << "bwe:large_pacing_queue pacing_rate_kbps="
                              << target_rate.kbps();
        }
      }
    }

    if (mode_ == ProcessMode::kPeriodic) {
      // In periodic processing mode, the IntevalBudget allows positive budget
      // up to (process interval duration) * (target rate), so we only need to
      // update it once before the packet sending loop.
      media_budget_.set_target_rate_kbps(target_rate.kbps());
    } else {
      media_rate_ = target_rate;
    }
    UpdateBudgetWithElapsedTime(elapsed_time);
  }

  PacedPacketInfo pacing_info;
  DataSize recommended_probe_size = DataSize::Zero();
  bool is_probing = prober_.is_probing();
  if (is_probing) {
    // Probe timing is sensitive, and handled explicitly by BitrateProber, so
    // use actual send time rather than target.
    pacing_info = prober_.CurrentCluster(now).value_or(PacedPacketInfo());
    if (pacing_info.probe_cluster_id != PacedPacketInfo::kNotAProbe) {
      recommended_probe_size = prober_.RecommendedMinProbeSize();
      RTC_DCHECK_GT(recommended_probe_size, DataSize::Zero());
    } else {
      // No valid probe cluster returned, probe might have timed out.
      is_probing = false;
    }
  }

  DataSize data_sent = DataSize::Zero();
  // Circuit breaker, making sure main loop isn't forever.
  static constexpr int kMaxIterations = 1 << 16;
  int iteration = 0;
  int packets_sent = 0;
  int padding_packets_generated = 0;
  for (; iteration < kMaxIterations; ++iteration) {
    // Fetch packet, so long as queue is not empty or budget is not
    // exhausted.
    std::unique_ptr<RtpPacketToSend> rtp_packet =
        GetPendingPacket(pacing_info, target_send_time, now);
    if (rtp_packet == nullptr) {
      // No packet available to send, check if we should send padding.
      DataSize padding_to_add = PaddingToAdd(recommended_probe_size, data_sent);
      if (padding_to_add > DataSize::Zero()) {
        std::vector<std::unique_ptr<RtpPacketToSend>> padding_packets =
            packet_sender_->GeneratePadding(padding_to_add);
        if (!padding_packets.empty()) {
          padding_packets_generated += padding_packets.size();
          for (auto& packet : padding_packets) {
            EnqueuePacket(std::move(packet));
          }
          // Continue loop to send the padding that was just added.
          continue;
        } else {
          // Can't generate padding, still update padding budget for next send
          // time.
          UpdatePaddingBudgetWithSentData(padding_to_add);
        }
      }
      // Can't fetch new packet and no padding to send, exit send loop.
      break;
    } else {
      RTC_DCHECK(rtp_packet);
      RTC_DCHECK(rtp_packet->packet_type().has_value());
      const RtpPacketMediaType packet_type = *rtp_packet->packet_type();
      DataSize packet_size = DataSize::Bytes(rtp_packet->payload_size() +
                                             rtp_packet->padding_size());

      if (include_overhead_) {
        packet_size += DataSize::Bytes(rtp_packet->headers_size()) +
                       transport_overhead_per_packet_;
      }

      packet_sender_->SendPacket(std::move(rtp_packet), pacing_info);
      for (auto& packet : packet_sender_->FetchFec()) {
        EnqueuePacket(std::move(packet));
      }
      data_sent += packet_size;
      ++packets_sent;

      // Send done, update send time.
      OnPacketSent(packet_type, packet_size, now);

      if (is_probing) {
        pacing_info.probe_cluster_bytes_sent += packet_size.bytes();
        // If we are currently probing, we need to stop the send loop when we
        // have reached the send target.
        if (data_sent >= recommended_probe_size) {
          break;
        }
      }

      // Update target send time in case that are more packets that we are late
      // in processing.
      if (mode_ == ProcessMode::kDynamic) {
        target_send_time = NextSendTime();
        if (target_send_time > now) {
          // Exit loop if not probing.
          if (!is_probing) {
            break;
          }
          target_send_time = now;
        }
        UpdateBudgetWithElapsedTime(UpdateTimeAndGetElapsed(target_send_time));
      }
    }
  }

  if (iteration >= kMaxIterations) {
    // Circuit break activated. Log warning, adjust send time and return.
    // TODO(sprang): Consider completely clearing state.
    RTC_LOG(LS_ERROR) << "PacingController exceeded max iterations in "
                         "send-loop: packets sent = "
                      << packets_sent << ", padding packets generated = "
                      << padding_packets_generated
                      << ", bytes sent = " << data_sent.bytes();
    last_send_time_ = now;
    last_process_time_ = now;
    return;
  }

  if (is_probing) {
    probing_send_failure_ = data_sent == DataSize::Zero();
    if (!probing_send_failure_) {
      prober_.ProbeSent(CurrentTime(), data_sent);
    }
  }
}

DataSize PacingController::PaddingToAdd(DataSize recommended_probe_size,
                                        DataSize data_sent) const {
  if (!packet_queue_.Empty()) {
    // Actual payload available, no need to add padding.
    return DataSize::Zero();
  }

  if (congested_) {
    // Don't add padding if congested, even if requested for probing.
    return DataSize::Zero();
  }

  if (packet_counter_ == 0) {
    // We can not send padding unless a normal packet has first been sent. If
    // we do, timestamps get messed up.
    return DataSize::Zero();
  }

  if (!recommended_probe_size.IsZero()) {
    if (recommended_probe_size > data_sent) {
      return recommended_probe_size - data_sent;
    }
    return DataSize::Zero();
  }

  if (mode_ == ProcessMode::kPeriodic) {
    return DataSize::Bytes(padding_budget_.bytes_remaining());
  } else if (padding_rate_ > DataRate::Zero() &&
             padding_debt_ == DataSize::Zero()) {
    return padding_target_duration_ * padding_rate_;
  }
  return DataSize::Zero();
}

std::unique_ptr<RtpPacketToSend> PacingController::GetPendingPacket(
    const PacedPacketInfo& pacing_info,
    Timestamp target_send_time,
    Timestamp now) {
  const bool is_probe =
      pacing_info.probe_cluster_id != PacedPacketInfo::kNotAProbe;
  // If first packet in probe, insert a small padding packet so we have a
  // more reliable start window for the rate estimation.
  if (is_probe && pacing_info.probe_cluster_bytes_sent == 0) {
    auto padding = packet_sender_->GeneratePadding(DataSize::Bytes(1));
    // If no RTP modules sending media are registered, we may not get a
    // padding packet back.
    if (!padding.empty()) {
      // We should never get more than one padding packets with a requested
      // size of 1 byte.
      RTC_DCHECK_EQ(padding.size(), 1u);
      return std::move(padding[0]);
    }
  }

  if (packet_queue_.Empty()) {
    return nullptr;
  }

  // First, check if there is any reason _not_ to send the next queued packet.

  // Unpaced audio packets and probes are exempted from send checks.
  bool unpaced_audio_packet =
      !pace_audio_ && packet_queue_.LeadingAudioPacketEnqueueTime().has_value();
  if (!unpaced_audio_packet && !is_probe) {
    if (congested_) {
      // Don't send anything if congested.
      return nullptr;
    }

    if (mode_ == ProcessMode::kPeriodic) {
      if (media_budget_.bytes_remaining() <= 0) {
        // Not enough budget.
        return nullptr;
      }
    } else {
      // Dynamic processing mode.
      if (now <= target_send_time) {
        // We allow sending slightly early if we think that we would actually
        // had been able to, had we been right on time - i.e. the current debt
        // is not more than would be reduced to zero at the target sent time.
        TimeDelta flush_time = media_debt_ / media_rate_;
        if (now + flush_time > target_send_time) {
          return nullptr;
        }
      }
    }
  }

  return packet_queue_.Pop();
}

void PacingController::OnPacketSent(RtpPacketMediaType packet_type,
                                    DataSize packet_size,
                                    Timestamp send_time) {
  if (!first_sent_packet_time_ && packet_type != RtpPacketMediaType::kPadding) {
    first_sent_packet_time_ = send_time;
  }

  bool audio_packet = packet_type == RtpPacketMediaType::kAudio;
  if ((!audio_packet || account_for_audio_) && packet_size > DataSize::Zero()) {
    UpdateBudgetWithSentData(packet_size);
  }

  last_send_time_ = send_time;
}

void PacingController::UpdateBudgetWithElapsedTime(TimeDelta delta) {
  if (mode_ == ProcessMode::kPeriodic) {
    delta = std::min(kMaxProcessingInterval, delta);
    media_budget_.IncreaseBudget(delta.ms());
    padding_budget_.IncreaseBudget(delta.ms());
  } else {
    media_debt_ -= std::min(media_debt_, media_rate_ * delta);
    padding_debt_ -= std::min(padding_debt_, padding_rate_ * delta);
  }
}

void PacingController::UpdateBudgetWithSentData(DataSize size) {
  if (mode_ == ProcessMode::kPeriodic) {
    media_budget_.UseBudget(size.bytes());
  } else {
    media_debt_ += size;
    media_debt_ = std::min(media_debt_, media_rate_ * kMaxDebtInTime);
  }
  UpdatePaddingBudgetWithSentData(size);
}

void PacingController::UpdatePaddingBudgetWithSentData(DataSize size) {
  if (mode_ == ProcessMode::kPeriodic) {
    padding_budget_.UseBudget(size.bytes());
  } else {
    padding_debt_ += size;
    padding_debt_ = std::min(padding_debt_, padding_rate_ * kMaxDebtInTime);
  }
}

void PacingController::SetQueueTimeLimit(TimeDelta limit) {
  queue_time_limit_ = limit;
}

}  // namespace webrtc
