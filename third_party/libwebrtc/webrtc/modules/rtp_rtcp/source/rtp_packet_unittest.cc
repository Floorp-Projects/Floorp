/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"

#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "rtc_base/random.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::make_tuple;

constexpr int8_t kPayloadType = 100;
constexpr uint32_t kSsrc = 0x12345678;
constexpr uint16_t kSeqNum = 0x1234;
constexpr uint8_t kSeqNumFirstByte = kSeqNum >> 8;
constexpr uint8_t kSeqNumSecondByte = kSeqNum & 0xff;
constexpr uint32_t kTimestamp = 0x65431278;
constexpr uint8_t kTransmissionOffsetExtensionId = 1;
constexpr uint8_t kAudioLevelExtensionId = 9;
constexpr uint8_t kRtpStreamIdExtensionId = 0xa;
constexpr uint8_t kRtpMidExtensionId = 0xb;
constexpr uint8_t kVideoTimingExtensionId = 0xc;
constexpr int32_t kTimeOffset = 0x56ce;
constexpr bool kVoiceActive = true;
constexpr uint8_t kAudioLevel = 0x5a;
constexpr char kStreamId[] = "streamid";
constexpr char kMid[] = "mid";
constexpr size_t kMaxPaddingSize = 224u;
// clang-format off
constexpr uint8_t kMinimumPacket[] = {
    0x80, kPayloadType, kSeqNumFirstByte, kSeqNumSecondByte,
    0x65, 0x43, 0x12, 0x78,
    0x12, 0x34, 0x56, 0x78};

constexpr uint8_t kPacketWithTO[] = {
    0x90, kPayloadType, kSeqNumFirstByte, kSeqNumSecondByte,
    0x65, 0x43, 0x12, 0x78,
    0x12, 0x34, 0x56, 0x78,
    0xbe, 0xde, 0x00, 0x01,
    0x12, 0x00, 0x56, 0xce};

constexpr uint8_t kPacketWithTOAndAL[] = {
    0x90, kPayloadType, kSeqNumFirstByte, kSeqNumSecondByte,
    0x65, 0x43, 0x12, 0x78,
    0x12, 0x34, 0x56, 0x78,
    0xbe, 0xde, 0x00, 0x02,
    0x12, 0x00, 0x56, 0xce,
    0x90, 0x80|kAudioLevel, 0x00, 0x00};

constexpr uint8_t kPacketWithRsid[] = {
    0x90, kPayloadType, kSeqNumFirstByte, kSeqNumSecondByte,
    0x65, 0x43, 0x12, 0x78,
    0x12, 0x34, 0x56, 0x78,
    0xbe, 0xde, 0x00, 0x03,
    0xa7, 's',  't',  'r',
    'e',  'a',  'm',  'i',
    'd' , 0x00, 0x00, 0x00};

constexpr uint8_t kPacketWithMid[] = {
    0x90, kPayloadType, kSeqNumFirstByte, kSeqNumSecondByte,
    0x65, 0x43, 0x12, 0x78,
    0x12, 0x34, 0x56, 0x78,
    0xbe, 0xde, 0x00, 0x01,
    0xb2, 'm', 'i', 'd'};

constexpr uint8_t kCsrcAudioLevelExtensionId = 0xc;
constexpr uint8_t kCsrcAudioLevelsSize = 4;
constexpr uint8_t kCsrcAudioLevels[] = {0x7f, 0x00, 0x10, 0x08};
constexpr uint8_t kPacketWithCsrcAudioLevels[] = {
    0x90, kPayloadType, kSeqNumFirstByte, kSeqNumSecondByte,
    0x65, 0x43, 0x12, 0x78,
    0x12, 0x34, 0x56, 0x78,
    0xbe, 0xde, 0x00, 0x02,
    (kCsrcAudioLevelExtensionId << 4) | (kCsrcAudioLevelsSize - 1),
          0x7f, 0x00, 0x10,
    0x08, 0x00, 0x00, 0x00};

constexpr uint32_t kCsrcs[] = {0x34567890, 0x32435465};
constexpr uint8_t kPayload[] = {'p', 'a', 'y', 'l', 'o', 'a', 'd'};
constexpr uint8_t kPacketPaddingSize = 8;
constexpr uint8_t kPacket[] = {
    0xb2, kPayloadType, kSeqNumFirstByte, kSeqNumSecondByte,
    0x65, 0x43, 0x12, 0x78,
    0x12, 0x34, 0x56, 0x78,
    0x34, 0x56, 0x78, 0x90,
    0x32, 0x43, 0x54, 0x65,
    0xbe, 0xde, 0x00, 0x01,
    0x12, 0x00, 0x56, 0xce,
    'p', 'a', 'y', 'l', 'o', 'a', 'd',
    'p', 'a', 'd', 'd', 'i', 'n', 'g', kPacketPaddingSize};

constexpr uint8_t kPacketWithInvalidExtension[] = {
    0x90, kPayloadType, kSeqNumFirstByte, kSeqNumSecondByte,
    0x65, 0x43, 0x12, 0x78,  // kTimestamp.
    0x12, 0x34, 0x56, 0x78,  // kSSrc.
    0xbe, 0xde, 0x00, 0x02,  // Extension block of size 2 x 32bit words.
    (kTransmissionOffsetExtensionId << 4) | 6,  // (6+1)-byte extension, but
           'e',  'x',  't',                     // Transmission Offset
     'd',  'a',  't',  'a',                     // expected to be 3-bytes.
     'p',  'a',  'y',  'l',  'o',  'a',  'd'};

constexpr uint8_t kPacketWithLegacyTimingExtension[] = {
    0x90, kPayloadType, kSeqNumFirstByte, kSeqNumSecondByte,
    0x65, 0x43, 0x12, 0x78,  // kTimestamp.
    0x12, 0x34, 0x56, 0x78,  // kSSrc.
    0xbe, 0xde, 0x00, 0x04,    // Extension block of size 4 x 32bit words.
    (kVideoTimingExtensionId << 4)
      | VideoTimingExtension::kValueSizeBytes - 2,  // Old format without flags.
          0x00, 0x01, 0x00,
    0x02, 0x00, 0x03, 0x00,
    0x04, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00};
// clang-format on
}  // namespace

TEST(RtpPacketTest, CreateMinimum) {
  RtpPacketToSend packet(nullptr);
  packet.SetPayloadType(kPayloadType);
  packet.SetSequenceNumber(kSeqNum);
  packet.SetTimestamp(kTimestamp);
  packet.SetSsrc(kSsrc);
  EXPECT_THAT(kMinimumPacket, ElementsAreArray(packet.data(), packet.size()));
}

TEST(RtpPacketTest, CreateWithExtension) {
  RtpPacketToSend::ExtensionManager extensions;
  extensions.Register(kRtpExtensionTransmissionTimeOffset,
                      kTransmissionOffsetExtensionId);
  RtpPacketToSend packet(&extensions);
  packet.SetPayloadType(kPayloadType);
  packet.SetSequenceNumber(kSeqNum);
  packet.SetTimestamp(kTimestamp);
  packet.SetSsrc(kSsrc);
  packet.SetExtension<TransmissionOffset>(kTimeOffset);
  EXPECT_THAT(kPacketWithTO, ElementsAreArray(packet.data(), packet.size()));
}

TEST(RtpPacketTest, CreateWith2Extensions) {
  RtpPacketToSend::ExtensionManager extensions;
  extensions.Register(kRtpExtensionTransmissionTimeOffset,
                      kTransmissionOffsetExtensionId);
  extensions.Register(kRtpExtensionAudioLevel, kAudioLevelExtensionId);
  RtpPacketToSend packet(&extensions);
  packet.SetPayloadType(kPayloadType);
  packet.SetSequenceNumber(kSeqNum);
  packet.SetTimestamp(kTimestamp);
  packet.SetSsrc(kSsrc);
  packet.SetExtension<TransmissionOffset>(kTimeOffset);
  packet.SetExtension<AudioLevel>(kVoiceActive, kAudioLevel);
  EXPECT_THAT(kPacketWithTOAndAL,
              ElementsAreArray(packet.data(), packet.size()));
}

TEST(RtpPacketTest, CreateWithDynamicSizedExtensions) {
  RtpPacketToSend::ExtensionManager extensions;
  extensions.Register<RtpStreamId>(kRtpStreamIdExtensionId);
  RtpPacketToSend packet(&extensions);
  packet.SetPayloadType(kPayloadType);
  packet.SetSequenceNumber(kSeqNum);
  packet.SetTimestamp(kTimestamp);
  packet.SetSsrc(kSsrc);
  packet.SetExtension<RtpStreamId>(kStreamId);
  EXPECT_THAT(kPacketWithRsid, ElementsAreArray(packet.data(), packet.size()));
}

TEST(RtpPacketTest, TryToCreateWithEmptyRsid) {
  RtpPacketToSend::ExtensionManager extensions;
  extensions.Register<RtpStreamId>(kRtpStreamIdExtensionId);
  RtpPacketToSend packet(&extensions);
  EXPECT_FALSE(packet.SetExtension<RtpStreamId>(""));
}

TEST(RtpPacketTest, TryToCreateWithLongRsid) {
  RtpPacketToSend::ExtensionManager extensions;
  constexpr char kLongStreamId[] = "LoooooooooongRsid";
  ASSERT_EQ(strlen(kLongStreamId), 17u);
  extensions.Register<RtpStreamId>(kRtpStreamIdExtensionId);
  RtpPacketToSend packet(&extensions);
  EXPECT_FALSE(packet.SetExtension<RtpStreamId>(kLongStreamId));
}

TEST(RtpPacketTest, TryToCreateWithEmptyMid) {
  RtpPacketToSend::ExtensionManager extensions;
  extensions.Register<RtpMid>(kRtpMidExtensionId);
  RtpPacketToSend packet(&extensions);
  EXPECT_FALSE(packet.SetExtension<RtpMid>(""));
}

TEST(RtpPacketTest, TryToCreateWithLongMid) {
  RtpPacketToSend::ExtensionManager extensions;
  constexpr char kLongMid[] = "LoooooooooonogMid";
  ASSERT_EQ(strlen(kLongMid), 17u);
  extensions.Register<RtpMid>(kRtpMidExtensionId);
  RtpPacketToSend packet(&extensions);
  EXPECT_FALSE(packet.SetExtension<RtpMid>(kLongMid));
}

TEST(RtpPacketTest, CreateWithExtensionsWithoutManager) {
  RtpPacketToSend packet(nullptr);
  packet.SetPayloadType(kPayloadType);
  packet.SetSequenceNumber(kSeqNum);
  packet.SetTimestamp(kTimestamp);
  packet.SetSsrc(kSsrc);

  auto raw = packet.AllocateRawExtension(kTransmissionOffsetExtensionId,
                                         TransmissionOffset::kValueSizeBytes);
  EXPECT_EQ(raw.size(), TransmissionOffset::kValueSizeBytes);
  TransmissionOffset::Write(raw.data(), kTimeOffset);

  raw = packet.AllocateRawExtension(kAudioLevelExtensionId,
                                    AudioLevel::kValueSizeBytes);
  EXPECT_EQ(raw.size(), AudioLevel::kValueSizeBytes);
  AudioLevel::Write(raw.data(), kVoiceActive, kAudioLevel);

  EXPECT_THAT(kPacketWithTOAndAL,
              ElementsAreArray(packet.data(), packet.size()));
}

TEST(RtpPacketTest, CreateWithMaxSizeHeaderExtension) {
  const size_t kMaxExtensionSize = 16;
  const int kId = 1;
  const uint8_t kValue[16] = "123456789abcdef";

  // Write packet with a custom extension.
  RtpPacketToSend packet(nullptr);
  packet.SetRawExtension(kId, kValue);
  // Using different size for same id is not allowed.
  EXPECT_TRUE(packet.AllocateRawExtension(kId, kMaxExtensionSize - 1).empty());

  packet.SetPayloadSize(42);
  // Rewriting allocated extension is allowed.
  EXPECT_EQ(packet.AllocateRawExtension(kId, kMaxExtensionSize).size(),
            kMaxExtensionSize);
  // Adding another extension after payload is set is not allowed.
  EXPECT_TRUE(packet.AllocateRawExtension(kId + 1, kMaxExtensionSize).empty());

  // Read packet with the custom extension.
  RtpPacketReceived parsed;
  EXPECT_TRUE(parsed.Parse(packet.Buffer()));
  auto read_raw = parsed.GetRawExtension(kId);
  EXPECT_THAT(read_raw, ElementsAreArray(kValue, kMaxExtensionSize));
}

TEST(RtpPacketTest, CreateWithDynamicSizedExtensionCsrcAudioLevel) {
  RtpPacketToSend::ExtensionManager extensions;
  extensions.Register<CsrcAudioLevel>(kCsrcAudioLevelExtensionId);
  RtpPacketToSend packet(&extensions);
  packet.SetPayloadType(kPayloadType);
  packet.SetSequenceNumber(kSeqNum);
  packet.SetTimestamp(kTimestamp);
  packet.SetSsrc(kSsrc);
  CsrcAudioLevelList levels;
  levels.numAudioLevels = kCsrcAudioLevelsSize;
  for (uint8_t i = 0; i < kCsrcAudioLevelsSize; i++) {
    levels.arrOfAudioLevels[i] = kCsrcAudioLevels[i];
  }
  packet.SetExtension<CsrcAudioLevel>(levels);
  EXPECT_THAT(kPacketWithCsrcAudioLevels,
    ElementsAreArray(packet.data(), packet.size()));
}

TEST(RtpPacketTest, SetReservedExtensionsAfterPayload) {
  const size_t kPayloadSize = 4;
  RtpPacketToSend::ExtensionManager extensions;
  extensions.Register(kRtpExtensionTransmissionTimeOffset,
                      kTransmissionOffsetExtensionId);
  extensions.Register(kRtpExtensionAudioLevel, kAudioLevelExtensionId);
  RtpPacketToSend packet(&extensions);

  EXPECT_TRUE(packet.ReserveExtension<TransmissionOffset>());
  packet.SetPayloadSize(kPayloadSize);
  // Can't set extension after payload.
  EXPECT_FALSE(packet.SetExtension<AudioLevel>(kVoiceActive, kAudioLevel));
  // Unless reserved.
  EXPECT_TRUE(packet.SetExtension<TransmissionOffset>(kTimeOffset));
}

TEST(RtpPacketTest, CreatePurePadding) {
  const size_t kPaddingSize = kMaxPaddingSize - 1;
  RtpPacketToSend packet(nullptr, 12 + kPaddingSize);
  packet.SetPayloadType(kPayloadType);
  packet.SetSequenceNumber(kSeqNum);
  packet.SetTimestamp(kTimestamp);
  packet.SetSsrc(kSsrc);
  Random random(0x123456789);

  EXPECT_LT(packet.size(), packet.capacity());
  EXPECT_FALSE(packet.SetPadding(kPaddingSize + 1, &random));
  EXPECT_TRUE(packet.SetPadding(kPaddingSize, &random));
  EXPECT_EQ(packet.size(), packet.capacity());
}

TEST(RtpPacketTest, CreateUnalignedPadding) {
  const size_t kPayloadSize = 3;  // Make padding start at unaligned address.
  RtpPacketToSend packet(nullptr, 12 + kPayloadSize + kMaxPaddingSize);
  packet.SetPayloadType(kPayloadType);
  packet.SetSequenceNumber(kSeqNum);
  packet.SetTimestamp(kTimestamp);
  packet.SetSsrc(kSsrc);
  packet.SetPayloadSize(kPayloadSize);
  Random r(0x123456789);

  EXPECT_LT(packet.size(), packet.capacity());
  EXPECT_TRUE(packet.SetPadding(kMaxPaddingSize, &r));
  EXPECT_EQ(packet.size(), packet.capacity());
}

TEST(RtpPacketTest, ParseMinimum) {
  RtpPacketReceived packet;
  EXPECT_TRUE(packet.Parse(kMinimumPacket, sizeof(kMinimumPacket)));
  EXPECT_EQ(kPayloadType, packet.PayloadType());
  EXPECT_EQ(kSeqNum, packet.SequenceNumber());
  EXPECT_EQ(kTimestamp, packet.Timestamp());
  EXPECT_EQ(kSsrc, packet.Ssrc());
  EXPECT_EQ(0u, packet.padding_size());
  EXPECT_EQ(0u, packet.payload_size());
}

TEST(RtpPacketTest, ParseBuffer) {
  rtc::CopyOnWriteBuffer unparsed(kMinimumPacket);
  const uint8_t* raw = unparsed.data();

  RtpPacketReceived packet;
  EXPECT_TRUE(packet.Parse(std::move(unparsed)));
  EXPECT_EQ(raw, packet.data());  // Expect packet take the buffer without copy.
  EXPECT_EQ(kSeqNum, packet.SequenceNumber());
  EXPECT_EQ(kTimestamp, packet.Timestamp());
  EXPECT_EQ(kSsrc, packet.Ssrc());
  EXPECT_EQ(0u, packet.padding_size());
  EXPECT_EQ(0u, packet.payload_size());
}

TEST(RtpPacketTest, ParseWithExtension) {
  RtpPacketToSend::ExtensionManager extensions;
  extensions.Register(kRtpExtensionTransmissionTimeOffset,
                      kTransmissionOffsetExtensionId);

  RtpPacketReceived packet(&extensions);
  EXPECT_TRUE(packet.Parse(kPacketWithTO, sizeof(kPacketWithTO)));
  EXPECT_EQ(kPayloadType, packet.PayloadType());
  EXPECT_EQ(kSeqNum, packet.SequenceNumber());
  EXPECT_EQ(kTimestamp, packet.Timestamp());
  EXPECT_EQ(kSsrc, packet.Ssrc());
  int32_t time_offset;
  EXPECT_TRUE(packet.GetExtension<TransmissionOffset>(&time_offset));
  EXPECT_EQ(kTimeOffset, time_offset);
  EXPECT_EQ(0u, packet.payload_size());
  EXPECT_EQ(0u, packet.padding_size());
}

TEST(RtpPacketTest, ParseWithInvalidSizedExtension) {
  RtpPacketToSend::ExtensionManager extensions;
  extensions.Register(kRtpExtensionTransmissionTimeOffset,
                      kTransmissionOffsetExtensionId);

  RtpPacketReceived packet(&extensions);
  EXPECT_TRUE(packet.Parse(kPacketWithInvalidExtension,
                           sizeof(kPacketWithInvalidExtension)));

  // Extension should be ignored.
  int32_t time_offset;
  EXPECT_FALSE(packet.GetExtension<TransmissionOffset>(&time_offset));

  // But shouldn't prevent reading payload.
  EXPECT_THAT(packet.payload(), ElementsAreArray(kPayload));
}

TEST(RtpPacketTest, ParseWithOverSizedExtension) {
  // clang-format off
  const uint8_t bad_packet[] = {
      0x90, kPayloadType, kSeqNumFirstByte, kSeqNumSecondByte,
      0x65, 0x43, 0x12, 0x78,  // kTimestamp.
      0x12, 0x34, 0x56, 0x78,  // kSsrc.
      0xbe, 0xde, 0x00, 0x01,  // Extension of size 1x32bit word.
      0x00,  // Add a byte of padding.
            0x12,  // Extension id 1 size (2+1).
                  0xda, 0x1a  // Only 2 bytes of extension payload.
  };
  // clang-format on
  RtpPacketToSend::ExtensionManager extensions;
  extensions.Register(TransmissionOffset::kId, 1);
  RtpPacketReceived packet(&extensions);

  // Parse should ignore bad extension and proceed.
  EXPECT_TRUE(packet.Parse(bad_packet, sizeof(bad_packet)));
  int32_t time_offset;
  // But extracting extension should fail.
  EXPECT_FALSE(packet.GetExtension<TransmissionOffset>(&time_offset));
}

TEST(RtpPacketTest, ParseWith2Extensions) {
  RtpPacketToSend::ExtensionManager extensions;
  extensions.Register(kRtpExtensionTransmissionTimeOffset,
                      kTransmissionOffsetExtensionId);
  extensions.Register(kRtpExtensionAudioLevel, kAudioLevelExtensionId);
  RtpPacketReceived packet(&extensions);
  EXPECT_TRUE(packet.Parse(kPacketWithTOAndAL, sizeof(kPacketWithTOAndAL)));
  int32_t time_offset;
  EXPECT_TRUE(packet.GetExtension<TransmissionOffset>(&time_offset));
  EXPECT_EQ(kTimeOffset, time_offset);
  bool voice_active;
  uint8_t audio_level;
  EXPECT_TRUE(packet.GetExtension<AudioLevel>(&voice_active, &audio_level));
  EXPECT_EQ(kVoiceActive, voice_active);
  EXPECT_EQ(kAudioLevel, audio_level);
}

TEST(RtpPacketTest, ParseWithAllFeatures) {
  RtpPacketToSend::ExtensionManager extensions;
  extensions.Register(kRtpExtensionTransmissionTimeOffset,
                      kTransmissionOffsetExtensionId);
  RtpPacketReceived packet(&extensions);
  EXPECT_TRUE(packet.Parse(kPacket, sizeof(kPacket)));
  EXPECT_EQ(kPayloadType, packet.PayloadType());
  EXPECT_EQ(kSeqNum, packet.SequenceNumber());
  EXPECT_EQ(kTimestamp, packet.Timestamp());
  EXPECT_EQ(kSsrc, packet.Ssrc());
  EXPECT_THAT(packet.Csrcs(), ElementsAreArray(kCsrcs));
  EXPECT_THAT(packet.payload(), ElementsAreArray(kPayload));
  EXPECT_EQ(kPacketPaddingSize, packet.padding_size());
  int32_t time_offset;
  EXPECT_TRUE(packet.GetExtension<TransmissionOffset>(&time_offset));
}

TEST(RtpPacketTest, ParseWithExtensionDelayed) {
  RtpPacketReceived packet;
  EXPECT_TRUE(packet.Parse(kPacketWithTO, sizeof(kPacketWithTO)));
  EXPECT_EQ(kPayloadType, packet.PayloadType());
  EXPECT_EQ(kSeqNum, packet.SequenceNumber());
  EXPECT_EQ(kTimestamp, packet.Timestamp());
  EXPECT_EQ(kSsrc, packet.Ssrc());

  RtpPacketToSend::ExtensionManager extensions;
  extensions.Register(kRtpExtensionTransmissionTimeOffset,
                      kTransmissionOffsetExtensionId);

  int32_t time_offset;
  EXPECT_FALSE(packet.GetExtension<TransmissionOffset>(&time_offset));
  packet.IdentifyExtensions(extensions);
  EXPECT_TRUE(packet.GetExtension<TransmissionOffset>(&time_offset));
  EXPECT_EQ(kTimeOffset, time_offset);
  EXPECT_EQ(0u, packet.payload_size());
  EXPECT_EQ(0u, packet.padding_size());
}

TEST(RtpPacketTest, ParseWithoutExtensionManager) {
  RtpPacketReceived packet;
  EXPECT_TRUE(packet.Parse(kPacketWithTO, sizeof(kPacketWithTO)));

  EXPECT_FALSE(packet.HasRawExtension(kAudioLevelExtensionId));
  EXPECT_TRUE(packet.GetRawExtension(kAudioLevelExtensionId).empty());

  EXPECT_TRUE(packet.HasRawExtension(kTransmissionOffsetExtensionId));

  int32_t time_offset = 0;
  auto raw_extension = packet.GetRawExtension(kTransmissionOffsetExtensionId);
  EXPECT_EQ(raw_extension.size(), TransmissionOffset::kValueSizeBytes);
  EXPECT_TRUE(TransmissionOffset::Parse(raw_extension, &time_offset));

  EXPECT_EQ(time_offset, kTimeOffset);
}

TEST(RtpPacketTest, ParseDynamicSizeExtension) {
  // clang-format off
  const uint8_t kPacket1[] = {
    0x90, kPayloadType, kSeqNumFirstByte, kSeqNumSecondByte,
    0x65, 0x43, 0x12, 0x78,  // Timestamp.
    0x12, 0x34, 0x56, 0x78,  // Ssrc.
    0xbe, 0xde, 0x00, 0x02,  // Extensions block of size 2x32bit words.
    0x21, 'H', 'D',          // Extension with id = 2, size = (1+1).
    0x12, 'r', 't', 'x',     // Extension with id = 1, size = (2+1).
    0x00};  // Extension padding.
  const uint8_t kPacket2[] = {
    0x90, kPayloadType, kSeqNumFirstByte, kSeqNumSecondByte,
    0x65, 0x43, 0x12, 0x78,  // Timestamp.
    0x12, 0x34, 0x56, 0x79,  // Ssrc.
    0xbe, 0xde, 0x00, 0x01,  // Extensions block of size 1x32bit words.
    0x11, 'H', 'D',          // Extension with id = 1, size = (1+1).
    0x00};  // Extension padding.
  // clang-format on
  RtpPacketReceived::ExtensionManager extensions;
  extensions.Register<RtpStreamId>(1);
  extensions.Register<RepairedRtpStreamId>(2);
  RtpPacketReceived packet(&extensions);
  ASSERT_TRUE(packet.Parse(kPacket1, sizeof(kPacket1)));

  std::string rsid;
  EXPECT_TRUE(packet.GetExtension<RtpStreamId>(&rsid));
  EXPECT_EQ(rsid, "rtx");

  std::string repaired_rsid;
  EXPECT_TRUE(packet.GetExtension<RepairedRtpStreamId>(&repaired_rsid));
  EXPECT_EQ(repaired_rsid, "HD");

  // Parse another packet with RtpStreamId extension of different size.
  ASSERT_TRUE(packet.Parse(kPacket2, sizeof(kPacket2)));
  EXPECT_TRUE(packet.GetExtension<RtpStreamId>(&rsid));
  EXPECT_EQ(rsid, "HD");
  EXPECT_FALSE(packet.GetExtension<RepairedRtpStreamId>(&repaired_rsid));
}

TEST(RtpPacketTest, ParseWithMid) {
  RtpPacketReceived::ExtensionManager extensions;
  extensions.Register<RtpMid>(kRtpMidExtensionId);
  RtpPacketReceived packet(&extensions);
  ASSERT_TRUE(packet.Parse(kPacketWithMid, sizeof(kPacketWithMid)));

  std::string mid;
  EXPECT_TRUE(packet.GetExtension<RtpMid>(&mid));
  EXPECT_EQ(mid, kMid);
}

TEST(RtpPacketTest, RawExtensionFunctionsAcceptZeroIdAndReturnFalse) {
  RtpPacketReceived::ExtensionManager extensions;
  RtpPacketReceived packet(&extensions);
  // Use ExtensionManager to set kInvalidId to 0 to demonstrate natural way for
  // using zero value as a parameter to Packet::*RawExtension functions.
  const int kInvalidId = extensions.GetId(TransmissionOffset::kId);
  ASSERT_EQ(kInvalidId, 0);

  ASSERT_TRUE(packet.Parse(kPacket, sizeof(kPacket)));

  EXPECT_FALSE(packet.HasRawExtension(kInvalidId));
  EXPECT_THAT(packet.GetRawExtension(kInvalidId), IsEmpty());
  const uint8_t kExtension[] = {'e', 'x', 't'};
  EXPECT_FALSE(packet.SetRawExtension(kInvalidId, kExtension));
  EXPECT_THAT(packet.AllocateRawExtension(kInvalidId, 3), IsEmpty());
}

TEST(RtpPacketTest, CreateAndParseTimingFrameExtension) {
  // Create a packet with video frame timing extension populated.
  RtpPacketToSend::ExtensionManager send_extensions;
  send_extensions.Register(kRtpExtensionVideoTiming, kVideoTimingExtensionId);
  RtpPacketToSend send_packet(&send_extensions);
  send_packet.SetPayloadType(kPayloadType);
  send_packet.SetSequenceNumber(kSeqNum);
  send_packet.SetTimestamp(kTimestamp);
  send_packet.SetSsrc(kSsrc);

  VideoSendTiming timing;
  timing.encode_start_delta_ms = 1;
  timing.encode_finish_delta_ms = 2;
  timing.packetization_finish_delta_ms = 3;
  timing.pacer_exit_delta_ms = 4;
  timing.flags =
      TimingFrameFlags::kTriggeredByTimer + TimingFrameFlags::kTriggeredBySize;

  send_packet.SetExtension<VideoTimingExtension>(timing);

  // Serialize the packet and then parse it again.
  RtpPacketReceived::ExtensionManager extensions;
  extensions.Register<VideoTimingExtension>(kVideoTimingExtensionId);
  RtpPacketReceived receive_packet(&extensions);
  EXPECT_TRUE(receive_packet.Parse(send_packet.Buffer()));

  VideoSendTiming receivied_timing;
  EXPECT_TRUE(
      receive_packet.GetExtension<VideoTimingExtension>(&receivied_timing));

  // Only check first and last timestamp (covered by other tests) plus flags.
  EXPECT_EQ(receivied_timing.encode_start_delta_ms,
            timing.encode_start_delta_ms);
  EXPECT_EQ(receivied_timing.pacer_exit_delta_ms, timing.pacer_exit_delta_ms);
  EXPECT_EQ(receivied_timing.flags, timing.flags);
}

TEST(RtpPacketTest, ParseLegacyTimingFrameExtension) {
  // Parse the modified packet.
  RtpPacketReceived::ExtensionManager extensions;
  extensions.Register<VideoTimingExtension>(kVideoTimingExtensionId);
  RtpPacketReceived packet(&extensions);
  EXPECT_TRUE(packet.Parse(kPacketWithLegacyTimingExtension,
                           sizeof(kPacketWithLegacyTimingExtension)));
  VideoSendTiming receivied_timing;
  EXPECT_TRUE(packet.GetExtension<VideoTimingExtension>(&receivied_timing));

  // Check first and last timestamp are still OK. Flags should now be 0.
  EXPECT_EQ(receivied_timing.encode_start_delta_ms, 1);
  EXPECT_EQ(receivied_timing.pacer_exit_delta_ms, 4);
  EXPECT_EQ(receivied_timing.flags, 0);
}

}  // namespace webrtc
