/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/testsupport/packet_reader.h"

#include <cassert>
#include <cstdio>

namespace webrtc {
namespace test {

PacketReader::PacketReader()
    : initialized_(false) {}

PacketReader::~PacketReader() {}

void PacketReader::InitializeReading(uint8_t* data,
                                     int data_length_in_bytes,
                                     int packet_size_in_bytes) {
  assert(data);
  assert(data_length_in_bytes >= 0);
  assert(packet_size_in_bytes > 0);
  data_ = data;
  data_length_ = data_length_in_bytes;
  packet_size_ = packet_size_in_bytes;
  currentIndex_ = 0;
  initialized_ = true;
}

int PacketReader::NextPacket(uint8_t** packet_pointer) {
  if (!initialized_) {
    fprintf(stderr, "Attempting to use uninitialized PacketReader!\n");
    return -1;
  }
  *packet_pointer = data_ + currentIndex_;
  // Check if we're about to read the last packet:
  if (data_length_ - currentIndex_ <= packet_size_) {
    int size = data_length_ - currentIndex_;
    currentIndex_ = data_length_;
    assert(size >= 0);
    return size;
  }
  currentIndex_ += packet_size_;
  assert(packet_size_ >= 0);
  return packet_size_;
}

}  // namespace test
}  // namespace webrtc
