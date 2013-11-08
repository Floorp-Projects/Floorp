/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_MODULES_VIDEO_CODING_MAIN_SOURCE_TEST_STREAM_GENERATOR_H_
#define WEBRTC_MODULES_VIDEO_CODING_MAIN_SOURCE_TEST_STREAM_GENERATOR_H_

#include <string.h>

#include <list>

#include "webrtc/modules/video_coding/main/source/packet.h"
#include "webrtc/modules/video_coding/main/test/test_util.h"

namespace webrtc {

const unsigned int kDefaultBitrateKbps = 1000;
const unsigned int kDefaultFrameRate = 25;
const unsigned int kMaxPacketSize = 1500;
const unsigned int kFrameSize =
    (kDefaultBitrateKbps + kDefaultFrameRate * 4) / (kDefaultFrameRate * 8);
const int kDefaultFramePeriodMs = 1000 / kDefaultFrameRate;

class StreamGenerator {
 public:
  StreamGenerator(uint16_t start_seq_num,
                  uint32_t start_timestamp,
                  int64_t current_time);
  void Init(uint16_t start_seq_num,
            uint32_t start_timestamp,
            int64_t current_time);

  void GenerateFrame(FrameType type,
                     int num_media_packets,
                     int num_empty_packets,
                     int64_t current_time);

  VCMPacket GeneratePacket(uint16_t sequence_number,
                           uint32_t timestamp,
                           unsigned int size,
                           bool first_packet,
                           bool marker_bit,
                           FrameType type);

  bool PopPacket(VCMPacket* packet, int index);
  void DropLastPacket();

  bool GetPacket(VCMPacket* packet, int index);

  bool NextPacket(VCMPacket* packet);

  uint16_t NextSequenceNumber() const;

  int PacketsRemaining() const;

 private:
  std::list<VCMPacket>::iterator GetPacketIterator(int index);

  std::list<VCMPacket> packets_;
  uint16_t sequence_number_;
  uint32_t timestamp_;
  int64_t start_time_;
  uint8_t packet_buffer[kMaxPacketSize];

  DISALLOW_COPY_AND_ASSIGN(StreamGenerator);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_MAIN_SOURCE_TEST_STREAM_GENERATOR_H_
