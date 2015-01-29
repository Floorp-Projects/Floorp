/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/modules/rtp_rtcp/mocks/mock_rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_format.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {
namespace {
const size_t kMaxPayloadSize = 1200;
const size_t kLengthFieldLength = 2;

enum Nalu {
  kSlice = 1,
  kIdr = 5,
  kSei = 6,
  kSps = 7,
  kPps = 8,
  kStapA = 24,
  kFuA = 28
};

static const size_t kNalHeaderSize = 1;
static const size_t kFuAHeaderSize = 2;

// Bit masks for FU (A and B) indicators.
enum NalDefs { kFBit = 0x80, kNriMask = 0x60, kTypeMask = 0x1F };

// Bit masks for FU (A and B) headers.
enum FuDefs { kSBit = 0x80, kEBit = 0x40, kRBit = 0x20 };

void VerifyFua(size_t fua_index,
               const uint8_t* expected_payload,
               int offset,
               const uint8_t* packet,
               size_t length,
               const std::vector<size_t>& expected_sizes) {
  ASSERT_EQ(expected_sizes[fua_index] + kFuAHeaderSize, length)
      << "FUA index: " << fua_index;
  const uint8_t kFuIndicator = 0x1C;  // F=0, NRI=0, Type=28.
  EXPECT_EQ(kFuIndicator, packet[0]) << "FUA index: " << fua_index;
  bool should_be_last_fua = (fua_index == expected_sizes.size() - 1);
  uint8_t fu_header = 0;
  if (fua_index == 0)
    fu_header = 0x85;  // S=1, E=0, R=0, Type=5.
  else if (should_be_last_fua)
    fu_header = 0x45;  // S=0, E=1, R=0, Type=5.
  else
    fu_header = 0x05;  // S=0, E=0, R=0, Type=5.
  EXPECT_EQ(fu_header, packet[1]) << "FUA index: " << fua_index;
  std::vector<uint8_t> expected_packet_payload(
      &expected_payload[offset],
      &expected_payload[offset + expected_sizes[fua_index]]);
  EXPECT_THAT(
      expected_packet_payload,
      ::testing::ElementsAreArray(&packet[2], expected_sizes[fua_index]))
      << "FUA index: " << fua_index;
}

void TestFua(size_t frame_size,
             size_t max_payload_size,
             const std::vector<size_t>& expected_sizes) {
  scoped_ptr<uint8_t[]> frame;
  frame.reset(new uint8_t[frame_size]);
  frame[0] = 0x05;  // F=0, NRI=0, Type=5.
  for (size_t i = 0; i < frame_size - kNalHeaderSize; ++i) {
    frame[i + kNalHeaderSize] = i;
  }
  RTPFragmentationHeader fragmentation;
  fragmentation.VerifyAndAllocateFragmentationHeader(1);
  fragmentation.fragmentationOffset[0] = 0;
  fragmentation.fragmentationLength[0] = frame_size;
  scoped_ptr<RtpPacketizer> packetizer(RtpPacketizer::Create(
      kRtpVideoH264, max_payload_size, NULL, kFrameEmpty));
  packetizer->SetPayloadData(frame.get(), frame_size, &fragmentation);

  scoped_ptr<uint8_t[]> packet(new uint8_t[max_payload_size]);
  size_t length = 0;
  bool last = false;
  size_t offset = kNalHeaderSize;
  for (size_t i = 0; i < expected_sizes.size(); ++i) {
    ASSERT_TRUE(packetizer->NextPacket(packet.get(), &length, &last));
    VerifyFua(i, frame.get(), offset, packet.get(), length, expected_sizes);
    EXPECT_EQ(i == expected_sizes.size() - 1, last) << "FUA index: " << i;
    offset += expected_sizes[i];
  }

  EXPECT_FALSE(packetizer->NextPacket(packet.get(), &length, &last));
}

size_t GetExpectedNaluOffset(const RTPFragmentationHeader& fragmentation,
                             size_t start_index,
                             size_t nalu_index) {
  assert(nalu_index < fragmentation.fragmentationVectorSize);
  size_t expected_nalu_offset = kNalHeaderSize;  // STAP-A header.
  for (size_t i = start_index; i < nalu_index; ++i) {
    expected_nalu_offset +=
        kLengthFieldLength + fragmentation.fragmentationLength[i];
  }
  return expected_nalu_offset;
}

void VerifyStapAPayload(const RTPFragmentationHeader& fragmentation,
                        size_t first_stapa_index,
                        size_t nalu_index,
                        const uint8_t* frame,
                        size_t frame_length,
                        const uint8_t* packet,
                        size_t packet_length) {
  size_t expected_payload_offset =
      GetExpectedNaluOffset(fragmentation, first_stapa_index, nalu_index) +
      kLengthFieldLength;
  size_t offset = fragmentation.fragmentationOffset[nalu_index];
  const uint8_t* expected_payload = &frame[offset];
  size_t expected_payload_length =
      fragmentation.fragmentationLength[nalu_index];
  ASSERT_LE(offset + expected_payload_length, frame_length);
  ASSERT_LE(expected_payload_offset + expected_payload_length, packet_length);
  std::vector<uint8_t> expected_payload_vector(
      expected_payload, &expected_payload[expected_payload_length]);
  EXPECT_THAT(expected_payload_vector,
              ::testing::ElementsAreArray(&packet[expected_payload_offset],
                                          expected_payload_length));
}

void VerifySingleNaluPayload(const RTPFragmentationHeader& fragmentation,
                             size_t nalu_index,
                             const uint8_t* frame,
                             size_t frame_length,
                             const uint8_t* packet,
                             size_t packet_length) {
  std::vector<uint8_t> expected_payload_vector(
      &frame[fragmentation.fragmentationOffset[nalu_index]],
      &frame[fragmentation.fragmentationOffset[nalu_index] +
             fragmentation.fragmentationLength[nalu_index]]);
  EXPECT_THAT(expected_payload_vector,
              ::testing::ElementsAreArray(packet, packet_length));
}
}  // namespace

TEST(RtpPacketizerH264Test, TestSingleNalu) {
  const uint8_t frame[2] = {0x05, 0xFF};  // F=0, NRI=0, Type=5.
  RTPFragmentationHeader fragmentation;
  fragmentation.VerifyAndAllocateFragmentationHeader(1);
  fragmentation.fragmentationOffset[0] = 0;
  fragmentation.fragmentationLength[0] = sizeof(frame);
  scoped_ptr<RtpPacketizer> packetizer(
      RtpPacketizer::Create(kRtpVideoH264, kMaxPayloadSize, NULL, kFrameEmpty));
  packetizer->SetPayloadData(frame, sizeof(frame), &fragmentation);
  uint8_t packet[kMaxPayloadSize] = {0};
  size_t length = 0;
  bool last = false;
  ASSERT_TRUE(packetizer->NextPacket(packet, &length, &last));
  EXPECT_EQ(2u, length);
  EXPECT_TRUE(last);
  VerifySingleNaluPayload(
      fragmentation, 0, frame, sizeof(frame), packet, length);
  EXPECT_FALSE(packetizer->NextPacket(packet, &length, &last));
}

TEST(RtpPacketizerH264Test, TestSingleNaluTwoPackets) {
  const size_t kFrameSize = kMaxPayloadSize + 100;
  uint8_t frame[kFrameSize] = {0};
  for (size_t i = 0; i < kFrameSize; ++i)
    frame[i] = i;
  RTPFragmentationHeader fragmentation;
  fragmentation.VerifyAndAllocateFragmentationHeader(2);
  fragmentation.fragmentationOffset[0] = 0;
  fragmentation.fragmentationLength[0] = kMaxPayloadSize;
  fragmentation.fragmentationOffset[1] = kMaxPayloadSize;
  fragmentation.fragmentationLength[1] = 100;
  // Set NAL headers.
  frame[fragmentation.fragmentationOffset[0]] = 0x01;
  frame[fragmentation.fragmentationOffset[1]] = 0x01;

  scoped_ptr<RtpPacketizer> packetizer(
      RtpPacketizer::Create(kRtpVideoH264, kMaxPayloadSize, NULL, kFrameEmpty));
  packetizer->SetPayloadData(frame, kFrameSize, &fragmentation);

  uint8_t packet[kMaxPayloadSize] = {0};
  size_t length = 0;
  bool last = false;
  ASSERT_TRUE(packetizer->NextPacket(packet, &length, &last));
  ASSERT_EQ(fragmentation.fragmentationOffset[1], length);
  VerifySingleNaluPayload(fragmentation, 0, frame, kFrameSize, packet, length);

  ASSERT_TRUE(packetizer->NextPacket(packet, &length, &last));
  ASSERT_EQ(fragmentation.fragmentationLength[1], length);
  VerifySingleNaluPayload(fragmentation, 1, frame, kFrameSize, packet, length);
  EXPECT_TRUE(last);

  EXPECT_FALSE(packetizer->NextPacket(packet, &length, &last));
}

TEST(RtpPacketizerH264Test, TestStapA) {
  const size_t kFrameSize =
      kMaxPayloadSize - 3 * kLengthFieldLength - kNalHeaderSize;
  uint8_t frame[kFrameSize] = {0x07, 0xFF,  // F=0, NRI=0, Type=7.
                               0x08, 0xFF,  // F=0, NRI=0, Type=8.
                               0x05};       // F=0, NRI=0, Type=5.
  const size_t kPayloadOffset = 5;
  for (size_t i = 0; i < kFrameSize - kPayloadOffset; ++i)
    frame[i + kPayloadOffset] = i;
  RTPFragmentationHeader fragmentation;
  fragmentation.VerifyAndAllocateFragmentationHeader(3);
  fragmentation.fragmentationOffset[0] = 0;
  fragmentation.fragmentationLength[0] = 2;
  fragmentation.fragmentationOffset[1] = 2;
  fragmentation.fragmentationLength[1] = 2;
  fragmentation.fragmentationOffset[2] = 4;
  fragmentation.fragmentationLength[2] =
      kNalHeaderSize + kFrameSize - kPayloadOffset;
  scoped_ptr<RtpPacketizer> packetizer(
      RtpPacketizer::Create(kRtpVideoH264, kMaxPayloadSize, NULL, kFrameEmpty));
  packetizer->SetPayloadData(frame, kFrameSize, &fragmentation);

  uint8_t packet[kMaxPayloadSize] = {0};
  size_t length = 0;
  bool last = false;
  ASSERT_TRUE(packetizer->NextPacket(packet, &length, &last));
  size_t expected_packet_size =
      kNalHeaderSize + 3 * kLengthFieldLength + kFrameSize;
  ASSERT_EQ(expected_packet_size, length);
  EXPECT_TRUE(last);
  for (size_t i = 0; i < fragmentation.fragmentationVectorSize; ++i)
    VerifyStapAPayload(fragmentation, 0, i, frame, kFrameSize, packet, length);

  EXPECT_FALSE(packetizer->NextPacket(packet, &length, &last));
}

TEST(RtpPacketizerH264Test, TestTooSmallForStapAHeaders) {
  const size_t kFrameSize = kMaxPayloadSize - 1;
  uint8_t frame[kFrameSize] = {0x07, 0xFF,  // F=0, NRI=0, Type=7.
                               0x08, 0xFF,  // F=0, NRI=0, Type=8.
                               0x05};       // F=0, NRI=0, Type=5.
  const size_t kPayloadOffset = 5;
  for (size_t i = 0; i < kFrameSize - kPayloadOffset; ++i)
    frame[i + kPayloadOffset] = i;
  RTPFragmentationHeader fragmentation;
  fragmentation.VerifyAndAllocateFragmentationHeader(3);
  fragmentation.fragmentationOffset[0] = 0;
  fragmentation.fragmentationLength[0] = 2;
  fragmentation.fragmentationOffset[1] = 2;
  fragmentation.fragmentationLength[1] = 2;
  fragmentation.fragmentationOffset[2] = 4;
  fragmentation.fragmentationLength[2] =
      kNalHeaderSize + kFrameSize - kPayloadOffset;
  scoped_ptr<RtpPacketizer> packetizer(
      RtpPacketizer::Create(kRtpVideoH264, kMaxPayloadSize, NULL, kFrameEmpty));
  packetizer->SetPayloadData(frame, kFrameSize, &fragmentation);

  uint8_t packet[kMaxPayloadSize] = {0};
  size_t length = 0;
  bool last = false;
  ASSERT_TRUE(packetizer->NextPacket(packet, &length, &last));
  size_t expected_packet_size = kNalHeaderSize;
  for (size_t i = 0; i < 2; ++i) {
    expected_packet_size +=
        kLengthFieldLength + fragmentation.fragmentationLength[i];
  }
  ASSERT_EQ(expected_packet_size, length);
  EXPECT_FALSE(last);
  for (size_t i = 0; i < 2; ++i)
    VerifyStapAPayload(fragmentation, 0, i, frame, kFrameSize, packet, length);

  ASSERT_TRUE(packetizer->NextPacket(packet, &length, &last));
  expected_packet_size = fragmentation.fragmentationLength[2];
  ASSERT_EQ(expected_packet_size, length);
  EXPECT_TRUE(last);
  VerifySingleNaluPayload(fragmentation, 2, frame, kFrameSize, packet, length);

  EXPECT_FALSE(packetizer->NextPacket(packet, &length, &last));
}

TEST(RtpPacketizerH264Test, TestMixedStapA_FUA) {
  const size_t kFuaNaluSize = 2 * (kMaxPayloadSize - 100);
  const size_t kStapANaluSize = 100;
  RTPFragmentationHeader fragmentation;
  fragmentation.VerifyAndAllocateFragmentationHeader(3);
  fragmentation.fragmentationOffset[0] = 0;
  fragmentation.fragmentationLength[0] = kFuaNaluSize;
  fragmentation.fragmentationOffset[1] = kFuaNaluSize;
  fragmentation.fragmentationLength[1] = kStapANaluSize;
  fragmentation.fragmentationOffset[2] = kFuaNaluSize + kStapANaluSize;
  fragmentation.fragmentationLength[2] = kStapANaluSize;
  const size_t kFrameSize = kFuaNaluSize + 2 * kStapANaluSize;
  uint8_t frame[kFrameSize];
  size_t nalu_offset = 0;
  for (size_t i = 0; i < fragmentation.fragmentationVectorSize; ++i) {
    nalu_offset = fragmentation.fragmentationOffset[i];
    frame[nalu_offset] = 0x05;  // F=0, NRI=0, Type=5.
    for (size_t j = 1; j < fragmentation.fragmentationLength[i]; ++j) {
      frame[nalu_offset + j] = i + j;
    }
  }
  scoped_ptr<RtpPacketizer> packetizer(
      RtpPacketizer::Create(kRtpVideoH264, kMaxPayloadSize, NULL, kFrameEmpty));
  packetizer->SetPayloadData(frame, kFrameSize, &fragmentation);

  // First expecting two FU-A packets.
  std::vector<size_t> fua_sizes;
  fua_sizes.push_back(1100);
  fua_sizes.push_back(1099);
  uint8_t packet[kMaxPayloadSize] = {0};
  size_t length = 0;
  bool last = false;
  int fua_offset = kNalHeaderSize;
  for (size_t i = 0; i < 2; ++i) {
    ASSERT_TRUE(packetizer->NextPacket(packet, &length, &last));
    VerifyFua(i, frame, fua_offset, packet, length, fua_sizes);
    EXPECT_FALSE(last);
    fua_offset += fua_sizes[i];
  }
  // Then expecting one STAP-A packet with two nal units.
  ASSERT_TRUE(packetizer->NextPacket(packet, &length, &last));
  size_t expected_packet_size =
      kNalHeaderSize + 2 * kLengthFieldLength + 2 * kStapANaluSize;
  ASSERT_EQ(expected_packet_size, length);
  EXPECT_TRUE(last);
  for (size_t i = 1; i < fragmentation.fragmentationVectorSize; ++i)
    VerifyStapAPayload(fragmentation, 1, i, frame, kFrameSize, packet, length);

  EXPECT_FALSE(packetizer->NextPacket(packet, &length, &last));
}

TEST(RtpPacketizerH264Test, TestFUAOddSize) {
  const size_t kExpectedPayloadSizes[2] = {600, 600};
  TestFua(
      kMaxPayloadSize + 1,
      kMaxPayloadSize,
      std::vector<size_t>(kExpectedPayloadSizes,
                          kExpectedPayloadSizes +
                              sizeof(kExpectedPayloadSizes) / sizeof(size_t)));
}

TEST(RtpPacketizerH264Test, TestFUAEvenSize) {
  const size_t kExpectedPayloadSizes[2] = {601, 600};
  TestFua(
      kMaxPayloadSize + 2,
      kMaxPayloadSize,
      std::vector<size_t>(kExpectedPayloadSizes,
                          kExpectedPayloadSizes +
                              sizeof(kExpectedPayloadSizes) / sizeof(size_t)));
}

TEST(RtpPacketizerH264Test, TestFUARounding) {
  const size_t kExpectedPayloadSizes[8] = {1266, 1266, 1266, 1266,
                                           1266, 1266, 1266, 1261};
  TestFua(
      10124,
      1448,
      std::vector<size_t>(kExpectedPayloadSizes,
                          kExpectedPayloadSizes +
                              sizeof(kExpectedPayloadSizes) / sizeof(size_t)));
}

TEST(RtpPacketizerH264Test, TestFUABig) {
  const size_t kExpectedPayloadSizes[10] = {1198, 1198, 1198, 1198, 1198,
                                            1198, 1198, 1198, 1198, 1198};
  // Generate 10 full sized packets, leave room for FU-A headers minus the NALU
  // header.
  TestFua(
      10 * (kMaxPayloadSize - kFuAHeaderSize) + kNalHeaderSize,
      kMaxPayloadSize,
      std::vector<size_t>(kExpectedPayloadSizes,
                          kExpectedPayloadSizes +
                              sizeof(kExpectedPayloadSizes) / sizeof(size_t)));
}

class RtpDepacketizerH264Test : public ::testing::Test {
 protected:
  RtpDepacketizerH264Test()
      : depacketizer_(RtpDepacketizer::Create(kRtpVideoH264)) {}

  void ExpectPacket(RtpDepacketizer::ParsedPayload* parsed_payload,
                    const uint8_t* data,
                    size_t length) {
    ASSERT_TRUE(parsed_payload != NULL);
    EXPECT_THAT(std::vector<uint8_t>(
                    parsed_payload->payload,
                    parsed_payload->payload + parsed_payload->payload_length),
                ::testing::ElementsAreArray(data, length));
  }

  scoped_ptr<RtpDepacketizer> depacketizer_;
};

TEST_F(RtpDepacketizerH264Test, TestSingleNalu) {
  uint8_t packet[2] = {0x05, 0xFF};  // F=0, NRI=0, Type=5.
  RtpDepacketizer::ParsedPayload payload;

  ASSERT_TRUE(depacketizer_->Parse(&payload, packet, sizeof(packet)));
  ExpectPacket(&payload, packet, sizeof(packet));
  EXPECT_EQ(kVideoFrameKey, payload.frame_type);
  EXPECT_EQ(kRtpVideoH264, payload.type.Video.codec);
  EXPECT_TRUE(payload.type.Video.isFirstPacket);
  EXPECT_TRUE(payload.type.Video.codecHeader.H264.single_nalu);
  EXPECT_FALSE(payload.type.Video.codecHeader.H264.stap_a);
}

TEST_F(RtpDepacketizerH264Test, TestStapAKey) {
  uint8_t packet[16] = {kStapA,  // F=0, NRI=0, Type=24.
                        // Length, nal header, payload.
                        0,      0x02, kIdr, 0xFF, 0,    0x03, kIdr, 0xFF,
                        0x00,   0,    0x04, kIdr, 0xFF, 0x00, 0x11};
  RtpDepacketizer::ParsedPayload payload;

  ASSERT_TRUE(depacketizer_->Parse(&payload, packet, sizeof(packet)));
  ExpectPacket(&payload, packet, sizeof(packet));
  EXPECT_EQ(kVideoFrameKey, payload.frame_type);
  EXPECT_EQ(kRtpVideoH264, payload.type.Video.codec);
  EXPECT_TRUE(payload.type.Video.isFirstPacket);
  EXPECT_TRUE(payload.type.Video.codecHeader.H264.single_nalu);
  EXPECT_TRUE(payload.type.Video.codecHeader.H264.stap_a);
}

TEST_F(RtpDepacketizerH264Test, TestStapADelta) {
  uint8_t packet[16] = {kStapA,  // F=0, NRI=0, Type=24.
                        // Length, nal header, payload.
                        0,      0x02, kSlice, 0xFF,   0,    0x03, kSlice, 0xFF,
                        0x00,   0,    0x04,   kSlice, 0xFF, 0x00, 0x11};
  RtpDepacketizer::ParsedPayload payload;

  ASSERT_TRUE(depacketizer_->Parse(&payload, packet, sizeof(packet)));
  ExpectPacket(&payload, packet, sizeof(packet));
  EXPECT_EQ(kVideoFrameDelta, payload.frame_type);
  EXPECT_EQ(kRtpVideoH264, payload.type.Video.codec);
  EXPECT_TRUE(payload.type.Video.isFirstPacket);
  EXPECT_TRUE(payload.type.Video.codecHeader.H264.single_nalu);
  EXPECT_TRUE(payload.type.Video.codecHeader.H264.stap_a);
}

TEST_F(RtpDepacketizerH264Test, TestFuA) {
  uint8_t packet1[3] = {
      kFuA,          // F=0, NRI=0, Type=28.
      kSBit | kIdr,  // FU header.
      0x01           // Payload.
  };
  const uint8_t kExpected1[2] = {kIdr, 0x01};

  uint8_t packet2[3] = {
      kFuA,  // F=0, NRI=0, Type=28.
      kIdr,  // FU header.
      0x02   // Payload.
  };
  const uint8_t kExpected2[1] = {0x02};

  uint8_t packet3[3] = {
      kFuA,          // F=0, NRI=0, Type=28.
      kEBit | kIdr,  // FU header.
      0x03           // Payload.
  };
  const uint8_t kExpected3[1] = {0x03};

  RtpDepacketizer::ParsedPayload payload;

  // We expect that the first packet is one byte shorter since the FU-A header
  // has been replaced by the original nal header.
  ASSERT_TRUE(depacketizer_->Parse(&payload, packet1, sizeof(packet1)));
  ExpectPacket(&payload, kExpected1, sizeof(kExpected1));
  EXPECT_EQ(kVideoFrameKey, payload.frame_type);
  EXPECT_EQ(kRtpVideoH264, payload.type.Video.codec);
  EXPECT_TRUE(payload.type.Video.isFirstPacket);
  EXPECT_FALSE(payload.type.Video.codecHeader.H264.single_nalu);
  EXPECT_FALSE(payload.type.Video.codecHeader.H264.stap_a);

  // Following packets will be 2 bytes shorter since they will only be appended
  // onto the first packet.
  payload = RtpDepacketizer::ParsedPayload();
  ASSERT_TRUE(depacketizer_->Parse(&payload, packet2, sizeof(packet2)));
  ExpectPacket(&payload, kExpected2, sizeof(kExpected2));
  EXPECT_EQ(kVideoFrameKey, payload.frame_type);
  EXPECT_EQ(kRtpVideoH264, payload.type.Video.codec);
  EXPECT_FALSE(payload.type.Video.isFirstPacket);
  EXPECT_FALSE(payload.type.Video.codecHeader.H264.single_nalu);
  EXPECT_FALSE(payload.type.Video.codecHeader.H264.stap_a);

  payload = RtpDepacketizer::ParsedPayload();
  ASSERT_TRUE(depacketizer_->Parse(&payload, packet3, sizeof(packet3)));
  ExpectPacket(&payload, kExpected3, sizeof(kExpected3));
  EXPECT_EQ(kVideoFrameKey, payload.frame_type);
  EXPECT_EQ(kRtpVideoH264, payload.type.Video.codec);
  EXPECT_FALSE(payload.type.Video.isFirstPacket);
  EXPECT_FALSE(payload.type.Video.codecHeader.H264.single_nalu);
  EXPECT_FALSE(payload.type.Video.codecHeader.H264.stap_a);
}
}  // namespace webrtc
