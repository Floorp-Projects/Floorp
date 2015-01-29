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
#include <sstream>

#include "webrtc/modules/remote_bitrate_estimator/tools/bwe_rtp.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_payload_registry.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/test/rtp_file_reader.h"

int main(int argc, char** argv) {
  if (argc < 4) {
    fprintf(stderr, "Usage: rtp_to_text <extension type> <extension id>"
           " <input_file.rtp> [-t]\n");
    fprintf(stderr, "<extension type> can either be:\n"
           "  abs for absolute send time or\n"
           "  tsoffset for timestamp offset.\n"
           "<extension id> is the id associated with the extension.\n"
           "  -t is an optional flag, if set only packet arrival time will be"
           " output.\n");
    return -1;
  }
  webrtc::test::RtpFileReader* reader;
  webrtc::RtpHeaderParser* parser;
  if (!ParseArgsAndSetupEstimator(argc, argv, NULL, NULL, &reader, &parser,
                                  NULL, NULL)) {
    return -1;
  }
  bool arrival_time_only = (argc >= 5 && strncmp(argv[4], "-t", 2) == 0);
  webrtc::scoped_ptr<webrtc::test::RtpFileReader> rtp_reader(reader);
  webrtc::scoped_ptr<webrtc::RtpHeaderParser> rtp_parser(parser);
  fprintf(stdout, "seqnum timestamp ts_offset abs_sendtime recvtime "
          "markerbit ssrc size\n");
  int packet_counter = 0;
  int non_zero_abs_send_time = 0;
  int non_zero_ts_offsets = 0;
  webrtc::test::RtpFileReader::Packet packet;
  while (rtp_reader->NextPacket(&packet)) {
    webrtc::RTPHeader header;
    parser->Parse(packet.data, packet.length, &header);
    if (header.extension.absoluteSendTime != 0)
      ++non_zero_abs_send_time;
    if (header.extension.transmissionTimeOffset != 0)
      ++non_zero_ts_offsets;
    if (arrival_time_only) {
      std::stringstream ss;
      ss << static_cast<int64_t>(packet.time_ms) * 1000000;
      fprintf(stdout, "%s\n", ss.str().c_str());
    } else {
      fprintf(stdout,
              "%u %u %d %u %u %d %u %d\n",
              header.sequenceNumber,
              header.timestamp,
              header.extension.transmissionTimeOffset,
              header.extension.absoluteSendTime,
              packet.time_ms,
              header.markerBit,
              header.ssrc,
              static_cast<int>(packet.length));
    }
    ++packet_counter;
  }
  fprintf(stderr, "Parsed %d packets\n", packet_counter);
  fprintf(stderr, "Packets with non-zero absolute send time: %d\n",
         non_zero_abs_send_time);
  fprintf(stderr, "Packets with non-zero timestamp offset: %d\n",
         non_zero_ts_offsets);
  return 0;
}
