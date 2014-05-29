/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/remote_bitrate_estimator/tools/bwe_rtp.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_payload_registry.h"
#include "webrtc/modules/video_coding/main/test/rtp_file_reader.h"
#include "webrtc/modules/video_coding/main/test/rtp_player.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

using webrtc::rtpplayer::RtpPacketSourceInterface;

class Observer : public webrtc::RemoteBitrateObserver {
 public:
  explicit Observer(webrtc::Clock* clock) : clock_(clock) {}

  // Called when a receive channel group has a new bitrate estimate for the
  // incoming streams.
  virtual void OnReceiveBitrateChanged(const std::vector<unsigned int>& ssrcs,
                                       unsigned int bitrate) {
    printf("[%u] Num SSRCs: %d, bitrate: %u\n",
           static_cast<uint32_t>(clock_->TimeInMilliseconds()),
           static_cast<int>(ssrcs.size()), bitrate);
  }

  virtual ~Observer() {}

 private:
  webrtc::Clock* clock_;
};

int main(int argc, char** argv) {
  if (argc < 4) {
    printf("Usage: bwe_rtp_play <extension type> <extension id> "
           "<input_file.rtp>\n");
    printf("<extension type> can either be:\n"
           "  abs for absolute send time or\n"
           "  tsoffset for timestamp offset.\n"
           "<extension id> is the id associated with the extension.\n");
    return -1;
  }
  RtpPacketSourceInterface* reader;
  webrtc::RemoteBitrateEstimator* estimator;
  webrtc::RtpHeaderParser* parser;
  std::string estimator_used;
  webrtc::SimulatedClock clock(0);
  Observer observer(&clock);
  if (!ParseArgsAndSetupEstimator(argc, argv, &clock, &observer, &reader,
                                  &parser, &estimator, &estimator_used)) {
    return -1;
  }
  webrtc::scoped_ptr<RtpPacketSourceInterface> rtp_reader(reader);
  webrtc::scoped_ptr<webrtc::RtpHeaderParser> rtp_parser(parser);
  webrtc::scoped_ptr<webrtc::RemoteBitrateEstimator> rbe(estimator);

  // Process the file.
  int packet_counter = 0;
  int64_t next_process_time_ms = 0;
  int64_t next_rtp_time_ms = 0;
  int64_t first_rtp_time_ms = -1;
  const uint32_t kMaxPacketSize = 1500;
  uint8_t packet_buffer[kMaxPacketSize];
  uint8_t* packet = packet_buffer;
  int non_zero_abs_send_time = 0;
  int non_zero_ts_offsets = 0;
  while (true) {
    uint32_t next_rtp_time;
    if (next_rtp_time_ms <= clock.TimeInMilliseconds()) {
      uint32_t packet_length = kMaxPacketSize;
      if (rtp_reader->NextPacket(packet, &packet_length,
                                 &next_rtp_time) == -1) {
        break;
      }
      if (first_rtp_time_ms == -1)
        first_rtp_time_ms = next_rtp_time;
      next_rtp_time_ms = next_rtp_time - first_rtp_time_ms;
      webrtc::RTPHeader header;
      parser->Parse(packet, packet_length, &header);
      if (header.extension.absoluteSendTime != 0)
        ++non_zero_abs_send_time;
      if (header.extension.transmissionTimeOffset != 0)
        ++non_zero_ts_offsets;
      rbe->IncomingPacket(clock.TimeInMilliseconds(),
                          packet_length - header.headerLength,
                          header);
      ++packet_counter;
    }
    next_process_time_ms = rbe->TimeUntilNextProcess() +
        clock.TimeInMilliseconds();
    if (next_process_time_ms <= clock.TimeInMilliseconds()) {
      rbe->Process();
    }
    int time_until_next_event =
        std::min(next_process_time_ms, next_rtp_time_ms) -
        clock.TimeInMilliseconds();
    clock.AdvanceTimeMilliseconds(std::max(time_until_next_event, 0));
  }
  printf("Parsed %d packets\nTime passed: %u ms\n", packet_counter,
         static_cast<uint32_t>(clock.TimeInMilliseconds()));
  printf("Estimator used: %s\n", estimator_used.c_str());
  printf("Packets with non-zero absolute send time: %d\n",
         non_zero_abs_send_time);
  printf("Packets with non-zero timestamp offset: %d\n",
         non_zero_ts_offsets);
  return 0;
}
