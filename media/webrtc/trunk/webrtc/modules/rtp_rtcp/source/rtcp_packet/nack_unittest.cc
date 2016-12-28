/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/nack.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::ElementsAreArray;
using ::testing::Invoke;
using ::testing::UnorderedElementsAreArray;

using webrtc::rtcp::Nack;
using webrtc::rtcp::RawPacket;
using webrtc::RTCPUtility::RtcpCommonHeader;
using webrtc::RTCPUtility::RtcpParseCommonHeader;

namespace webrtc {
namespace {

const uint32_t kSenderSsrc = 0x12345678;
const uint32_t kRemoteSsrc = 0x23456789;

const uint16_t kList[] = {0, 1, 3, 8, 16};
const size_t kListLength = sizeof(kList) / sizeof(kList[0]);
const uint8_t kPacket[] = {0x81, 205,  0x00, 0x03, 0x12, 0x34, 0x56, 0x78,
                           0x23, 0x45, 0x67, 0x89, 0x00, 0x00, 0x80, 0x85};
const size_t kPacketLength = sizeof(kPacket);

const uint16_t kWrapList[] = {0xffdc, 0xffec, 0xfffe, 0xffff, 0x0000,
                              0x0001, 0x0003, 0x0014, 0x0064};
const size_t kWrapListLength = sizeof(kWrapList) / sizeof(kWrapList[0]);
const uint8_t kWrapPacket[] = {0x81, 205,  0x00, 0x06, 0x12, 0x34, 0x56, 0x78,
                               0x23, 0x45, 0x67, 0x89, 0xff, 0xdc, 0x80, 0x00,
                               0xff, 0xfe, 0x00, 0x17, 0x00, 0x14, 0x00, 0x00,
                               0x00, 0x64, 0x00, 0x00};
const size_t kWrapPacketLength = sizeof(kWrapPacket);

TEST(RtcpPacketNackTest, Create) {
  Nack nack;
  nack.From(kSenderSsrc);
  nack.To(kRemoteSsrc);
  nack.WithList(kList, kListLength);

  rtc::scoped_ptr<RawPacket> packet = nack.Build();

  EXPECT_EQ(kPacketLength, packet->Length());
  EXPECT_EQ(0, memcmp(kPacket, packet->Buffer(), kPacketLength));
}

TEST(RtcpPacketNackTest, Parse) {
  RtcpCommonHeader header;
  EXPECT_TRUE(RtcpParseCommonHeader(kPacket, kPacketLength, &header));
  EXPECT_EQ(kPacketLength, header.BlockSize());
  Nack parsed;

  EXPECT_TRUE(
      parsed.Parse(header, kPacket + RtcpCommonHeader::kHeaderSizeBytes));
  const Nack& const_parsed = parsed;

  EXPECT_EQ(kSenderSsrc, const_parsed.sender_ssrc());
  EXPECT_EQ(kRemoteSsrc, const_parsed.media_ssrc());
  EXPECT_THAT(const_parsed.packet_ids(), ElementsAreArray(kList));
}

TEST(RtcpPacketNackTest, CreateWrap) {
  Nack nack;
  nack.From(kSenderSsrc);
  nack.To(kRemoteSsrc);
  nack.WithList(kWrapList, kWrapListLength);

  rtc::scoped_ptr<RawPacket> packet = nack.Build();

  EXPECT_EQ(kWrapPacketLength, packet->Length());
  EXPECT_EQ(0, memcmp(kWrapPacket, packet->Buffer(), kWrapPacketLength));
}

TEST(RtcpPacketNackTest, ParseWrap) {
  RtcpCommonHeader header;
  EXPECT_TRUE(RtcpParseCommonHeader(kWrapPacket, kWrapPacketLength, &header));
  EXPECT_EQ(kWrapPacketLength, header.BlockSize());

  Nack parsed;
  EXPECT_TRUE(
      parsed.Parse(header, kWrapPacket + RtcpCommonHeader::kHeaderSizeBytes));

  EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
  EXPECT_EQ(kRemoteSsrc, parsed.media_ssrc());
  EXPECT_THAT(parsed.packet_ids(), ElementsAreArray(kWrapList));
}

TEST(RtcpPacketNackTest, BadOrder) {
  // Does not guarantee optimal packing, but should guarantee correctness.
  const uint16_t kUnorderedList[] = {1, 25, 13, 12, 9, 27, 29};
  const size_t kUnorderedListLength =
      sizeof(kUnorderedList) / sizeof(kUnorderedList[0]);
  Nack nack;
  nack.From(kSenderSsrc);
  nack.To(kRemoteSsrc);
  nack.WithList(kUnorderedList, kUnorderedListLength);

  rtc::scoped_ptr<RawPacket> packet = nack.Build();

  Nack parsed;
  RtcpCommonHeader header;
  EXPECT_TRUE(
      RtcpParseCommonHeader(packet->Buffer(), packet->Length(), &header));
  EXPECT_TRUE(parsed.Parse(
      header, packet->Buffer() + RtcpCommonHeader::kHeaderSizeBytes));

  EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
  EXPECT_EQ(kRemoteSsrc, parsed.media_ssrc());
  EXPECT_THAT(parsed.packet_ids(), UnorderedElementsAreArray(kUnorderedList));
}

TEST(RtcpPacketNackTest, CreateFragmented) {
  Nack nack;
  const uint16_t kList[] = {1, 100, 200, 300, 400};
  const uint16_t kListLength = sizeof(kList) / sizeof(kList[0]);
  nack.From(kSenderSsrc);
  nack.To(kRemoteSsrc);
  nack.WithList(kList, kListLength);

  class MockPacketReadyCallback : public rtcp::RtcpPacket::PacketReadyCallback {
   public:
    MOCK_METHOD2(OnPacketReady, void(uint8_t*, size_t));
  } verifier;

  class NackVerifier {
   public:
    explicit NackVerifier(std::vector<uint16_t> ids) : ids_(ids) {}
    void operator()(uint8_t* data, size_t length) {
      RtcpCommonHeader header;
      EXPECT_TRUE(RtcpParseCommonHeader(data, length, &header));
      EXPECT_EQ(length, header.BlockSize());
      Nack nack;
      EXPECT_TRUE(
          nack.Parse(header, data + RtcpCommonHeader::kHeaderSizeBytes));
      EXPECT_EQ(kSenderSsrc, nack.sender_ssrc());
      EXPECT_EQ(kRemoteSsrc, nack.media_ssrc());
      EXPECT_THAT(nack.packet_ids(), ElementsAreArray(ids_));
    }
    std::vector<uint16_t> ids_;
  } packet1({1, 100, 200}), packet2({300, 400});

  EXPECT_CALL(verifier, OnPacketReady(_, _))
      .WillOnce(Invoke(packet1))
      .WillOnce(Invoke(packet2));
  const size_t kBufferSize = 12 + (3 * 4);  // Fits common header + 3 nack items
  uint8_t buffer[kBufferSize];
  EXPECT_TRUE(nack.BuildExternalBuffer(buffer, kBufferSize, &verifier));
}

TEST(RtcpPacketNackTest, CreateFailsWithTooSmallBuffer) {
  const uint16_t kList[] = {1};
  const size_t kMinNackBlockSize = 16;
  Nack nack;
  nack.From(kSenderSsrc);
  nack.To(kRemoteSsrc);
  nack.WithList(kList, 1);
  class Verifier : public rtcp::RtcpPacket::PacketReadyCallback {
   public:
    void OnPacketReady(uint8_t* data, size_t length) override {
      ADD_FAILURE() << "Buffer should be too small.";
    }
  } verifier;
  uint8_t buffer[kMinNackBlockSize - 1];
  EXPECT_FALSE(
      nack.BuildExternalBuffer(buffer, kMinNackBlockSize - 1, &verifier));
}

TEST(RtcpPacketNackTest, ParseFailsWithTooSmallBuffer) {
  RtcpCommonHeader header;
  EXPECT_TRUE(RtcpParseCommonHeader(kPacket, kPacketLength, &header));
  header.payload_size_bytes--;  // Damage the packet
  Nack parsed;
  EXPECT_FALSE(
      parsed.Parse(header, kPacket + RtcpCommonHeader::kHeaderSizeBytes));
}

}  // namespace
}  // namespace webrtc
