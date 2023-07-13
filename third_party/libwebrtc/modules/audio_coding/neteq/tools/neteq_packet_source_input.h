/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_TOOLS_NETEQ_PACKET_SOURCE_INPUT_H_
#define MODULES_AUDIO_CODING_NETEQ_TOOLS_NETEQ_PACKET_SOURCE_INPUT_H_

#include <map>
#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "modules/audio_coding/neteq/tools/neteq_input.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"

namespace webrtc {
namespace test {

class RtpFileSource;

// An adapter class to dress up a PacketSource object as a NetEqInput.
class NetEqPacketSourceInput : public NetEqInput {
 public:
  using RtpHeaderExtensionMap = std::map<int, webrtc::RTPExtensionType>;

  NetEqPacketSourceInput(
      absl::string_view file_name,
      const NetEqPacketSourceInput::RtpHeaderExtensionMap& hdr_ext_map,
      absl::optional<uint32_t> ssrc_filter);

  absl::optional<int64_t> NextEventTime() const override {
    if (event_) {
      return event_->timestamp_ms();
    }
    return absl::nullopt;
  }
  std::unique_ptr<Event> PopEvent() override;
  absl::optional<RTPHeader> NextHeader() const override;
  bool ended() const override { return event_ == nullptr; }

 private:
  static constexpr int64_t kOutputPeriodMs = 10;
  std::unique_ptr<Event> GetNextEvent();

  std::unique_ptr<Packet> packet_;
  std::unique_ptr<PacketSource> packet_source_;
  int64_t next_output_event_ms_;
  std::unique_ptr<Event> event_;
};

}  // namespace test
}  // namespace webrtc
#endif  // MODULES_AUDIO_CODING_NETEQ_TOOLS_NETEQ_PACKET_SOURCE_INPUT_H_
