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

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/audio_coding/codecs/red/audio_encoder_copy_red.h"
#include "webrtc/modules/audio_coding/codecs/mock/mock_audio_encoder.h"

using ::testing::Return;
using ::testing::_;
using ::testing::SetArgPointee;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::MockFunction;

namespace webrtc {

namespace {
static const size_t kMockMaxEncodedBytes = 1000;
static const size_t kMaxNumSamples = 48 * 10 * 2;  // 10 ms @ 48 kHz stereo.
}

class AudioEncoderCopyRedTest : public ::testing::Test {
 protected:
  AudioEncoderCopyRedTest()
      : timestamp_(4711),
        sample_rate_hz_(16000),
        num_audio_samples_10ms(sample_rate_hz_ / 100),
        red_payload_type_(200) {
    AudioEncoderCopyRed::Config config;
    config.payload_type = red_payload_type_;
    config.speech_encoder = &mock_encoder_;
    red_.reset(new AudioEncoderCopyRed(config));
    memset(audio_, 0, sizeof(audio_));
    EXPECT_CALL(mock_encoder_, NumChannels()).WillRepeatedly(Return(1U));
    EXPECT_CALL(mock_encoder_, SampleRateHz())
        .WillRepeatedly(Return(sample_rate_hz_));
    EXPECT_CALL(mock_encoder_, MaxEncodedBytes())
        .WillRepeatedly(Return(kMockMaxEncodedBytes));
    encoded_.resize(red_->MaxEncodedBytes(), 0);
  }

  void TearDown() override {
    red_.reset();
    // Don't expect the red_ object to delete the AudioEncoder object. But it
    // will be deleted with the test fixture. This is why we explicitly delete
    // the red_ object above, and set expectations on mock_encoder_ afterwards.
    EXPECT_CALL(mock_encoder_, Die()).Times(1);
  }

  void Encode() {
    ASSERT_TRUE(red_.get() != NULL);
    encoded_info_ = red_->Encode(
        timestamp_,
        rtc::ArrayView<const int16_t>(audio_, num_audio_samples_10ms),
        encoded_.size(), &encoded_[0]);
    timestamp_ += num_audio_samples_10ms;
  }

  MockAudioEncoder mock_encoder_;
  rtc::scoped_ptr<AudioEncoderCopyRed> red_;
  uint32_t timestamp_;
  int16_t audio_[kMaxNumSamples];
  const int sample_rate_hz_;
  size_t num_audio_samples_10ms;
  std::vector<uint8_t> encoded_;
  AudioEncoder::EncodedInfo encoded_info_;
  const int red_payload_type_;
};

class MockEncodeHelper {
 public:
  MockEncodeHelper() : write_payload_(false), payload_(NULL) {
    memset(&info_, 0, sizeof(info_));
  }

  AudioEncoder::EncodedInfo Encode(uint32_t timestamp,
                                   rtc::ArrayView<const int16_t> audio,
                                   size_t max_encoded_bytes,
                                   uint8_t* encoded) {
    if (write_payload_) {
      RTC_CHECK(encoded);
      RTC_CHECK_LE(info_.encoded_bytes, max_encoded_bytes);
      memcpy(encoded, payload_, info_.encoded_bytes);
    }
    return info_;
  }

  AudioEncoder::EncodedInfo info_;
  bool write_payload_;
  uint8_t* payload_;
};

TEST_F(AudioEncoderCopyRedTest, CreateAndDestroy) {
}

TEST_F(AudioEncoderCopyRedTest, CheckSampleRatePropagation) {
  EXPECT_CALL(mock_encoder_, SampleRateHz()).WillOnce(Return(17));
  EXPECT_EQ(17, red_->SampleRateHz());
}

TEST_F(AudioEncoderCopyRedTest, CheckNumChannelsPropagation) {
  EXPECT_CALL(mock_encoder_, NumChannels()).WillOnce(Return(17U));
  EXPECT_EQ(17U, red_->NumChannels());
}

TEST_F(AudioEncoderCopyRedTest, CheckFrameSizePropagation) {
  EXPECT_CALL(mock_encoder_, Num10MsFramesInNextPacket()).WillOnce(Return(17U));
  EXPECT_EQ(17U, red_->Num10MsFramesInNextPacket());
}

TEST_F(AudioEncoderCopyRedTest, CheckMaxFrameSizePropagation) {
  EXPECT_CALL(mock_encoder_, Max10MsFramesInAPacket()).WillOnce(Return(17U));
  EXPECT_EQ(17U, red_->Max10MsFramesInAPacket());
}

TEST_F(AudioEncoderCopyRedTest, CheckSetBitratePropagation) {
  EXPECT_CALL(mock_encoder_, SetTargetBitrate(4711));
  red_->SetTargetBitrate(4711);
}

TEST_F(AudioEncoderCopyRedTest, CheckProjectedPacketLossRatePropagation) {
  EXPECT_CALL(mock_encoder_, SetProjectedPacketLossRate(0.5));
  red_->SetProjectedPacketLossRate(0.5);
}

// Checks that the an Encode() call is immediately propagated to the speech
// encoder.
TEST_F(AudioEncoderCopyRedTest, CheckImmediateEncode) {
  // Interleaving the EXPECT_CALL sequence with expectations on the MockFunction
  // check ensures that exactly one call to EncodeInternal happens in each
  // Encode call.
  InSequence s;
  MockFunction<void(int check_point_id)> check;
  for (int i = 1; i <= 6; ++i) {
    EXPECT_CALL(mock_encoder_, EncodeInternal(_, _, _, _))
        .WillRepeatedly(Return(AudioEncoder::EncodedInfo()));
    EXPECT_CALL(check, Call(i));
    Encode();
    check.Call(i);
  }
}

// Checks that no output is produced if the underlying codec doesn't emit any
// new data, even if the RED codec is loaded with a secondary encoding.
TEST_F(AudioEncoderCopyRedTest, CheckNoOutput) {
  // Start with one Encode() call that will produce output.
  static const size_t kEncodedSize = 17;
  AudioEncoder::EncodedInfo info;
  info.encoded_bytes = kEncodedSize;
  EXPECT_CALL(mock_encoder_, EncodeInternal(_, _, _, _))
      .WillOnce(Return(info));
  Encode();
  // First call is a special case, since it does not include a secondary
  // payload.
  EXPECT_EQ(1u, encoded_info_.redundant.size());
  EXPECT_EQ(kEncodedSize, encoded_info_.encoded_bytes);

  // Next call to the speech encoder will not produce any output.
  info.encoded_bytes = 0;
  EXPECT_CALL(mock_encoder_, EncodeInternal(_, _, _, _))
      .WillOnce(Return(info));
  Encode();
  EXPECT_EQ(0u, encoded_info_.encoded_bytes);

  // Final call to the speech encoder will produce output.
  info.encoded_bytes = kEncodedSize;
  EXPECT_CALL(mock_encoder_, EncodeInternal(_, _, _, _))
      .WillOnce(Return(info));
  Encode();
  EXPECT_EQ(2 * kEncodedSize, encoded_info_.encoded_bytes);
  ASSERT_EQ(2u, encoded_info_.redundant.size());
}

// Checks that the correct payload sizes are populated into the redundancy
// information.
TEST_F(AudioEncoderCopyRedTest, CheckPayloadSizes) {
  // Let the mock encoder return payload sizes 1, 2, 3, ..., 10 for the sequence
  // of calls.
  static const int kNumPackets = 10;
  InSequence s;
  for (int encode_size = 1; encode_size <= kNumPackets; ++encode_size) {
    AudioEncoder::EncodedInfo info;
    info.encoded_bytes = encode_size;
    EXPECT_CALL(mock_encoder_, EncodeInternal(_, _, _, _))
        .WillOnce(Return(info));
  }

  // First call is a special case, since it does not include a secondary
  // payload.
  Encode();
  EXPECT_EQ(1u, encoded_info_.redundant.size());
  EXPECT_EQ(1u, encoded_info_.encoded_bytes);

  for (size_t i = 2; i <= kNumPackets; ++i) {
    Encode();
    ASSERT_EQ(2u, encoded_info_.redundant.size());
    EXPECT_EQ(i, encoded_info_.redundant[0].encoded_bytes);
    EXPECT_EQ(i - 1, encoded_info_.redundant[1].encoded_bytes);
    EXPECT_EQ(i + i - 1, encoded_info_.encoded_bytes);
  }
}

// Checks that the correct timestamps are returned.
TEST_F(AudioEncoderCopyRedTest, CheckTimestamps) {
  MockEncodeHelper helper;

  helper.info_.encoded_bytes = 17;
  helper.info_.encoded_timestamp = timestamp_;
  uint32_t primary_timestamp = timestamp_;
  EXPECT_CALL(mock_encoder_, EncodeInternal(_, _, _, _))
      .WillRepeatedly(Invoke(&helper, &MockEncodeHelper::Encode));

  // First call is a special case, since it does not include a secondary
  // payload.
  Encode();
  EXPECT_EQ(primary_timestamp, encoded_info_.encoded_timestamp);

  uint32_t secondary_timestamp = primary_timestamp;
  primary_timestamp = timestamp_;
  helper.info_.encoded_timestamp = timestamp_;
  Encode();
  ASSERT_EQ(2u, encoded_info_.redundant.size());
  EXPECT_EQ(primary_timestamp, encoded_info_.redundant[0].encoded_timestamp);
  EXPECT_EQ(secondary_timestamp, encoded_info_.redundant[1].encoded_timestamp);
  EXPECT_EQ(primary_timestamp, encoded_info_.encoded_timestamp);
}

// Checks that the primary and secondary payloads are written correctly.
TEST_F(AudioEncoderCopyRedTest, CheckPayloads) {
  // Let the mock encoder write payloads with increasing values. The first
  // payload will have values 0, 1, 2, ..., kPayloadLenBytes - 1.
  MockEncodeHelper helper;
  static const size_t kPayloadLenBytes = 5;
  helper.info_.encoded_bytes = kPayloadLenBytes;
  helper.write_payload_ = true;
  uint8_t payload[kPayloadLenBytes];
  for (uint8_t i = 0; i < kPayloadLenBytes; ++i) {
    payload[i] = i;
  }
  helper.payload_ = payload;
  EXPECT_CALL(mock_encoder_, EncodeInternal(_, _, _, _))
      .WillRepeatedly(Invoke(&helper, &MockEncodeHelper::Encode));

  // First call is a special case, since it does not include a secondary
  // payload.
  Encode();
  EXPECT_EQ(kPayloadLenBytes, encoded_info_.encoded_bytes);
  for (size_t i = 0; i < kPayloadLenBytes; ++i) {
    EXPECT_EQ(i, encoded_[i]);
  }

  for (int j = 0; j < 5; ++j) {
    // Increment all values of the payload by 10.
    for (size_t i = 0; i < kPayloadLenBytes; ++i)
      helper.payload_[i] += 10;

    Encode();
    ASSERT_EQ(2u, encoded_info_.redundant.size());
    EXPECT_EQ(kPayloadLenBytes, encoded_info_.redundant[0].encoded_bytes);
    EXPECT_EQ(kPayloadLenBytes, encoded_info_.redundant[1].encoded_bytes);
    for (size_t i = 0; i < kPayloadLenBytes; ++i) {
      // Check primary payload.
      EXPECT_EQ((j + 1) * 10 + i, encoded_[i]);
      // Check secondary payload.
      EXPECT_EQ(j * 10 + i, encoded_[i + kPayloadLenBytes]);
    }
  }
}

// Checks correct propagation of payload type.
// Checks that the correct timestamps are returned.
TEST_F(AudioEncoderCopyRedTest, CheckPayloadType) {
  MockEncodeHelper helper;

  helper.info_.encoded_bytes = 17;
  const int primary_payload_type = red_payload_type_ + 1;
  helper.info_.payload_type = primary_payload_type;
  EXPECT_CALL(mock_encoder_, EncodeInternal(_, _, _, _))
      .WillRepeatedly(Invoke(&helper, &MockEncodeHelper::Encode));

  // First call is a special case, since it does not include a secondary
  // payload.
  Encode();
  ASSERT_EQ(1u, encoded_info_.redundant.size());
  EXPECT_EQ(primary_payload_type, encoded_info_.redundant[0].payload_type);
  EXPECT_EQ(red_payload_type_, encoded_info_.payload_type);

  const int secondary_payload_type = red_payload_type_ + 2;
  helper.info_.payload_type = secondary_payload_type;
  Encode();
  ASSERT_EQ(2u, encoded_info_.redundant.size());
  EXPECT_EQ(secondary_payload_type, encoded_info_.redundant[0].payload_type);
  EXPECT_EQ(primary_payload_type, encoded_info_.redundant[1].payload_type);
  EXPECT_EQ(red_payload_type_, encoded_info_.payload_type);
}

#if GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

// This test fixture tests various error conditions that makes the
// AudioEncoderCng die via CHECKs.
class AudioEncoderCopyRedDeathTest : public AudioEncoderCopyRedTest {
 protected:
  AudioEncoderCopyRedDeathTest() : AudioEncoderCopyRedTest() {}
};

TEST_F(AudioEncoderCopyRedDeathTest, WrongFrameSize) {
  num_audio_samples_10ms *= 2;  // 20 ms frame.
  EXPECT_DEATH(Encode(), "");
  num_audio_samples_10ms = 0;  // Zero samples.
  EXPECT_DEATH(Encode(), "");
}

TEST_F(AudioEncoderCopyRedDeathTest, NullSpeechEncoder) {
  AudioEncoderCopyRed* red = NULL;
  AudioEncoderCopyRed::Config config;
  config.speech_encoder = NULL;
  EXPECT_DEATH(red = new AudioEncoderCopyRed(config),
               "Speech encoder not provided.");
  // The delete operation is needed to avoid leak reports from memcheck.
  delete red;
}

#endif  // GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

}  // namespace webrtc
