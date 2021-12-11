/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/decoder_database.h"

#include <assert.h>
#include <stdlib.h>

#include <string>

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "rtc_base/refcountedobject.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_audio_decoder.h"
#include "test/mock_audio_decoder_factory.h"

using testing::_;
using testing::Invoke;

namespace webrtc {

TEST(DecoderDatabase, CreateAndDestroy) {
  DecoderDatabase db(new rtc::RefCountedObject<MockAudioDecoderFactory>);
  EXPECT_EQ(0, db.Size());
  EXPECT_TRUE(db.Empty());
}

TEST(DecoderDatabase, InsertAndRemove) {
  rtc::scoped_refptr<MockAudioDecoderFactory> factory(
      new rtc::RefCountedObject<MockAudioDecoderFactory>);
  EXPECT_CALL(*factory, IsSupportedDecoder(_))
      .WillOnce(Invoke([](const SdpAudioFormat& format) {
        EXPECT_EQ("pcmu", format.name);
        return true;
      }));
  DecoderDatabase db(factory);
  const uint8_t kPayloadType = 0;
  const std::string kCodecName = "Robert\'); DROP TABLE Students;";
  EXPECT_EQ(
      DecoderDatabase::kOK,
      db.RegisterPayload(kPayloadType, NetEqDecoder::kDecoderPCMu, kCodecName));
  EXPECT_EQ(1, db.Size());
  EXPECT_FALSE(db.Empty());
  EXPECT_EQ(DecoderDatabase::kOK, db.Remove(kPayloadType));
  EXPECT_EQ(0, db.Size());
  EXPECT_TRUE(db.Empty());
}

TEST(DecoderDatabase, InsertAndRemoveAll) {
  rtc::scoped_refptr<MockAudioDecoderFactory> factory(
      new rtc::RefCountedObject<MockAudioDecoderFactory>);
  EXPECT_CALL(*factory, IsSupportedDecoder(_))
      .WillOnce(Invoke([](const SdpAudioFormat& format) {
        EXPECT_EQ("pcmu", format.name);
        return true;
      }))
      .WillOnce(Invoke([](const SdpAudioFormat& format) {
        EXPECT_EQ("pcma", format.name);
        return true;
      }));
  DecoderDatabase db(factory);
  const std::string kCodecName1 = "Robert\'); DROP TABLE Students;";
  const std::string kCodecName2 = "https://xkcd.com/327/";
  EXPECT_EQ(DecoderDatabase::kOK,
            db.RegisterPayload(0, NetEqDecoder::kDecoderPCMu, kCodecName1));
  EXPECT_EQ(DecoderDatabase::kOK,
            db.RegisterPayload(1, NetEqDecoder::kDecoderPCMa, kCodecName2));
  EXPECT_EQ(2, db.Size());
  EXPECT_FALSE(db.Empty());
  db.RemoveAll();
  EXPECT_EQ(0, db.Size());
  EXPECT_TRUE(db.Empty());
}

TEST(DecoderDatabase, GetDecoderInfo) {
  rtc::scoped_refptr<MockAudioDecoderFactory> factory(
      new rtc::RefCountedObject<MockAudioDecoderFactory>);
  EXPECT_CALL(*factory, IsSupportedDecoder(_))
      .WillOnce(Invoke([](const SdpAudioFormat& format) {
        EXPECT_EQ("pcmu", format.name);
        return true;
      }));
  auto* decoder = new MockAudioDecoder;
  EXPECT_CALL(*factory, MakeAudioDecoderMock(_, _))
      .WillOnce(Invoke([decoder](const SdpAudioFormat& format,
                                 std::unique_ptr<AudioDecoder>* dec) {
        EXPECT_EQ("pcmu", format.name);
        dec->reset(decoder);
      }));
  DecoderDatabase db(factory);
  const uint8_t kPayloadType = 0;
  const std::string kCodecName = "Robert\'); DROP TABLE Students;";
  EXPECT_EQ(
      DecoderDatabase::kOK,
      db.RegisterPayload(kPayloadType, NetEqDecoder::kDecoderPCMu, kCodecName));
  const DecoderDatabase::DecoderInfo* info;
  info = db.GetDecoderInfo(kPayloadType);
  ASSERT_TRUE(info != NULL);
  EXPECT_TRUE(info->IsType("pcmu"));
  EXPECT_EQ(kCodecName, info->get_name());
  EXPECT_EQ(decoder, db.GetDecoder(kPayloadType));
  info = db.GetDecoderInfo(kPayloadType + 1);  // Other payload type.
  EXPECT_TRUE(info == NULL);  // Should not be found.
}

TEST(DecoderDatabase, GetDecoder) {
  DecoderDatabase db(CreateBuiltinAudioDecoderFactory());
  const uint8_t kPayloadType = 0;
  const std::string kCodecName = "Robert\'); DROP TABLE Students;";
  EXPECT_EQ(DecoderDatabase::kOK,
            db.RegisterPayload(kPayloadType, NetEqDecoder::kDecoderPCM16B,
                               kCodecName));
  AudioDecoder* dec = db.GetDecoder(kPayloadType);
  ASSERT_TRUE(dec != NULL);
}

TEST(DecoderDatabase, TypeTests) {
  rtc::scoped_refptr<MockAudioDecoderFactory> factory(
      new rtc::RefCountedObject<MockAudioDecoderFactory>);
  EXPECT_CALL(*factory, IsSupportedDecoder(_))
      .WillOnce(Invoke([](const SdpAudioFormat& format) {
        EXPECT_EQ("pcmu", format.name);
        return true;
      }));
  DecoderDatabase db(factory);
  const uint8_t kPayloadTypePcmU = 0;
  const uint8_t kPayloadTypeCng = 13;
  const uint8_t kPayloadTypeDtmf = 100;
  const uint8_t kPayloadTypeRed = 101;
  const uint8_t kPayloadNotUsed = 102;
  // Load into database.
  EXPECT_EQ(
      DecoderDatabase::kOK,
      db.RegisterPayload(kPayloadTypePcmU, NetEqDecoder::kDecoderPCMu, "pcmu"));
  EXPECT_EQ(DecoderDatabase::kOK,
            db.RegisterPayload(kPayloadTypeCng, NetEqDecoder::kDecoderCNGnb,
                               "cng-nb"));
  EXPECT_EQ(
      DecoderDatabase::kOK,
      db.RegisterPayload(kPayloadTypeDtmf, NetEqDecoder::kDecoderAVT, "avt"));
  EXPECT_EQ(
      DecoderDatabase::kOK,
      db.RegisterPayload(kPayloadTypeRed, NetEqDecoder::kDecoderRED, "red"));
  EXPECT_EQ(4, db.Size());
  // Test.
  EXPECT_FALSE(db.IsComfortNoise(kPayloadNotUsed));
  EXPECT_FALSE(db.IsDtmf(kPayloadNotUsed));
  EXPECT_FALSE(db.IsRed(kPayloadNotUsed));
  EXPECT_FALSE(db.IsComfortNoise(kPayloadTypePcmU));
  EXPECT_FALSE(db.IsDtmf(kPayloadTypePcmU));
  EXPECT_FALSE(db.IsRed(kPayloadTypePcmU));
  EXPECT_FALSE(db.IsType(kPayloadTypePcmU, "isac"));
  EXPECT_TRUE(db.IsType(kPayloadTypePcmU, "pcmu"));
  EXPECT_TRUE(db.IsComfortNoise(kPayloadTypeCng));
  EXPECT_TRUE(db.IsDtmf(kPayloadTypeDtmf));
  EXPECT_TRUE(db.IsRed(kPayloadTypeRed));
}

TEST(DecoderDatabase, ExternalDecoder) {
  DecoderDatabase db(new rtc::RefCountedObject<MockAudioDecoderFactory>);
  const uint8_t kPayloadType = 0;
  const std::string kCodecName = "Robert\'); DROP TABLE Students;";
  MockAudioDecoder decoder;
  // Load into database.
  EXPECT_EQ(DecoderDatabase::kOK,
            db.InsertExternal(kPayloadType, NetEqDecoder::kDecoderPCMu,
                              kCodecName, &decoder));
  EXPECT_EQ(1, db.Size());
  // Get decoder and make sure we get the external one.
  EXPECT_EQ(&decoder, db.GetDecoder(kPayloadType));
  // Get the decoder info struct and check it too.
  const DecoderDatabase::DecoderInfo* info;
  info = db.GetDecoderInfo(kPayloadType);
  ASSERT_TRUE(info != NULL);
  EXPECT_TRUE(info->IsType("pcmu"));
  EXPECT_EQ(info->get_name(), kCodecName);
  EXPECT_EQ(kCodecName, info->get_name());
  // Expect not to delete the decoder when removing it from the database, since
  // it was declared externally.
  EXPECT_CALL(decoder, Die()).Times(0);
  EXPECT_EQ(DecoderDatabase::kOK, db.Remove(kPayloadType));
  EXPECT_TRUE(db.Empty());

  EXPECT_CALL(decoder, Die()).Times(1);  // Will be called when |db| is deleted.
}

TEST(DecoderDatabase, CheckPayloadTypes) {
  constexpr int kNumPayloads = 10;
  rtc::scoped_refptr<MockAudioDecoderFactory> factory(
      new rtc::RefCountedObject<MockAudioDecoderFactory>);
  EXPECT_CALL(*factory, IsSupportedDecoder(_))
      .Times(kNumPayloads)
      .WillRepeatedly(Invoke([](const SdpAudioFormat& format) {
        EXPECT_EQ("pcmu", format.name);
        return true;
      }));
  DecoderDatabase db(factory);
  // Load a number of payloads into the database. Payload types are 0, 1, ...,
  // while the decoder type is the same for all payload types (this does not
  // matter for the test).
  for (uint8_t payload_type = 0; payload_type < kNumPayloads; ++payload_type) {
    EXPECT_EQ(DecoderDatabase::kOK,
              db.RegisterPayload(payload_type, NetEqDecoder::kDecoderPCMu, ""));
  }
  PacketList packet_list;
  for (int i = 0; i < kNumPayloads + 1; ++i) {
    // Create packet with payload type |i|. The last packet will have a payload
    // type that is not registered in the decoder database.
    Packet packet;
    packet.payload_type = i;
    packet_list.push_back(std::move(packet));
  }

  // Expect to return false, since the last packet is of an unknown type.
  EXPECT_EQ(DecoderDatabase::kDecoderNotFound,
            db.CheckPayloadTypes(packet_list));

  packet_list.pop_back();  // Remove the unknown one.

  EXPECT_EQ(DecoderDatabase::kOK, db.CheckPayloadTypes(packet_list));

  // Delete all packets.
  PacketList::iterator it = packet_list.begin();
  while (it != packet_list.end()) {
    it = packet_list.erase(it);
  }
}

#if defined(WEBRTC_CODEC_ISAC) || defined(WEBRTC_CODEC_ISACFX)
#define IF_ISAC(x) x
#else
#define IF_ISAC(x) DISABLED_##x
#endif

// Test the methods for setting and getting active speech and CNG decoders.
TEST(DecoderDatabase, IF_ISAC(ActiveDecoders)) {
  DecoderDatabase db(CreateBuiltinAudioDecoderFactory());
  // Load payload types.
  ASSERT_EQ(DecoderDatabase::kOK,
            db.RegisterPayload(0, NetEqDecoder::kDecoderPCMu, "pcmu"));
  ASSERT_EQ(DecoderDatabase::kOK,
            db.RegisterPayload(103, NetEqDecoder::kDecoderISAC, "isac"));
  ASSERT_EQ(DecoderDatabase::kOK,
            db.RegisterPayload(13, NetEqDecoder::kDecoderCNGnb, "cng-nb"));
  // Verify that no decoders are active from the start.
  EXPECT_EQ(NULL, db.GetActiveDecoder());
  EXPECT_EQ(NULL, db.GetActiveCngDecoder());

  // Set active speech codec.
  bool changed;  // Should be true when the active decoder changed.
  EXPECT_EQ(DecoderDatabase::kOK, db.SetActiveDecoder(0, &changed));
  EXPECT_TRUE(changed);
  AudioDecoder* decoder = db.GetActiveDecoder();
  ASSERT_FALSE(decoder == NULL);  // Should get a decoder here.

  // Set the same again. Expect no change.
  EXPECT_EQ(DecoderDatabase::kOK, db.SetActiveDecoder(0, &changed));
  EXPECT_FALSE(changed);
  decoder = db.GetActiveDecoder();
  ASSERT_FALSE(decoder == NULL);  // Should get a decoder here.

  // Change active decoder.
  EXPECT_EQ(DecoderDatabase::kOK, db.SetActiveDecoder(103, &changed));
  EXPECT_TRUE(changed);
  decoder = db.GetActiveDecoder();
  ASSERT_FALSE(decoder == NULL);  // Should get a decoder here.

  // Remove the active decoder, and verify that the active becomes NULL.
  EXPECT_EQ(DecoderDatabase::kOK, db.Remove(103));
  EXPECT_EQ(NULL, db.GetActiveDecoder());

  // Set active CNG codec.
  EXPECT_EQ(DecoderDatabase::kOK, db.SetActiveCngDecoder(13));
  ComfortNoiseDecoder* cng = db.GetActiveCngDecoder();
  ASSERT_FALSE(cng == NULL);  // Should get a decoder here.

  // Remove the active CNG decoder, and verify that the active becomes NULL.
  EXPECT_EQ(DecoderDatabase::kOK, db.Remove(13));
  EXPECT_EQ(NULL, db.GetActiveCngDecoder());

  // Try to set non-existing codecs as active.
  EXPECT_EQ(DecoderDatabase::kDecoderNotFound,
            db.SetActiveDecoder(17, &changed));
  EXPECT_EQ(DecoderDatabase::kDecoderNotFound,
            db.SetActiveCngDecoder(17));
}
}  // namespace webrtc
