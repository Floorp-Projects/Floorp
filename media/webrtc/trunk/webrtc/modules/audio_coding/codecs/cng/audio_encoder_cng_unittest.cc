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
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/common_audio/vad/mock/mock_vad.h"
#include "webrtc/modules/audio_coding/codecs/cng/audio_encoder_cng.h"
#include "webrtc/modules/audio_coding/codecs/mock/mock_audio_encoder.h"

using ::testing::Return;
using ::testing::_;
using ::testing::SetArgPointee;
using ::testing::InSequence;
using ::testing::Invoke;

namespace webrtc {

namespace {
static const size_t kMockMaxEncodedBytes = 1000;
static const size_t kMaxNumSamples = 48 * 10 * 2;  // 10 ms @ 48 kHz stereo.
static const size_t kMockReturnEncodedBytes = 17;
static const int kCngPayloadType = 18;
}

class AudioEncoderCngTest : public ::testing::Test {
 protected:
  AudioEncoderCngTest()
      : mock_vad_(new MockVad),
        timestamp_(4711),
        num_audio_samples_10ms_(0),
        sample_rate_hz_(8000) {
    memset(audio_, 0, kMaxNumSamples * 2);
    config_.speech_encoder = &mock_encoder_;
    EXPECT_CALL(mock_encoder_, NumChannels()).WillRepeatedly(Return(1));
    // Let the AudioEncoderCng object use a MockVad instead of its internally
    // created Vad object.
    config_.vad = mock_vad_;
    config_.payload_type = kCngPayloadType;
  }

  void TearDown() override {
    EXPECT_CALL(*mock_vad_, Die()).Times(1);
    cng_.reset();
    // Don't expect the cng_ object to delete the AudioEncoder object. But it
    // will be deleted with the test fixture. This is why we explicitly delete
    // the cng_ object above, and set expectations on mock_encoder_ afterwards.
    EXPECT_CALL(mock_encoder_, Die()).Times(1);
  }

  void CreateCng() {
    // The config_ parameters may be changed by the TEST_Fs up until CreateCng()
    // is called, thus we cannot use the values until now.
    num_audio_samples_10ms_ = static_cast<size_t>(10 * sample_rate_hz_ / 1000);
    ASSERT_LE(num_audio_samples_10ms_, kMaxNumSamples);
    EXPECT_CALL(mock_encoder_, SampleRateHz())
        .WillRepeatedly(Return(sample_rate_hz_));
    // Max10MsFramesInAPacket() is just used to verify that the SID frame period
    // is not too small. The return value does not matter that much, as long as
    // it is smaller than 10.
    EXPECT_CALL(mock_encoder_, Max10MsFramesInAPacket()).WillOnce(Return(1u));
    EXPECT_CALL(mock_encoder_, MaxEncodedBytes())
        .WillRepeatedly(Return(kMockMaxEncodedBytes));
    cng_.reset(new AudioEncoderCng(config_));
    encoded_.resize(cng_->MaxEncodedBytes(), 0);
  }

  void Encode() {
    ASSERT_TRUE(cng_) << "Must call CreateCng() first.";
    encoded_info_ = cng_->Encode(
        timestamp_,
        rtc::ArrayView<const int16_t>(audio_, num_audio_samples_10ms_),
        encoded_.size(), &encoded_[0]);
    timestamp_ += static_cast<uint32_t>(num_audio_samples_10ms_);
  }

  // Expect |num_calls| calls to the encoder, all successful. The last call
  // claims to have encoded |kMockMaxEncodedBytes| bytes, and all the preceding
  // ones 0 bytes.
  void ExpectEncodeCalls(size_t num_calls) {
    InSequence s;
    AudioEncoder::EncodedInfo info;
    for (size_t j = 0; j < num_calls - 1; ++j) {
      EXPECT_CALL(mock_encoder_, EncodeInternal(_, _, _, _))
          .WillOnce(Return(info));
    }
    info.encoded_bytes = kMockReturnEncodedBytes;
    EXPECT_CALL(mock_encoder_, EncodeInternal(_, _, _, _))
        .WillOnce(Return(info));
  }

  // Verifies that the cng_ object waits until it has collected
  // |blocks_per_frame| blocks of audio, and then dispatches all of them to
  // the underlying codec (speech or cng).
  void CheckBlockGrouping(size_t blocks_per_frame, bool active_speech) {
    EXPECT_CALL(mock_encoder_, Num10MsFramesInNextPacket())
        .WillRepeatedly(Return(blocks_per_frame));
    CreateCng();
    EXPECT_CALL(*mock_vad_, VoiceActivity(_, _, _))
        .WillRepeatedly(Return(active_speech ? Vad::kActive : Vad::kPassive));

    // Don't expect any calls to the encoder yet.
    EXPECT_CALL(mock_encoder_, EncodeInternal(_, _, _, _)).Times(0);
    for (size_t i = 0; i < blocks_per_frame - 1; ++i) {
      Encode();
      EXPECT_EQ(0u, encoded_info_.encoded_bytes);
    }
    if (active_speech)
      ExpectEncodeCalls(blocks_per_frame);
    Encode();
    if (active_speech) {
      EXPECT_EQ(kMockReturnEncodedBytes, encoded_info_.encoded_bytes);
    } else {
      EXPECT_EQ(static_cast<size_t>(config_.num_cng_coefficients + 1),
                encoded_info_.encoded_bytes);
    }
  }

  // Verifies that the audio is partitioned into larger blocks before calling
  // the VAD.
  void CheckVadInputSize(int input_frame_size_ms,
                         int expected_first_block_size_ms,
                         int expected_second_block_size_ms) {
    const size_t blocks_per_frame =
        static_cast<size_t>(input_frame_size_ms / 10);

    EXPECT_CALL(mock_encoder_, Num10MsFramesInNextPacket())
        .WillRepeatedly(Return(blocks_per_frame));

    // Expect nothing to happen before the last block is sent to cng_.
    EXPECT_CALL(*mock_vad_, VoiceActivity(_, _, _)).Times(0);
    for (size_t i = 0; i < blocks_per_frame - 1; ++i) {
      Encode();
    }

    // Let the VAD decision be passive, since an active decision may lead to
    // early termination of the decision loop.
    InSequence s;
    EXPECT_CALL(
        *mock_vad_,
        VoiceActivity(_, expected_first_block_size_ms * sample_rate_hz_ / 1000,
                      sample_rate_hz_)).WillOnce(Return(Vad::kPassive));
    if (expected_second_block_size_ms > 0) {
      EXPECT_CALL(*mock_vad_,
                  VoiceActivity(
                      _, expected_second_block_size_ms * sample_rate_hz_ / 1000,
                      sample_rate_hz_)).WillOnce(Return(Vad::kPassive));
    }

    // With this call to Encode(), |mock_vad_| should be called according to the
    // above expectations.
    Encode();
  }

  // Tests a frame with both active and passive speech. Returns true if the
  // decision was active speech, false if it was passive.
  bool CheckMixedActivePassive(Vad::Activity first_type,
                               Vad::Activity second_type) {
    // Set the speech encoder frame size to 60 ms, to ensure that the VAD will
    // be called twice.
    const size_t blocks_per_frame = 6;
    EXPECT_CALL(mock_encoder_, Num10MsFramesInNextPacket())
        .WillRepeatedly(Return(blocks_per_frame));
    InSequence s;
    EXPECT_CALL(*mock_vad_, VoiceActivity(_, _, _))
        .WillOnce(Return(first_type));
    if (first_type == Vad::kPassive) {
      // Expect a second call to the VAD only if the first frame was passive.
      EXPECT_CALL(*mock_vad_, VoiceActivity(_, _, _))
          .WillOnce(Return(second_type));
    }
    encoded_info_.payload_type = 0;
    for (size_t i = 0; i < blocks_per_frame; ++i) {
      Encode();
    }
    return encoded_info_.payload_type != kCngPayloadType;
  }

  AudioEncoderCng::Config config_;
  rtc::scoped_ptr<AudioEncoderCng> cng_;
  MockAudioEncoder mock_encoder_;
  MockVad* mock_vad_;  // Ownership is transferred to |cng_|.
  uint32_t timestamp_;
  int16_t audio_[kMaxNumSamples];
  size_t num_audio_samples_10ms_;
  std::vector<uint8_t> encoded_;
  AudioEncoder::EncodedInfo encoded_info_;
  int sample_rate_hz_;
};

TEST_F(AudioEncoderCngTest, CreateAndDestroy) {
  CreateCng();
}

TEST_F(AudioEncoderCngTest, CheckFrameSizePropagation) {
  CreateCng();
  EXPECT_CALL(mock_encoder_, Num10MsFramesInNextPacket()).WillOnce(Return(17U));
  EXPECT_EQ(17U, cng_->Num10MsFramesInNextPacket());
}

TEST_F(AudioEncoderCngTest, CheckChangeBitratePropagation) {
  CreateCng();
  EXPECT_CALL(mock_encoder_, SetTargetBitrate(4711));
  cng_->SetTargetBitrate(4711);
}

TEST_F(AudioEncoderCngTest, CheckProjectedPacketLossRatePropagation) {
  CreateCng();
  EXPECT_CALL(mock_encoder_, SetProjectedPacketLossRate(0.5));
  cng_->SetProjectedPacketLossRate(0.5);
}

TEST_F(AudioEncoderCngTest, EncodeCallsVad) {
  EXPECT_CALL(mock_encoder_, Num10MsFramesInNextPacket())
      .WillRepeatedly(Return(1U));
  CreateCng();
  EXPECT_CALL(*mock_vad_, VoiceActivity(_, _, _))
      .WillOnce(Return(Vad::kPassive));
  Encode();
}

TEST_F(AudioEncoderCngTest, EncodeCollects1BlockPassiveSpeech) {
  CheckBlockGrouping(1, false);
}

TEST_F(AudioEncoderCngTest, EncodeCollects2BlocksPassiveSpeech) {
  CheckBlockGrouping(2, false);
}

TEST_F(AudioEncoderCngTest, EncodeCollects3BlocksPassiveSpeech) {
  CheckBlockGrouping(3, false);
}

TEST_F(AudioEncoderCngTest, EncodeCollects1BlockActiveSpeech) {
  CheckBlockGrouping(1, true);
}

TEST_F(AudioEncoderCngTest, EncodeCollects2BlocksActiveSpeech) {
  CheckBlockGrouping(2, true);
}

TEST_F(AudioEncoderCngTest, EncodeCollects3BlocksActiveSpeech) {
  CheckBlockGrouping(3, true);
}

TEST_F(AudioEncoderCngTest, EncodePassive) {
  const size_t kBlocksPerFrame = 3;
  EXPECT_CALL(mock_encoder_, Num10MsFramesInNextPacket())
      .WillRepeatedly(Return(kBlocksPerFrame));
  CreateCng();
  EXPECT_CALL(*mock_vad_, VoiceActivity(_, _, _))
      .WillRepeatedly(Return(Vad::kPassive));
  // Expect no calls at all to the speech encoder mock.
  EXPECT_CALL(mock_encoder_, EncodeInternal(_, _, _, _)).Times(0);
  uint32_t expected_timestamp = timestamp_;
  for (size_t i = 0; i < 100; ++i) {
    Encode();
    // Check if it was time to call the cng encoder. This is done once every
    // |kBlocksPerFrame| calls.
    if ((i + 1) % kBlocksPerFrame == 0) {
      // Now check if a SID interval has elapsed.
      if ((i % (config_.sid_frame_interval_ms / 10)) < kBlocksPerFrame) {
        // If so, verify that we got a CNG encoding.
        EXPECT_EQ(kCngPayloadType, encoded_info_.payload_type);
        EXPECT_FALSE(encoded_info_.speech);
        EXPECT_EQ(static_cast<size_t>(config_.num_cng_coefficients) + 1,
                  encoded_info_.encoded_bytes);
        EXPECT_EQ(expected_timestamp, encoded_info_.encoded_timestamp);
      }
      expected_timestamp += kBlocksPerFrame * num_audio_samples_10ms_;
    } else {
      // Otherwise, expect no output.
      EXPECT_EQ(0u, encoded_info_.encoded_bytes);
    }
  }
}

// Verifies that the correct action is taken for frames with both active and
// passive speech.
TEST_F(AudioEncoderCngTest, MixedActivePassive) {
  CreateCng();

  // All of the frame is active speech.
  ExpectEncodeCalls(6);
  EXPECT_TRUE(CheckMixedActivePassive(Vad::kActive, Vad::kActive));
  EXPECT_TRUE(encoded_info_.speech);

  // First half of the frame is active speech.
  ExpectEncodeCalls(6);
  EXPECT_TRUE(CheckMixedActivePassive(Vad::kActive, Vad::kPassive));
  EXPECT_TRUE(encoded_info_.speech);

  // Second half of the frame is active speech.
  ExpectEncodeCalls(6);
  EXPECT_TRUE(CheckMixedActivePassive(Vad::kPassive, Vad::kActive));
  EXPECT_TRUE(encoded_info_.speech);

  // All of the frame is passive speech. Expect no calls to |mock_encoder_|.
  EXPECT_FALSE(CheckMixedActivePassive(Vad::kPassive, Vad::kPassive));
  EXPECT_FALSE(encoded_info_.speech);
}

// These tests verify that the audio is partitioned into larger blocks before
// calling the VAD.
// The parameters for CheckVadInputSize are:
// CheckVadInputSize(frame_size, expected_first_block_size,
//                   expected_second_block_size);
TEST_F(AudioEncoderCngTest, VadInputSize10Ms) {
  CreateCng();
  CheckVadInputSize(10, 10, 0);
}
TEST_F(AudioEncoderCngTest, VadInputSize20Ms) {
  CreateCng();
  CheckVadInputSize(20, 20, 0);
}
TEST_F(AudioEncoderCngTest, VadInputSize30Ms) {
  CreateCng();
  CheckVadInputSize(30, 30, 0);
}
TEST_F(AudioEncoderCngTest, VadInputSize40Ms) {
  CreateCng();
  CheckVadInputSize(40, 20, 20);
}
TEST_F(AudioEncoderCngTest, VadInputSize50Ms) {
  CreateCng();
  CheckVadInputSize(50, 30, 20);
}
TEST_F(AudioEncoderCngTest, VadInputSize60Ms) {
  CreateCng();
  CheckVadInputSize(60, 30, 30);
}

// Verifies that the correct payload type is set when CNG is encoded.
TEST_F(AudioEncoderCngTest, VerifyCngPayloadType) {
  CreateCng();
  EXPECT_CALL(mock_encoder_, EncodeInternal(_, _, _, _)).Times(0);
  EXPECT_CALL(mock_encoder_, Num10MsFramesInNextPacket()).WillOnce(Return(1U));
  EXPECT_CALL(*mock_vad_, VoiceActivity(_, _, _))
      .WillOnce(Return(Vad::kPassive));
  encoded_info_.payload_type = 0;
  Encode();
  EXPECT_EQ(kCngPayloadType, encoded_info_.payload_type);
}

// Verifies that a SID frame is encoded immediately as the signal changes from
// active speech to passive.
TEST_F(AudioEncoderCngTest, VerifySidFrameAfterSpeech) {
  CreateCng();
  EXPECT_CALL(mock_encoder_, Num10MsFramesInNextPacket())
      .WillRepeatedly(Return(1U));
  // Start with encoding noise.
  EXPECT_CALL(*mock_vad_, VoiceActivity(_, _, _))
      .Times(2)
      .WillRepeatedly(Return(Vad::kPassive));
  Encode();
  EXPECT_EQ(kCngPayloadType, encoded_info_.payload_type);
  EXPECT_EQ(static_cast<size_t>(config_.num_cng_coefficients) + 1,
            encoded_info_.encoded_bytes);
  // Encode again, and make sure we got no frame at all (since the SID frame
  // period is 100 ms by default).
  Encode();
  EXPECT_EQ(0u, encoded_info_.encoded_bytes);

  // Now encode active speech.
  encoded_info_.payload_type = 0;
  EXPECT_CALL(*mock_vad_, VoiceActivity(_, _, _))
      .WillOnce(Return(Vad::kActive));
  AudioEncoder::EncodedInfo info;
  info.encoded_bytes = kMockReturnEncodedBytes;
  EXPECT_CALL(mock_encoder_, EncodeInternal(_, _, _, _)).WillOnce(Return(info));
  Encode();
  EXPECT_EQ(kMockReturnEncodedBytes, encoded_info_.encoded_bytes);

  // Go back to noise again, and verify that a SID frame is emitted.
  EXPECT_CALL(*mock_vad_, VoiceActivity(_, _, _))
      .WillOnce(Return(Vad::kPassive));
  Encode();
  EXPECT_EQ(kCngPayloadType, encoded_info_.payload_type);
  EXPECT_EQ(static_cast<size_t>(config_.num_cng_coefficients) + 1,
            encoded_info_.encoded_bytes);
}

// Resetting the CNG should reset both the VAD and the encoder.
TEST_F(AudioEncoderCngTest, Reset) {
  CreateCng();
  EXPECT_CALL(mock_encoder_, Reset()).Times(1);
  EXPECT_CALL(*mock_vad_, Reset()).Times(1);
  cng_->Reset();
}

#if GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

// This test fixture tests various error conditions that makes the
// AudioEncoderCng die via CHECKs.
class AudioEncoderCngDeathTest : public AudioEncoderCngTest {
 protected:
  AudioEncoderCngDeathTest() : AudioEncoderCngTest() {
    // Don't provide a Vad mock object, since it will leak when the test dies.
    config_.vad = NULL;
    EXPECT_CALL(*mock_vad_, Die()).Times(1);
    delete mock_vad_;
    mock_vad_ = NULL;
  }

  // Override AudioEncoderCngTest::TearDown, since that one expects a call to
  // the destructor of |mock_vad_|. In this case, that object is already
  // deleted.
  void TearDown() override {
    cng_.reset();
    // Don't expect the cng_ object to delete the AudioEncoder object. But it
    // will be deleted with the test fixture. This is why we explicitly delete
    // the cng_ object above, and set expectations on mock_encoder_ afterwards.
    EXPECT_CALL(mock_encoder_, Die()).Times(1);
  }
};

TEST_F(AudioEncoderCngDeathTest, WrongFrameSize) {
  CreateCng();
  num_audio_samples_10ms_ *= 2;  // 20 ms frame.
  EXPECT_DEATH(Encode(), "");
  num_audio_samples_10ms_ = 0;  // Zero samples.
  EXPECT_DEATH(Encode(), "");
}

TEST_F(AudioEncoderCngDeathTest, WrongNumCoefficients) {
  config_.num_cng_coefficients = -1;
  EXPECT_DEATH(CreateCng(), "Invalid configuration");
  config_.num_cng_coefficients = 0;
  EXPECT_DEATH(CreateCng(), "Invalid configuration");
  config_.num_cng_coefficients = 13;
  EXPECT_DEATH(CreateCng(), "Invalid configuration");
}

TEST_F(AudioEncoderCngDeathTest, NullSpeechEncoder) {
  config_.speech_encoder = NULL;
  EXPECT_DEATH(CreateCng(), "Invalid configuration");
}

TEST_F(AudioEncoderCngDeathTest, Stereo) {
  EXPECT_CALL(mock_encoder_, NumChannels()).WillRepeatedly(Return(2));
  EXPECT_DEATH(CreateCng(), "Invalid configuration");
  config_.num_channels = 2;
  EXPECT_DEATH(CreateCng(), "Invalid configuration");
}

TEST_F(AudioEncoderCngDeathTest, EncoderFrameSizeTooLarge) {
  CreateCng();
  EXPECT_CALL(mock_encoder_, Num10MsFramesInNextPacket())
      .WillRepeatedly(Return(7U));
  for (int i = 0; i < 6; ++i)
    Encode();
  EXPECT_DEATH(Encode(),
               "Frame size cannot be larger than 60 ms when using VAD/CNG.");
}

#endif  // GTEST_HAS_DEATH_TEST

}  // namespace webrtc
