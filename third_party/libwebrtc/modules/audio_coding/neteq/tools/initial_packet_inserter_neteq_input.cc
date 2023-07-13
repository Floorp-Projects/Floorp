/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/tools/initial_packet_inserter_neteq_input.h"

#include <limits>
#include <memory>
#include <utility>

#include "rtc_base/checks.h"

namespace webrtc {
namespace test {

InitialPacketInserterNetEqInput::InitialPacketInserterNetEqInput(
    std::unique_ptr<NetEqInput> source,
    int number_of_initial_packets,
    int sample_rate_hz)
    : source_(std::move(source)),
      packets_to_insert_(number_of_initial_packets),
      sample_rate_hz_(sample_rate_hz) {}

absl::optional<int64_t> InitialPacketInserterNetEqInput::NextEventTime() const {
  return source_->NextEventTime();
}

std::unique_ptr<NetEqInput::Event> InitialPacketInserterNetEqInput::PopEvent() {
  std::unique_ptr<NetEqInput::Event> event;
  if (!first_packet_) {
    event = source_->PopEvent();
    if (event == nullptr || event->type() != Event::Type::kPacketData) {
      return event;
    }
    first_packet_ = std::move(event);
  }
  if (packets_to_insert_ > 0) {
    RTC_CHECK(first_packet_);
    std::unique_ptr<PacketData> dummy_packet = std::make_unique<PacketData>();
    PacketData& first_packet = static_cast<PacketData&>(*first_packet_);
    dummy_packet->header = first_packet.header;
    dummy_packet->payload =
        rtc::Buffer(first_packet.payload.data(), first_packet.payload.size());
    dummy_packet->timestamp_ms_ = first_packet.timestamp_ms_;
    dummy_packet->header.sequenceNumber -= packets_to_insert_;
    // This assumes 20ms per packet.
    dummy_packet->header.timestamp -=
        20 * sample_rate_hz_ * packets_to_insert_ / 1000;
    packets_to_insert_--;
    return dummy_packet;
  }
  return source_->PopEvent();
}

bool InitialPacketInserterNetEqInput::ended() const {
  return source_->ended();
}

absl::optional<RTPHeader> InitialPacketInserterNetEqInput::NextHeader() const {
  return source_->NextHeader();
}

}  // namespace test
}  // namespace webrtc
