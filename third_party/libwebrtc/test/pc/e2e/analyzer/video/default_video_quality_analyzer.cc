/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "api/array_view.h"
#include "api/numerics/samples_stats_counter.h"
#include "api/units/time_delta.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "rtc_base/logging.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/time_utils.h"
#include "rtc_tools/frame_analyzer/video_geometry_aligner.h"
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_frames_comparator.h"
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_internal_shared_objects.h"
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_shared_objects.h"

namespace webrtc {
namespace {

constexpr int kMicrosPerSecond = 1000000;
constexpr int kBitsInByte = 8;
constexpr absl::string_view kSkipRenderedFrameReasonProcessed = "processed";
constexpr absl::string_view kSkipRenderedFrameReasonRendered = "rendered";
constexpr absl::string_view kSkipRenderedFrameReasonDropped =
    "considered dropped";

void LogFrameCounters(const std::string& name, const FrameCounters& counters) {
  RTC_LOG(LS_INFO) << "[" << name << "] Captured    : " << counters.captured;
  RTC_LOG(LS_INFO) << "[" << name << "] Pre encoded : " << counters.pre_encoded;
  RTC_LOG(LS_INFO) << "[" << name << "] Encoded     : " << counters.encoded;
  RTC_LOG(LS_INFO) << "[" << name << "] Received    : " << counters.received;
  RTC_LOG(LS_INFO) << "[" << name << "] Decoded     : " << counters.decoded;
  RTC_LOG(LS_INFO) << "[" << name << "] Rendered    : " << counters.rendered;
  RTC_LOG(LS_INFO) << "[" << name << "] Dropped     : " << counters.dropped;
}

absl::string_view ToString(FrameDropPhase phase) {
  switch (phase) {
    case FrameDropPhase::kBeforeEncoder:
      return "kBeforeEncoder";
    case FrameDropPhase::kByEncoder:
      return "kByEncoder";
    case FrameDropPhase::kTransport:
      return "kTransport";
    case FrameDropPhase::kAfterDecoder:
      return "kAfterDecoder";
    case FrameDropPhase::kLastValue:
      RTC_CHECK(false) << "FrameDropPhase::kLastValue mustn't be used";
  }
}

void LogStreamInternalStats(const std::string& name,
                            const StreamStats& stats,
                            Timestamp start_time) {
  for (const auto& entry : stats.dropped_by_phase) {
    RTC_LOG(LS_INFO) << "[" << name << "] Dropped at " << ToString(entry.first)
                     << ": " << entry.second;
  }
  Timestamp first_encoded_frame_time = Timestamp::PlusInfinity();
  for (const StreamCodecInfo& encoder : stats.encoders) {
    RTC_DCHECK(encoder.switched_on_at.IsFinite());
    RTC_DCHECK(encoder.switched_from_at.IsFinite());
    if (first_encoded_frame_time.IsInfinite()) {
      first_encoded_frame_time = encoder.switched_on_at;
    }
    RTC_LOG(LS_INFO)
        << "[" << name << "] Used encoder: \"" << encoder.codec_name
        << "\" used from (frame_id=" << encoder.first_frame_id
        << "; from_stream_start="
        << (encoder.switched_on_at - stats.stream_started_time).ms()
        << "ms, from_call_start=" << (encoder.switched_on_at - start_time).ms()
        << "ms) until (frame_id=" << encoder.last_frame_id
        << "; from_stream_start="
        << (encoder.switched_from_at - stats.stream_started_time).ms()
        << "ms, from_call_start="
        << (encoder.switched_from_at - start_time).ms() << "ms)";
  }
  for (const StreamCodecInfo& decoder : stats.decoders) {
    RTC_DCHECK(decoder.switched_on_at.IsFinite());
    RTC_DCHECK(decoder.switched_from_at.IsFinite());
    RTC_LOG(LS_INFO)
        << "[" << name << "] Used decoder: \"" << decoder.codec_name
        << "\" used from (frame_id=" << decoder.first_frame_id
        << "; from_stream_start="
        << (decoder.switched_on_at - stats.stream_started_time).ms()
        << "ms, from_call_start=" << (decoder.switched_on_at - start_time).ms()
        << "ms) until (frame_id=" << decoder.last_frame_id
        << "; from_stream_start="
        << (decoder.switched_from_at - stats.stream_started_time).ms()
        << "ms, from_call_start="
        << (decoder.switched_from_at - start_time).ms() << "ms)";
  }
}

template <typename T>
absl::optional<T> MaybeGetValue(const std::map<size_t, T>& map, size_t key) {
  auto it = map.find(key);
  if (it == map.end()) {
    return absl::nullopt;
  }
  return it->second;
}

SamplesStatsCounter::StatsSample StatsSample(double value,
                                             Timestamp sampling_time) {
  return SamplesStatsCounter::StatsSample{value, sampling_time};
}

}  // namespace

DefaultVideoQualityAnalyzer::DefaultVideoQualityAnalyzer(
    webrtc::Clock* clock,
    DefaultVideoQualityAnalyzerOptions options)
    : options_(options),
      clock_(clock),
      frames_comparator_(clock, cpu_measurer_, options) {}
DefaultVideoQualityAnalyzer::~DefaultVideoQualityAnalyzer() {
  Stop();
}

void DefaultVideoQualityAnalyzer::Start(
    std::string test_case_name,
    rtc::ArrayView<const std::string> peer_names,
    int max_threads_count) {
  test_label_ = std::move(test_case_name);
  frames_comparator_.Start(max_threads_count);
  {
    MutexLock lock(&mutex_);
    peers_ = std::make_unique<NamesCollection>(peer_names);
    RTC_CHECK(start_time_.IsMinusInfinity());

    RTC_CHECK_EQ(state_, State::kNew)
        << "DefaultVideoQualityAnalyzer is already started";
    state_ = State::kActive;
    start_time_ = Now();
  }
}

uint16_t DefaultVideoQualityAnalyzer::OnFrameCaptured(
    absl::string_view peer_name,
    const std::string& stream_label,
    const webrtc::VideoFrame& frame) {
  // `next_frame_id` is atomic, so we needn't lock here.
  Timestamp captured_time = Now();
  Timestamp start_time = Timestamp::MinusInfinity();
  size_t peer_index = -1;
  size_t peers_count = -1;
  size_t stream_index;
  uint16_t frame_id = VideoFrame::kNotSetId;
  {
    MutexLock lock(&mutex_);
    frame_id = GetNextFrameId();
    RTC_CHECK_EQ(state_, State::kActive)
        << "DefaultVideoQualityAnalyzer has to be started before use";
    // Create a local copy of `start_time_`, peer's index and total peers count
    // to access it without holding a `mutex_` during access to
    // `frames_comparator_`.
    start_time = start_time_;
    peer_index = peers_->index(peer_name);
    peers_count = peers_->size();
    stream_index = streams_.AddIfAbsent(stream_label);
  }
  // Ensure stats for this stream exists.
  frames_comparator_.EnsureStatsForStream(stream_index, peer_index, peers_count,
                                          captured_time, start_time);
  {
    MutexLock lock(&mutex_);
    stream_to_sender_[stream_index] = peer_index;
    frame_counters_.captured++;
    for (size_t i = 0; i < peers_->size(); ++i) {
      if (i != peer_index || options_.enable_receive_own_stream) {
        InternalStatsKey key(stream_index, peer_index, i);
        stream_frame_counters_[key].captured++;
      }
    }

    auto state_it = stream_states_.find(stream_index);
    if (state_it == stream_states_.end()) {
      stream_states_.emplace(
          stream_index,
          StreamState(peer_index, peers_->size(),
                      options_.enable_receive_own_stream, captured_time));
    }
    StreamState* state = &stream_states_.at(stream_index);
    state->PushBack(frame_id);
    // Update frames in flight info.
    auto it = captured_frames_in_flight_.find(frame_id);
    if (it != captured_frames_in_flight_.end()) {
      // If we overflow uint16_t and hit previous frame id and this frame is
      // still in flight, it means that this stream wasn't rendered for long
      // time and we need to process existing frame as dropped.
      for (size_t i = 0; i < peers_->size(); ++i) {
        if (i == peer_index && !options_.enable_receive_own_stream) {
          continue;
        }

        uint16_t oldest_frame_id = state->PopFront(i);
        RTC_DCHECK_EQ(frame_id, oldest_frame_id);
        frame_counters_.dropped++;
        InternalStatsKey key(stream_index, peer_index, i);
        stream_frame_counters_.at(key).dropped++;

        analyzer_stats_.frames_in_flight_left_count.AddSample(
            StatsSample(captured_frames_in_flight_.size(), Now()));
        frames_comparator_.AddComparison(
            InternalStatsKey(stream_index, peer_index, i),
            /*captured=*/absl::nullopt,
            /*rendered=*/absl::nullopt, FrameComparisonType::kDroppedFrame,
            it->second.GetStatsForPeer(i));
      }

      captured_frames_in_flight_.erase(it);
    }
    captured_frames_in_flight_.emplace(
        frame_id,
        FrameInFlight(stream_index, frame, captured_time, peer_index,
                      peers_->size(), options_.enable_receive_own_stream));
    // Set frame id on local copy of the frame
    captured_frames_in_flight_.at(frame_id).SetFrameId(frame_id);

    // Update history stream<->frame mapping
    for (auto it = stream_to_frame_id_history_.begin();
         it != stream_to_frame_id_history_.end(); ++it) {
      it->second.erase(frame_id);
    }
    stream_to_frame_id_history_[stream_index].insert(frame_id);
    stream_to_frame_id_full_history_[stream_index].push_back(frame_id);

    // If state has too many frames that are in flight => remove the oldest
    // queued frame in order to avoid to use too much memory.
    if (state->GetAliveFramesCount() >
        options_.max_frames_in_flight_per_stream_count) {
      uint16_t frame_id_to_remove = state->MarkNextAliveFrameAsDead();
      auto it = captured_frames_in_flight_.find(frame_id_to_remove);
      RTC_CHECK(it != captured_frames_in_flight_.end())
          << "Frame with ID " << frame_id_to_remove
          << " is expected to be in flight, but hasn't been found in "
          << "|captured_frames_in_flight_|";
      bool is_removed = it->second.RemoveFrame();
      RTC_DCHECK(is_removed)
          << "Invalid stream state: alive frame is removed already";
    }
  }
  return frame_id;
}

void DefaultVideoQualityAnalyzer::OnFramePreEncode(
    absl::string_view peer_name,
    const webrtc::VideoFrame& frame) {
  MutexLock lock(&mutex_);
  RTC_CHECK_EQ(state_, State::kActive)
      << "DefaultVideoQualityAnalyzer has to be started before use";

  auto it = captured_frames_in_flight_.find(frame.id());
  RTC_DCHECK(it != captured_frames_in_flight_.end())
      << "Frame id=" << frame.id() << " not found";
  frame_counters_.pre_encoded++;
  size_t peer_index = peers_->index(peer_name);
  for (size_t i = 0; i < peers_->size(); ++i) {
    if (i != peer_index || options_.enable_receive_own_stream) {
      InternalStatsKey key(it->second.stream(), peer_index, i);
      stream_frame_counters_.at(key).pre_encoded++;
    }
  }
  it->second.SetPreEncodeTime(Now());
}

void DefaultVideoQualityAnalyzer::OnFrameEncoded(
    absl::string_view peer_name,
    uint16_t frame_id,
    const webrtc::EncodedImage& encoded_image,
    const EncoderStats& stats) {
  MutexLock lock(&mutex_);
  RTC_CHECK_EQ(state_, State::kActive)
      << "DefaultVideoQualityAnalyzer has to be started before use";

  auto it = captured_frames_in_flight_.find(frame_id);
  if (it == captured_frames_in_flight_.end()) {
    RTC_LOG(LS_WARNING)
        << "The encoding of video frame with id [" << frame_id << "] for peer ["
        << peer_name << "] finished after all receivers rendered this frame. "
        << "It can be OK for simulcast/SVC if higher quality stream is not "
        << "required, but it may indicate an ERROR for singlecast or if it "
        << "happens often.";
    return;
  }
  // For SVC we can receive multiple encoded images for one frame, so to cover
  // all cases we have to pick the last encode time.
  if (!it->second.HasEncodedTime()) {
    // Increase counters only when we meet this frame first time.
    frame_counters_.encoded++;
    size_t peer_index = peers_->index(peer_name);
    for (size_t i = 0; i < peers_->size(); ++i) {
      if (i != peer_index || options_.enable_receive_own_stream) {
        InternalStatsKey key(it->second.stream(), peer_index, i);
        stream_frame_counters_.at(key).encoded++;
      }
    }
  }
  Timestamp now = Now();
  StreamCodecInfo used_encoder;
  used_encoder.codec_name = stats.encoder_name;
  used_encoder.first_frame_id = frame_id;
  used_encoder.last_frame_id = frame_id;
  used_encoder.switched_on_at = now;
  used_encoder.switched_from_at = now;
  it->second.OnFrameEncoded(now, encoded_image._frameType,
                            DataSize::Bytes(encoded_image.size()),
                            stats.target_encode_bitrate, used_encoder);
}

void DefaultVideoQualityAnalyzer::OnFrameDropped(
    absl::string_view peer_name,
    webrtc::EncodedImageCallback::DropReason reason) {
  // Here we do nothing, because we will see this drop on renderer side.
}

void DefaultVideoQualityAnalyzer::OnFramePreDecode(
    absl::string_view peer_name,
    uint16_t frame_id,
    const webrtc::EncodedImage& input_image) {
  MutexLock lock(&mutex_);
  RTC_CHECK_EQ(state_, State::kActive)
      << "DefaultVideoQualityAnalyzer has to be started before use";

  size_t peer_index = peers_->index(peer_name);

  if (frame_id == VideoFrame::kNotSetId) {
    frame_counters_.received++;
    unknown_sender_frame_counters_[std::string(peer_name)].received++;
    return;
  }

  auto it = captured_frames_in_flight_.find(frame_id);
  if (it == captured_frames_in_flight_.end() ||
      it->second.HasReceivedTime(peer_index)) {
    // It means this frame was predecoded before, so we can skip it. It may
    // happen when we have multiple simulcast streams in one track and received
    // the same picture from two different streams because SFU can't reliably
    // correlate two simulcast streams and started relaying the second stream
    // from the same frame it has relayed right before for the first stream.
    return;
  }

  frame_counters_.received++;
  InternalStatsKey key(it->second.stream(),
                       stream_to_sender_.at(it->second.stream()), peer_index);
  stream_frame_counters_.at(key).received++;
  // Determine the time of the last received packet of this video frame.
  RTC_DCHECK(!input_image.PacketInfos().empty());
  Timestamp last_receive_time =
      std::max_element(input_image.PacketInfos().cbegin(),
                       input_image.PacketInfos().cend(),
                       [](const RtpPacketInfo& a, const RtpPacketInfo& b) {
                         return a.receive_time() < b.receive_time();
                       })
          ->receive_time();
  it->second.OnFramePreDecode(peer_index,
                              /*received_time=*/last_receive_time,
                              /*decode_start_time=*/Now(),
                              input_image._frameType,
                              DataSize::Bytes(input_image.size()));
}

void DefaultVideoQualityAnalyzer::OnFrameDecoded(
    absl::string_view peer_name,
    const webrtc::VideoFrame& frame,
    const DecoderStats& stats) {
  MutexLock lock(&mutex_);
  RTC_CHECK_EQ(state_, State::kActive)
      << "DefaultVideoQualityAnalyzer has to be started before use";

  size_t peer_index = peers_->index(peer_name);

  if (frame.id() == VideoFrame::kNotSetId) {
    frame_counters_.decoded++;
    unknown_sender_frame_counters_[std::string(peer_name)].decoded++;
    return;
  }

  auto it = captured_frames_in_flight_.find(frame.id());
  if (it == captured_frames_in_flight_.end() ||
      it->second.HasDecodeEndTime(peer_index)) {
    // It means this frame was decoded before, so we can skip it. It may happen
    // when we have multiple simulcast streams in one track and received
    // the same picture from two different streams because SFU can't reliably
    // correlate two simulcast streams and started relaying the second stream
    // from the same frame it has relayed right before for the first stream.
    return;
  }
  frame_counters_.decoded++;
  InternalStatsKey key(it->second.stream(),
                       stream_to_sender_.at(it->second.stream()), peer_index);
  stream_frame_counters_.at(key).decoded++;
  Timestamp now = Now();
  StreamCodecInfo used_decoder;
  used_decoder.codec_name = stats.decoder_name;
  used_decoder.first_frame_id = frame.id();
  used_decoder.last_frame_id = frame.id();
  used_decoder.switched_on_at = now;
  used_decoder.switched_from_at = now;
  it->second.OnFrameDecoded(peer_index, now, used_decoder);
}

void DefaultVideoQualityAnalyzer::OnFrameRendered(
    absl::string_view peer_name,
    const webrtc::VideoFrame& frame) {
  MutexLock lock(&mutex_);
  RTC_CHECK_EQ(state_, State::kActive)
      << "DefaultVideoQualityAnalyzer has to be started before use";

  size_t peer_index = peers_->index(peer_name);

  if (frame.id() == VideoFrame::kNotSetId) {
    frame_counters_.rendered++;
    unknown_sender_frame_counters_[std::string(peer_name)].rendered++;
    return;
  }

  auto frame_it = captured_frames_in_flight_.find(frame.id());
  if (frame_it == captured_frames_in_flight_.end() ||
      frame_it->second.HasRenderedTime(peer_index) ||
      frame_it->second.IsDropped(peer_index)) {
    // It means this frame was rendered or dropped before, so we can skip it.
    // It may happen when we have multiple simulcast streams in one track and
    // received the same frame from two different streams because SFU can't
    // reliably correlate two simulcast streams and started relaying the second
    // stream from the same frame it has relayed right before for the first
    // stream.
    absl::string_view reason = kSkipRenderedFrameReasonProcessed;
    if (frame_it != captured_frames_in_flight_.end()) {
      if (frame_it->second.HasRenderedTime(peer_index)) {
        reason = kSkipRenderedFrameReasonRendered;
      } else if (frame_it->second.IsDropped(peer_index)) {
        reason = kSkipRenderedFrameReasonDropped;
      }
    }
    RTC_LOG(LS_WARNING)
        << "Peer " << peer_name
        << "; Received frame out of order: received frame with id "
        << frame.id() << " which was " << reason << " before";
    return;
  }

  // Find corresponding captured frame.
  FrameInFlight* frame_in_flight = &frame_it->second;
  absl::optional<VideoFrame> captured_frame = frame_in_flight->frame();

  const size_t stream_index = frame_in_flight->stream();
  StreamState* state = &stream_states_.at(stream_index);
  const InternalStatsKey stats_key(stream_index, state->owner(), peer_index);

  // Update frames counters.
  frame_counters_.rendered++;
  stream_frame_counters_.at(stats_key).rendered++;

  // Update current frame stats.
  frame_in_flight->OnFrameRendered(peer_index, Now(), frame.width(),
                                   frame.height());

  // After we received frame here we need to check if there are any dropped
  // frames between this one and last one, that was rendered for this video
  // stream.
  int dropped_count = 0;
  while (!state->IsEmpty(peer_index) &&
         state->Front(peer_index) != frame.id()) {
    dropped_count++;
    uint16_t dropped_frame_id = state->PopFront(peer_index);
    // Frame with id `dropped_frame_id` was dropped. We need:
    // 1. Update global and stream frame counters
    // 2. Extract corresponding frame from `captured_frames_in_flight_`
    // 3. Send extracted frame to comparison with dropped=true
    // 4. Cleanup dropped frame
    frame_counters_.dropped++;
    stream_frame_counters_.at(stats_key).dropped++;

    auto dropped_frame_it = captured_frames_in_flight_.find(dropped_frame_id);
    RTC_DCHECK(dropped_frame_it != captured_frames_in_flight_.end());
    absl::optional<VideoFrame> dropped_frame = dropped_frame_it->second.frame();
    dropped_frame_it->second.MarkDropped(peer_index);

    analyzer_stats_.frames_in_flight_left_count.AddSample(
        StatsSample(captured_frames_in_flight_.size(), Now()));
    frames_comparator_.AddComparison(
        stats_key, /*captured=*/absl::nullopt, /*rendered=*/absl::nullopt,
        FrameComparisonType::kDroppedFrame,
        dropped_frame_it->second.GetStatsForPeer(peer_index));

    if (dropped_frame_it->second.HaveAllPeersReceived()) {
      captured_frames_in_flight_.erase(dropped_frame_it);
    }
  }
  RTC_DCHECK(!state->IsEmpty(peer_index));
  state->PopFront(peer_index);

  if (state->last_rendered_frame_time(peer_index)) {
    frame_in_flight->SetPrevFrameRenderedTime(
        peer_index, state->last_rendered_frame_time(peer_index).value());
  }
  state->SetLastRenderedFrameTime(peer_index,
                                  frame_in_flight->rendered_time(peer_index));
  analyzer_stats_.frames_in_flight_left_count.AddSample(
      StatsSample(captured_frames_in_flight_.size(), Now()));
  frames_comparator_.AddComparison(
      stats_key, dropped_count, captured_frame, /*rendered=*/frame,
      FrameComparisonType::kRegular,
      frame_in_flight->GetStatsForPeer(peer_index));

  if (frame_it->second.HaveAllPeersReceived()) {
    captured_frames_in_flight_.erase(frame_it);
  }
}

void DefaultVideoQualityAnalyzer::OnEncoderError(
    absl::string_view peer_name,
    const webrtc::VideoFrame& frame,
    int32_t error_code) {
  RTC_LOG(LS_ERROR) << "Encoder error for frame.id=" << frame.id()
                    << ", code=" << error_code;
}

void DefaultVideoQualityAnalyzer::OnDecoderError(absl::string_view peer_name,
                                                 uint16_t frame_id,
                                                 int32_t error_code) {
  RTC_LOG(LS_ERROR) << "Decoder error for frame_id=" << frame_id
                    << ", code=" << error_code;
}

void DefaultVideoQualityAnalyzer::RegisterParticipantInCall(
    absl::string_view peer_name) {
  MutexLock lock(&mutex_);
  RTC_CHECK(!peers_->HasName(peer_name));
  size_t new_peer_index = peers_->AddIfAbsent(peer_name);

  // Ensure stats for receiving (for frames from other peers to this one)
  // streams exists. Since in flight frames will be sent to the new peer
  // as well. Sending stats (from this peer to others) will be added by
  // DefaultVideoQualityAnalyzer::OnFrameCaptured.
  std::vector<std::pair<InternalStatsKey, Timestamp>> stream_started_time;
  for (auto& key_val : stream_to_sender_) {
    size_t stream_index = key_val.first;
    size_t sender_peer_index = key_val.second;
    InternalStatsKey key(stream_index, sender_peer_index, new_peer_index);

    // To initiate `FrameCounters` for the stream we should pick frame
    // counters with the same stream index and the same sender's peer index
    // and any receiver's peer index and copy from its sender side
    // counters.
    FrameCounters counters;
    for (size_t i = 0; i < peers_->size(); ++i) {
      InternalStatsKey prototype_key(stream_index, sender_peer_index, i);
      auto it = stream_frame_counters_.find(prototype_key);
      if (it != stream_frame_counters_.end()) {
        counters.captured = it->second.captured;
        counters.pre_encoded = it->second.pre_encoded;
        counters.encoded = it->second.encoded;
        break;
      }
    }
    // It may happen if we had only one peer before this method was invoked,
    // then `counters` will be empty. In such case empty `counters` are ok.
    stream_frame_counters_.insert({key, std::move(counters)});

    stream_started_time.push_back(
        {key, stream_states_.at(stream_index).stream_started_time()});
  }
  frames_comparator_.RegisterParticipantInCall(stream_started_time,
                                               start_time_);
  // Ensure, that frames states are handled correctly
  // (e.g. dropped frames tracking).
  for (auto& key_val : stream_states_) {
    key_val.second.AddPeer();
  }
  // Register new peer for every frame in flight.
  // It is guaranteed, that no garbage FrameInFlight objects will stay in
  // memory because of adding new peer. Even if the new peer won't receive the
  // frame, the frame will be removed by OnFrameRendered after next frame comes
  // for the new peer. It is important because FrameInFlight is a large object.
  for (auto& key_val : captured_frames_in_flight_) {
    key_val.second.AddPeer();
  }
}

void DefaultVideoQualityAnalyzer::Stop() {
  std::map<InternalStatsKey, Timestamp> last_rendered_frame_times;
  {
    MutexLock lock(&mutex_);
    if (state_ == State::kStopped) {
      return;
    }
    RTC_CHECK_EQ(state_, State::kActive)
        << "DefaultVideoQualityAnalyzer has to be started before use";

    state_ = State::kStopped;

    // Add the amount of frames in flight to the analyzer stats before all left
    // frames in flight will be sent to the `frames_compartor_`.
    analyzer_stats_.frames_in_flight_left_count.AddSample(
        StatsSample(captured_frames_in_flight_.size(), Now()));

    for (auto& state_entry : stream_states_) {
      const size_t stream_index = state_entry.first;
      StreamState& stream_state = state_entry.second;
      for (size_t i = 0; i < peers_->size(); ++i) {
        if (i == stream_state.owner() && !options_.enable_receive_own_stream) {
          continue;
        }

        InternalStatsKey stats_key(stream_index, stream_state.owner(), i);

        // If there are no freezes in the call we have to report
        // time_between_freezes_ms as call duration and in such case
        // `stream_last_freeze_end_time` for this stream will be `start_time_`.
        // If there is freeze, then we need add time from last rendered frame
        // to last freeze end as time between freezes.
        if (stream_state.last_rendered_frame_time(i)) {
          last_rendered_frame_times.emplace(
              stats_key, stream_state.last_rendered_frame_time(i).value());
        }

        // Add frames in flight for this stream into frames comparator.
        // Frames in flight were not rendered, so they won't affect stream's
        // last rendered frame time.
        while (!stream_state.IsEmpty(i)) {
          uint16_t frame_id = stream_state.PopFront(i);
          auto it = captured_frames_in_flight_.find(frame_id);
          RTC_DCHECK(it != captured_frames_in_flight_.end());
          FrameInFlight& frame = it->second;

          frames_comparator_.AddComparison(
              stats_key, /*captured=*/absl::nullopt,
              /*rendered=*/absl::nullopt, FrameComparisonType::kFrameInFlight,
              frame.GetStatsForPeer(i));

          if (frame.HaveAllPeersReceived()) {
            captured_frames_in_flight_.erase(it);
          }
        }
      }
    }
  }
  frames_comparator_.Stop(last_rendered_frame_times);

  // Perform final Metrics update. On this place analyzer is stopped and no one
  // holds any locks.
  {
    MutexLock lock(&mutex_);
    FramesComparatorStats frames_comparator_stats =
        frames_comparator_.frames_comparator_stats();
    analyzer_stats_.comparisons_queue_size =
        frames_comparator_stats.comparisons_queue_size;
    analyzer_stats_.comparisons_done = frames_comparator_stats.comparisons_done;
    analyzer_stats_.cpu_overloaded_comparisons_done =
        frames_comparator_stats.cpu_overloaded_comparisons_done;
    analyzer_stats_.memory_overloaded_comparisons_done =
        frames_comparator_stats.memory_overloaded_comparisons_done;
  }
  ReportResults();
}

std::string DefaultVideoQualityAnalyzer::GetStreamLabel(uint16_t frame_id) {
  MutexLock lock1(&mutex_);
  auto it = captured_frames_in_flight_.find(frame_id);
  if (it != captured_frames_in_flight_.end()) {
    return streams_.name(it->second.stream());
  }
  for (auto hist_it = stream_to_frame_id_history_.begin();
       hist_it != stream_to_frame_id_history_.end(); ++hist_it) {
    auto hist_set_it = hist_it->second.find(frame_id);
    if (hist_set_it != hist_it->second.end()) {
      return streams_.name(hist_it->first);
    }
  }
  RTC_CHECK(false) << "Unknown frame_id=" << frame_id;
}

std::set<StatsKey> DefaultVideoQualityAnalyzer::GetKnownVideoStreams() const {
  MutexLock lock(&mutex_);
  std::set<StatsKey> out;
  for (auto& item : frames_comparator_.stream_stats()) {
    RTC_LOG(LS_INFO) << item.first.ToString() << " ==> "
                     << ToStatsKey(item.first).ToString();
    out.insert(ToStatsKey(item.first));
  }
  return out;
}

VideoStreamsInfo DefaultVideoQualityAnalyzer::GetKnownStreams() const {
  MutexLock lock(&mutex_);
  std::map<std::string, std::string> stream_to_sender;
  std::map<std::string, std::set<std::string>> sender_to_streams;
  std::map<std::string, std::set<std::string>> stream_to_receivers;

  for (auto& item : frames_comparator_.stream_stats()) {
    const std::string& stream_label = streams_.name(item.first.stream);
    const std::string& sender = peers_->name(item.first.sender);
    const std::string& receiver = peers_->name(item.first.receiver);
    RTC_LOG(LS_INFO) << item.first.ToString() << " ==> "
                     << "stream=" << stream_label << "; sender=" << sender
                     << "; receiver=" << receiver;
    stream_to_sender.emplace(stream_label, sender);
    auto streams_it = sender_to_streams.find(sender);
    if (streams_it != sender_to_streams.end()) {
      streams_it->second.emplace(stream_label);
    } else {
      sender_to_streams.emplace(sender, std::set<std::string>{stream_label});
    }
    auto receivers_it = stream_to_receivers.find(stream_label);
    if (receivers_it != stream_to_receivers.end()) {
      receivers_it->second.emplace(receiver);
    } else {
      stream_to_receivers.emplace(stream_label,
                                  std::set<std::string>{receiver});
    }
  }

  return VideoStreamsInfo(std::move(stream_to_sender),
                          std::move(sender_to_streams),
                          std::move(stream_to_receivers));
}

FrameCounters DefaultVideoQualityAnalyzer::GetGlobalCounters() const {
  MutexLock lock(&mutex_);
  return frame_counters_;
}

std::map<std::string, FrameCounters>
DefaultVideoQualityAnalyzer::GetUnknownSenderFrameCounters() const {
  MutexLock lock(&mutex_);
  return unknown_sender_frame_counters_;
}

std::map<StatsKey, FrameCounters>
DefaultVideoQualityAnalyzer::GetPerStreamCounters() const {
  MutexLock lock(&mutex_);
  std::map<StatsKey, FrameCounters> out;
  for (auto& item : stream_frame_counters_) {
    out.emplace(ToStatsKey(item.first), item.second);
  }
  return out;
}

std::map<StatsKey, StreamStats> DefaultVideoQualityAnalyzer::GetStats() const {
  MutexLock lock1(&mutex_);
  std::map<StatsKey, StreamStats> out;
  for (auto& item : frames_comparator_.stream_stats()) {
    out.emplace(ToStatsKey(item.first), item.second);
  }
  return out;
}

AnalyzerStats DefaultVideoQualityAnalyzer::GetAnalyzerStats() const {
  MutexLock lock(&mutex_);
  return analyzer_stats_;
}

uint16_t DefaultVideoQualityAnalyzer::GetNextFrameId() {
  uint16_t frame_id = next_frame_id_++;
  if (next_frame_id_ == VideoFrame::kNotSetId) {
    next_frame_id_ = 1;
  }
  return frame_id;
}

void DefaultVideoQualityAnalyzer::ReportResults() {
  using ::webrtc::test::ImproveDirection;

  MutexLock lock(&mutex_);
  for (auto& item : frames_comparator_.stream_stats()) {
    ReportResults(GetTestCaseName(ToMetricName(item.first)), item.second,
                  stream_frame_counters_.at(item.first));
  }
  test::PrintResult("cpu_usage", "", test_label_.c_str(), GetCpuUsagePercent(),
                    "%", false, ImproveDirection::kSmallerIsBetter);
  LogFrameCounters("Global", frame_counters_);
  if (!unknown_sender_frame_counters_.empty()) {
    RTC_LOG(LS_INFO) << "Received frame counters with unknown frame id:";
    for (const auto& [peer_name, frame_counters] :
         unknown_sender_frame_counters_) {
      LogFrameCounters(peer_name, frame_counters);
    }
  }
  RTC_LOG(LS_INFO) << "Received frame counters per stream:";
  for (const auto& [stats_key, stream_stats] :
       frames_comparator_.stream_stats()) {
    LogFrameCounters(ToStatsKey(stats_key).ToString(),
                     stream_frame_counters_.at(stats_key));
    LogStreamInternalStats(ToStatsKey(stats_key).ToString(), stream_stats,
                           start_time_);
  }
  if (!analyzer_stats_.comparisons_queue_size.IsEmpty()) {
    RTC_LOG(LS_INFO) << "comparisons_queue_size min="
                     << analyzer_stats_.comparisons_queue_size.GetMin()
                     << "; max="
                     << analyzer_stats_.comparisons_queue_size.GetMax()
                     << "; 99%="
                     << analyzer_stats_.comparisons_queue_size.GetPercentile(
                            0.99);
  }
  RTC_LOG(LS_INFO) << "comparisons_done=" << analyzer_stats_.comparisons_done;
  RTC_LOG(LS_INFO) << "cpu_overloaded_comparisons_done="
                   << analyzer_stats_.cpu_overloaded_comparisons_done;
  RTC_LOG(LS_INFO) << "memory_overloaded_comparisons_done="
                   << analyzer_stats_.memory_overloaded_comparisons_done;
}

void DefaultVideoQualityAnalyzer::ReportResults(
    const std::string& test_case_name,
    const StreamStats& stats,
    const FrameCounters& frame_counters) {
  using ::webrtc::test::ImproveDirection;
  TimeDelta test_duration = Now() - start_time_;

  double sum_squared_interframe_delays_secs = 0;
  Timestamp video_start_time = Timestamp::PlusInfinity();
  Timestamp video_end_time = Timestamp::MinusInfinity();
  for (const SamplesStatsCounter::StatsSample& sample :
       stats.time_between_rendered_frames_ms.GetTimedSamples()) {
    double interframe_delay_ms = sample.value;
    const double interframe_delays_secs = interframe_delay_ms / 1000.0;
    // Sum of squared inter frame intervals is used to calculate the harmonic
    // frame rate metric. The metric aims to reflect overall experience related
    // to smoothness of video playback and includes both freezes and pauses.
    sum_squared_interframe_delays_secs +=
        interframe_delays_secs * interframe_delays_secs;
    if (sample.time < video_start_time) {
      video_start_time = sample.time;
    }
    if (sample.time > video_end_time) {
      video_end_time = sample.time;
    }
  }
  double harmonic_framerate_fps = 0;
  TimeDelta video_duration = video_end_time - video_start_time;
  if (sum_squared_interframe_delays_secs > 0.0 && video_duration.IsFinite()) {
    harmonic_framerate_fps = static_cast<double>(video_duration.us()) /
                             static_cast<double>(kMicrosPerSecond) /
                             sum_squared_interframe_delays_secs;
  }

  ReportResult("psnr", test_case_name, stats.psnr, "dB",
               ImproveDirection::kBiggerIsBetter);
  ReportResult("ssim", test_case_name, stats.ssim, "unitless",
               ImproveDirection::kBiggerIsBetter);
  ReportResult("transport_time", test_case_name, stats.transport_time_ms, "ms",
               ImproveDirection::kSmallerIsBetter);
  ReportResult("total_delay_incl_transport", test_case_name,
               stats.total_delay_incl_transport_ms, "ms",
               ImproveDirection::kSmallerIsBetter);
  ReportResult("time_between_rendered_frames", test_case_name,
               stats.time_between_rendered_frames_ms, "ms",
               ImproveDirection::kSmallerIsBetter);
  test::PrintResult("harmonic_framerate", "", test_case_name,
                    harmonic_framerate_fps, "Hz", /*important=*/false,
                    ImproveDirection::kBiggerIsBetter);
  test::PrintResult("encode_frame_rate", "", test_case_name,
                    stats.encode_frame_rate.IsEmpty()
                        ? 0
                        : stats.encode_frame_rate.GetEventsPerSecond(),
                    "Hz", /*important=*/false,
                    ImproveDirection::kBiggerIsBetter);
  ReportResult("encode_time", test_case_name, stats.encode_time_ms, "ms",
               ImproveDirection::kSmallerIsBetter);
  ReportResult("time_between_freezes", test_case_name,
               stats.time_between_freezes_ms, "ms",
               ImproveDirection::kBiggerIsBetter);
  ReportResult("freeze_time_ms", test_case_name, stats.freeze_time_ms, "ms",
               ImproveDirection::kSmallerIsBetter);
  ReportResult("pixels_per_frame", test_case_name,
               stats.resolution_of_rendered_frame, "count",
               ImproveDirection::kBiggerIsBetter);
  test::PrintResult("min_psnr", "", test_case_name,
                    stats.psnr.IsEmpty() ? 0 : stats.psnr.GetMin(), "dB",
                    /*important=*/false, ImproveDirection::kBiggerIsBetter);
  ReportResult("decode_time", test_case_name, stats.decode_time_ms, "ms",
               ImproveDirection::kSmallerIsBetter);
  ReportResult("receive_to_render_time", test_case_name,
               stats.receive_to_render_time_ms, "ms",
               ImproveDirection::kSmallerIsBetter);
  test::PrintResult("dropped_frames", "", test_case_name,
                    frame_counters.dropped, "count",
                    /*important=*/false, ImproveDirection::kSmallerIsBetter);
  test::PrintResult("frames_in_flight", "", test_case_name,
                    frame_counters.captured - frame_counters.rendered -
                        frame_counters.dropped,
                    "count",
                    /*important=*/false, ImproveDirection::kSmallerIsBetter);
  test::PrintResult("rendered_frames", "", test_case_name,
                    frame_counters.rendered, "count", /*important=*/false,
                    ImproveDirection::kBiggerIsBetter);
  ReportResult("max_skipped", test_case_name, stats.skipped_between_rendered,
               "count", ImproveDirection::kSmallerIsBetter);
  ReportResult("target_encode_bitrate", test_case_name,
               stats.target_encode_bitrate / kBitsInByte, "bytesPerSecond",
               ImproveDirection::kNone);
  test::PrintResult(
      "actual_encode_bitrate", "", test_case_name,
      static_cast<double>(stats.total_encoded_images_payload) /
          static_cast<double>(test_duration.us()) * kMicrosPerSecond,
      "bytesPerSecond", /*important=*/false, ImproveDirection::kNone);

  if (options_.report_detailed_frame_stats) {
    test::PrintResult("num_encoded_frames", "", test_case_name,
                      frame_counters.encoded, "count",
                      /*important=*/false, ImproveDirection::kBiggerIsBetter);
    test::PrintResult("num_decoded_frames", "", test_case_name,
                      frame_counters.decoded, "count",
                      /*important=*/false, ImproveDirection::kBiggerIsBetter);
    test::PrintResult("num_send_key_frames", "", test_case_name,
                      stats.num_send_key_frames, "count",
                      /*important=*/false, ImproveDirection::kBiggerIsBetter);
    test::PrintResult("num_recv_key_frames", "", test_case_name,
                      stats.num_recv_key_frames, "count",
                      /*important=*/false, ImproveDirection::kBiggerIsBetter);

    ReportResult("recv_key_frame_size_bytes", test_case_name,
                 stats.recv_key_frame_size_bytes, "count",
                 ImproveDirection::kBiggerIsBetter);
    ReportResult("recv_delta_frame_size_bytes", test_case_name,
                 stats.recv_delta_frame_size_bytes, "count",
                 ImproveDirection::kBiggerIsBetter);
  }
}

void DefaultVideoQualityAnalyzer::ReportResult(
    const std::string& metric_name,
    const std::string& test_case_name,
    const SamplesStatsCounter& counter,
    const std::string& unit,
    webrtc::test::ImproveDirection improve_direction) {
  test::PrintResult(metric_name, /*modifier=*/"", test_case_name, counter, unit,
                    /*important=*/false, improve_direction);
}

std::string DefaultVideoQualityAnalyzer::GetTestCaseName(
    const std::string& stream_label) const {
  return test_label_ + "/" + stream_label;
}

Timestamp DefaultVideoQualityAnalyzer::Now() {
  return clock_->CurrentTime();
}

StatsKey DefaultVideoQualityAnalyzer::ToStatsKey(
    const InternalStatsKey& key) const {
  return StatsKey(streams_.name(key.stream), peers_->name(key.receiver));
}

std::string DefaultVideoQualityAnalyzer::ToMetricName(
    const InternalStatsKey& key) const {
  const std::string& stream_label = streams_.name(key.stream);
  if (peers_->size() <= 2 && key.sender != key.receiver) {
    return stream_label;
  }
  rtc::StringBuilder out;
  out << stream_label << "_" << peers_->name(key.sender) << "_"
      << peers_->name(key.receiver);
  return out.str();
}

double DefaultVideoQualityAnalyzer::GetCpuUsagePercent() {
  return cpu_measurer_.GetCpuUsagePercent();
}

std::map<std::string, std::vector<uint16_t>>
DefaultVideoQualityAnalyzer::GetStreamFrames() const {
  MutexLock lock(&mutex_);
  std::map<std::string, std::vector<uint16_t>> out;
  for (auto entry_it : stream_to_frame_id_full_history_) {
    out.insert({streams_.name(entry_it.first), entry_it.second});
  }
  return out;
}

uint16_t DefaultVideoQualityAnalyzer::StreamState::PopFront(size_t peer) {
  size_t peer_queue = GetPeerQueueIndex(peer);
  size_t alive_frames_queue = GetAliveFramesQueueIndex();
  absl::optional<uint16_t> frame_id = frame_ids_.PopFront(peer_queue);
  RTC_DCHECK(frame_id.has_value());

  // If alive's frame queue is longer than all others, than also pop frame from
  // it, because that frame is received by all receivers.
  size_t alive_size = frame_ids_.size(alive_frames_queue);
  size_t other_size = 0;
  for (size_t i = 0; i < frame_ids_.readers_count(); ++i) {
    size_t cur_size = frame_ids_.size(i);
    if (i != alive_frames_queue && cur_size > other_size) {
      other_size = cur_size;
    }
  }
  // Pops frame from alive queue if alive's queue is the longest one.
  if (alive_size > other_size) {
    absl::optional<uint16_t> alive_frame_id =
        frame_ids_.PopFront(alive_frames_queue);
    RTC_DCHECK(alive_frame_id.has_value());
    RTC_DCHECK_EQ(frame_id.value(), alive_frame_id.value());
  }

  return frame_id.value();
}

uint16_t DefaultVideoQualityAnalyzer::StreamState::MarkNextAliveFrameAsDead() {
  absl::optional<uint16_t> frame_id =
      frame_ids_.PopFront(GetAliveFramesQueueIndex());
  RTC_DCHECK(frame_id.has_value());
  return frame_id.value();
}

void DefaultVideoQualityAnalyzer::StreamState::SetLastRenderedFrameTime(
    size_t peer,
    Timestamp time) {
  auto it = last_rendered_frame_time_.find(peer);
  if (it == last_rendered_frame_time_.end()) {
    last_rendered_frame_time_.insert({peer, time});
  } else {
    it->second = time;
  }
}

absl::optional<Timestamp>
DefaultVideoQualityAnalyzer::StreamState::last_rendered_frame_time(
    size_t peer) const {
  return MaybeGetValue(last_rendered_frame_time_, peer);
}

size_t DefaultVideoQualityAnalyzer::StreamState::GetPeerQueueIndex(
    size_t peer_index) const {
  // When sender isn't expecting to receive its own stream we will use their
  // queue for tracking alive frames. Otherwise we will use the queue #0 to
  // track alive frames and will shift all other queues for peers on 1.
  // It means when `enable_receive_own_stream_` is true peer's queue will have
  // index equal to `peer_index` + 1 and when `enable_receive_own_stream_` is
  // false peer's queue will have index equal to `peer_index`.
  if (!enable_receive_own_stream_) {
    return peer_index;
  }
  return peer_index + 1;
}

size_t DefaultVideoQualityAnalyzer::StreamState::GetAliveFramesQueueIndex()
    const {
  // When sender isn't expecting to receive its own stream we will use their
  // queue for tracking alive frames. Otherwise we will use the queue #0 to
  // track alive frames and will shift all other queues for peers on 1.
  if (!enable_receive_own_stream_) {
    return owner_;
  }
  return 0;
}

bool DefaultVideoQualityAnalyzer::FrameInFlight::RemoveFrame() {
  if (!frame_) {
    return false;
  }
  frame_ = absl::nullopt;
  return true;
}

void DefaultVideoQualityAnalyzer::FrameInFlight::SetFrameId(uint16_t id) {
  if (frame_) {
    frame_->set_id(id);
  }
}

std::vector<size_t>
DefaultVideoQualityAnalyzer::FrameInFlight::GetPeersWhichDidntReceive() const {
  std::vector<size_t> out;
  for (size_t i = 0; i < peers_count_; ++i) {
    auto it = receiver_stats_.find(i);
    bool should_current_peer_receive =
        i != owner_ || enable_receive_own_stream_;
    if (should_current_peer_receive &&
        (it == receiver_stats_.end() ||
         it->second.rendered_time.IsInfinite())) {
      out.push_back(i);
    }
  }
  return out;
}

bool DefaultVideoQualityAnalyzer::FrameInFlight::HaveAllPeersReceived() const {
  for (size_t i = 0; i < peers_count_; ++i) {
    // Skip `owner_` only if peer can't receive its own stream.
    if (i == owner_ && !enable_receive_own_stream_) {
      continue;
    }

    auto it = receiver_stats_.find(i);
    if (it == receiver_stats_.end()) {
      return false;
    }

    if (!it->second.dropped && it->second.rendered_time.IsInfinite()) {
      return false;
    }
  }
  return true;
}

void DefaultVideoQualityAnalyzer::FrameInFlight::OnFrameEncoded(
    webrtc::Timestamp time,
    VideoFrameType frame_type,
    DataSize encoded_image_size,
    uint32_t target_encode_bitrate,
    StreamCodecInfo used_encoder) {
  encoded_time_ = time;
  frame_type_ = frame_type;
  encoded_image_size_ = encoded_image_size;
  target_encode_bitrate_ += target_encode_bitrate;
  // Update used encoder info. If simulcast/SVC is used, this method can
  // be called multiple times, in such case we should preserve the value
  // of `used_encoder_.switched_on_at` from the first invocation as the
  // smallest one.
  Timestamp encoder_switched_on_at = used_encoder_.has_value()
                                         ? used_encoder_->switched_on_at
                                         : Timestamp::PlusInfinity();
  RTC_DCHECK(used_encoder.switched_on_at.IsFinite());
  RTC_DCHECK(used_encoder.switched_from_at.IsFinite());
  used_encoder_ = used_encoder;
  if (encoder_switched_on_at < used_encoder_->switched_on_at) {
    used_encoder_->switched_on_at = encoder_switched_on_at;
  }
}

void DefaultVideoQualityAnalyzer::FrameInFlight::OnFramePreDecode(
    size_t peer,
    webrtc::Timestamp received_time,
    webrtc::Timestamp decode_start_time,
    VideoFrameType frame_type,
    DataSize encoded_image_size) {
  receiver_stats_[peer].received_time = received_time;
  receiver_stats_[peer].decode_start_time = decode_start_time;
  receiver_stats_[peer].frame_type = frame_type;
  receiver_stats_[peer].encoded_image_size = encoded_image_size;
}

bool DefaultVideoQualityAnalyzer::FrameInFlight::HasReceivedTime(
    size_t peer) const {
  auto it = receiver_stats_.find(peer);
  if (it == receiver_stats_.end()) {
    return false;
  }
  return it->second.received_time.IsFinite();
}

void DefaultVideoQualityAnalyzer::FrameInFlight::OnFrameDecoded(
    size_t peer,
    webrtc::Timestamp time,
    StreamCodecInfo used_decoder) {
  receiver_stats_[peer].decode_end_time = time;
  receiver_stats_[peer].used_decoder = used_decoder;
}

bool DefaultVideoQualityAnalyzer::FrameInFlight::HasDecodeEndTime(
    size_t peer) const {
  auto it = receiver_stats_.find(peer);
  if (it == receiver_stats_.end()) {
    return false;
  }
  return it->second.decode_end_time.IsFinite();
}

void DefaultVideoQualityAnalyzer::FrameInFlight::OnFrameRendered(
    size_t peer,
    webrtc::Timestamp time,
    int width,
    int height) {
  receiver_stats_[peer].rendered_time = time;
  receiver_stats_[peer].rendered_frame_width = width;
  receiver_stats_[peer].rendered_frame_height = height;
}

bool DefaultVideoQualityAnalyzer::FrameInFlight::HasRenderedTime(
    size_t peer) const {
  auto it = receiver_stats_.find(peer);
  if (it == receiver_stats_.end()) {
    return false;
  }
  return it->second.rendered_time.IsFinite();
}

bool DefaultVideoQualityAnalyzer::FrameInFlight::IsDropped(size_t peer) const {
  auto it = receiver_stats_.find(peer);
  if (it == receiver_stats_.end()) {
    return false;
  }
  return it->second.dropped;
}

FrameStats DefaultVideoQualityAnalyzer::FrameInFlight::GetStatsForPeer(
    size_t peer) const {
  FrameStats stats(captured_time_);
  stats.pre_encode_time = pre_encode_time_;
  stats.encoded_time = encoded_time_;
  stats.target_encode_bitrate = target_encode_bitrate_;
  stats.encoded_frame_type = frame_type_;
  stats.encoded_image_size = encoded_image_size_;
  stats.used_encoder = used_encoder_;

  absl::optional<ReceiverFrameStats> receiver_stats =
      MaybeGetValue<ReceiverFrameStats>(receiver_stats_, peer);
  if (receiver_stats.has_value()) {
    stats.received_time = receiver_stats->received_time;
    stats.decode_start_time = receiver_stats->decode_start_time;
    stats.decode_end_time = receiver_stats->decode_end_time;
    stats.rendered_time = receiver_stats->rendered_time;
    stats.prev_frame_rendered_time = receiver_stats->prev_frame_rendered_time;
    stats.rendered_frame_width = receiver_stats->rendered_frame_width;
    stats.rendered_frame_height = receiver_stats->rendered_frame_height;
    stats.used_decoder = receiver_stats->used_decoder;
    stats.pre_decoded_frame_type = receiver_stats->frame_type;
    stats.pre_decoded_image_size = receiver_stats->encoded_image_size;
  }
  return stats;
}

}  // namespace webrtc
