/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_TOOLS_NETEQ_EVENT_LOG_INPUT_H_
#define MODULES_AUDIO_CODING_NETEQ_TOOLS_NETEQ_EVENT_LOG_INPUT_H_

#include <map>
#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "modules/audio_coding/neteq/tools/neteq_packet_source_input.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"

namespace webrtc {
namespace test {

class RtcEventLogSource;

// Implementation of a NetEqInput from an RtcEventLogSource.
class NetEqEventLogInput final : public NetEqInput {
 public:
  static NetEqEventLogInput* CreateFromFile(
      absl::string_view file_name,
      absl::optional<uint32_t> ssrc_filter);
  static NetEqEventLogInput* CreateFromString(
      absl::string_view file_contents,
      absl::optional<uint32_t> ssrc_filter);

  absl::optional<int64_t> NextEventTime() const override {
    if (event_) {
      return event_->timestamp_ms();
    }
    return absl::nullopt;
  }
  std::unique_ptr<Event> PopEvent() override;
  absl::optional<RTPHeader> NextHeader() const override;
  bool ended() const override {
    return !next_output_event_ms_ || packet_ == nullptr;
  }

 private:
  NetEqEventLogInput(std::unique_ptr<RtcEventLogSource> source);
  std::unique_ptr<Event> GetNextEvent();
  std::unique_ptr<Event> CreatePacketEvent();
  std::unique_ptr<Event> CreateOutputEvent();
  std::unique_ptr<Event> CreateSetMinimumDelayEvent();
  absl::optional<int64_t> NextOutputEventTime();

  std::unique_ptr<RtcEventLogSource> source_;
  std::unique_ptr<Packet> packet_;
  absl::optional<int64_t> next_output_event_ms_;
  absl::optional<SetMinimumDelay> next_minimum_delay_event_;
  std::unique_ptr<Event> event_;
};

}  // namespace test
}  // namespace webrtc
#endif  // MODULES_AUDIO_CODING_NETEQ_TOOLS_NETEQ_EVENT_LOG_INPUT_H_
