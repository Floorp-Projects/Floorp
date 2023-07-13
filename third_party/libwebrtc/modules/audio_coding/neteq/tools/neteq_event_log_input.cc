/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/tools/neteq_event_log_input.h"

#include <limits>
#include <memory>

#include "absl/strings/string_view.h"
#include "modules/audio_coding/neteq/tools/rtc_event_log_source.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace test {

NetEqEventLogInput* NetEqEventLogInput::CreateFromFile(
    absl::string_view file_name,
    absl::optional<uint32_t> ssrc_filter) {
  auto event_log_src =
      RtcEventLogSource::CreateFromFile(file_name, ssrc_filter);
  if (!event_log_src) {
    return nullptr;
  }
  return new NetEqEventLogInput(std::move(event_log_src));
}

NetEqEventLogInput* NetEqEventLogInput::CreateFromString(
    absl::string_view file_contents,
    absl::optional<uint32_t> ssrc_filter) {
  auto event_log_src =
      RtcEventLogSource::CreateFromString(file_contents, ssrc_filter);
  if (!event_log_src) {
    return nullptr;
  }
  return new NetEqEventLogInput(std::move(event_log_src));
}

std::unique_ptr<NetEqInput::Event> NetEqEventLogInput::PopEvent() {
  std::unique_ptr<NetEqInput::Event> event_to_return = std::move(event_);
  event_ = GetNextEvent();
  return event_to_return;
}

absl::optional<RTPHeader> NetEqEventLogInput::NextHeader() const {
  return packet_ ? absl::optional<RTPHeader>(packet_->header()) : absl::nullopt;
}

absl::optional<int64_t> NetEqEventLogInput::NextOutputEventTime() {
  absl::optional<int64_t> next_output_event_ms;
  next_output_event_ms = source_->NextAudioOutputEventMs();
  if (*next_output_event_ms == std::numeric_limits<int64_t>::max()) {
    next_output_event_ms = absl::nullopt;
  }
  return next_output_event_ms;
}

std::unique_ptr<NetEqInput::Event> NetEqEventLogInput::CreatePacketEvent() {
  std::unique_ptr<PacketData> packet_data = std::make_unique<PacketData>();
  packet_data->header = packet_->header();
  if (packet_->payload_length_bytes() == 0 &&
      packet_->virtual_payload_length_bytes() > 0) {
    // This is a header-only "dummy" packet. Set the payload to all zeros, with
    // length according to the virtual length.
    packet_data->payload.SetSize(packet_->virtual_payload_length_bytes());
    std::fill_n(packet_data->payload.data(), packet_data->payload.size(), 0);
  } else {
    packet_data->payload.SetData(packet_->payload(),
                                 packet_->payload_length_bytes());
  }
  packet_data->timestamp_ms_ = packet_->time_ms();
  packet_ = source_->NextPacket();
  return packet_data;
}

std::unique_ptr<NetEqInput::Event> NetEqEventLogInput::CreateOutputEvent() {
  std::unique_ptr<NetEqInput::Event> event =
      std::make_unique<NetEqInput::GetAudio>(next_output_event_ms_.value());
  next_output_event_ms_ = NextOutputEventTime();
  return event;
}

std::unique_ptr<NetEqInput::Event>
NetEqEventLogInput::CreateSetMinimumDelayEvent() {
  std::unique_ptr<NetEqInput::Event> event =
      std::make_unique<NetEqInput::SetMinimumDelay>(
          next_minimum_delay_event_.value());
  next_minimum_delay_event_ = source_->NextSetMinimumDelayEvent();
  return event;
}

std::unique_ptr<NetEqInput::Event> NetEqEventLogInput::GetNextEvent() {
  std::unique_ptr<NetEqInput::Event> event;
  int64_t packet_time_ms = packet_ ? static_cast<int64_t>(packet_->time_ms())
                                   : std::numeric_limits<int64_t>::max();
  int64_t output_time_ms = next_output_event_ms_.has_value()
                               ? next_output_event_ms_.value()
                               : std::numeric_limits<int64_t>::max();
  int64_t minimum_delay_ms = next_minimum_delay_event_.has_value()
                                 ? next_minimum_delay_event_->timestamp_ms()
                                 : std::numeric_limits<int64_t>::max();
  if (packet_ && packet_time_ms <= minimum_delay_ms &&
      packet_time_ms <= output_time_ms) {
    event = CreatePacketEvent();
  } else if (next_minimum_delay_event_.has_value() &&
             minimum_delay_ms <= output_time_ms) {
    event = CreateSetMinimumDelayEvent();
  } else if (next_output_event_ms_.has_value()) {
    event = CreateOutputEvent();
  }
  return event;
}

NetEqEventLogInput::NetEqEventLogInput(
    std::unique_ptr<RtcEventLogSource> source)
    : source_(std::move(source)) {
  packet_ = source_->NextPacket();
  next_minimum_delay_event_ = source_->NextSetMinimumDelayEvent();
  next_output_event_ms_ = NextOutputEventTime();
  event_ = GetNextEvent();
}

}  // namespace test
}  // namespace webrtc
