/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/audio_coding/codecs/opus/interface/opus_interface.h"
#include "webrtc/modules/audio_coding/codecs/opus/opus_inst.h"
#include "webrtc/modules/audio_coding/neteq/tools/audio_loop.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace webrtc {

using test::AudioLoop;
using ::testing::TestWithParam;
using ::testing::Values;
using ::testing::Combine;

// Maximum number of bytes in output bitstream.
const size_t kMaxBytes = 1000;
// Sample rate of Opus.
const int kOpusRateKhz = 48;
// Number of samples-per-channel in a 20 ms frame, sampled at 48 kHz.
const int kOpus20msFrameSamples = kOpusRateKhz * 20;
// Number of samples-per-channel in a 10 ms frame, sampled at 48 kHz.
const int kOpus10msFrameSamples = kOpusRateKhz * 10;

class OpusTest : public TestWithParam<::testing::tuple<int, int>> {
 protected:
  OpusTest();

  void TestDtxEffect(bool dtx);

  // Prepare |speech_data_| for encoding, read from a hard-coded file.
  // After preparation, |speech_data_.GetNextBlock()| returns a pointer to a
  // block of |block_length_ms| milliseconds. The data is looped every
  // |loop_length_ms| milliseconds.
  void PrepareSpeechData(int channel, int block_length_ms, int loop_length_ms);

  int EncodeDecode(WebRtcOpusEncInst* encoder,
                   const int16_t* input_audio,
                   const int input_samples,
                   WebRtcOpusDecInst* decoder,
                   int16_t* output_audio,
                   int16_t* audio_type);

  void SetMaxPlaybackRate(WebRtcOpusEncInst* encoder,
                          opus_int32 expect, int32_t set);

  WebRtcOpusEncInst* opus_encoder_;
  WebRtcOpusDecInst* opus_decoder_;

  AudioLoop speech_data_;
  uint8_t bitstream_[kMaxBytes];
  int encoded_bytes_;
  int channels_;
  int application_;
};

OpusTest::OpusTest()
    : opus_encoder_(NULL),
      opus_decoder_(NULL),
      encoded_bytes_(0),
      channels_(::testing::get<0>(GetParam())),
      application_(::testing::get<1>(GetParam())) {
}

void OpusTest::PrepareSpeechData(int channel, int block_length_ms,
                                 int loop_length_ms) {
  const std::string file_name =
        webrtc::test::ResourcePath((channel == 1) ?
            "audio_coding/testfile32kHz" :
            "audio_coding/teststereo32kHz", "pcm");
  if (loop_length_ms < block_length_ms) {
    loop_length_ms = block_length_ms;
  }
  EXPECT_TRUE(speech_data_.Init(file_name,
                                loop_length_ms * kOpusRateKhz * channel,
                                block_length_ms * kOpusRateKhz * channel));
}

void OpusTest::SetMaxPlaybackRate(WebRtcOpusEncInst* encoder,
                                  opus_int32 expect,
                                  int32_t set) {
  opus_int32 bandwidth;
  EXPECT_EQ(0, WebRtcOpus_SetMaxPlaybackRate(opus_encoder_, set));
  opus_encoder_ctl(opus_encoder_->encoder,
                   OPUS_GET_MAX_BANDWIDTH(&bandwidth));
  EXPECT_EQ(expect, bandwidth);
}

int OpusTest::EncodeDecode(WebRtcOpusEncInst* encoder,
                           const int16_t* input_audio,
                           const int input_samples,
                           WebRtcOpusDecInst* decoder,
                           int16_t* output_audio,
                           int16_t* audio_type) {
  encoded_bytes_ = WebRtcOpus_Encode(encoder,
                                    input_audio,
                                    input_samples, kMaxBytes,
                                    bitstream_);
  return WebRtcOpus_Decode(decoder, bitstream_,
                           encoded_bytes_, output_audio,
                           audio_type);
}

// Test if encoder/decoder can enter DTX mode properly and do not enter DTX when
// they should not. This test is signal dependent.
void OpusTest::TestDtxEffect(bool dtx) {
  PrepareSpeechData(channels_, 20, 2000);

  // Create encoder memory.
  EXPECT_EQ(0, WebRtcOpus_EncoderCreate(&opus_encoder_,
                                        channels_,
                                        application_));
  EXPECT_EQ(0, WebRtcOpus_DecoderCreate(&opus_decoder_, channels_));

  // Set bitrate.
  EXPECT_EQ(0, WebRtcOpus_SetBitRate(opus_encoder_,
                                     channels_ == 1 ? 32000 : 64000));

  // Set input audio as silence.
  int16_t* silence = new int16_t[kOpus20msFrameSamples * channels_];
  memset(silence, 0, sizeof(int16_t) * kOpus20msFrameSamples * channels_);

  // Setting DTX.
  EXPECT_EQ(0, dtx ? WebRtcOpus_EnableDtx(opus_encoder_) :
      WebRtcOpus_DisableDtx(opus_encoder_));

  int16_t audio_type;
  int16_t* output_data_decode = new int16_t[kOpus20msFrameSamples * channels_];

  for (int i = 0; i < 100; ++i) {
    EXPECT_EQ(kOpus20msFrameSamples,
              EncodeDecode(opus_encoder_, speech_data_.GetNextBlock(),
                           kOpus20msFrameSamples, opus_decoder_,
                           output_data_decode, &audio_type));
    // If not DTX, it should never enter DTX mode. If DTX, we do not care since
    // whether it enters DTX depends on the signal type.
    if (!dtx) {
      EXPECT_GT(encoded_bytes_, 1);
      EXPECT_EQ(0, opus_encoder_->in_dtx_mode);
      EXPECT_EQ(0, opus_decoder_->in_dtx_mode);
      EXPECT_EQ(0, audio_type);  // Speech.
    }
  }

  // We input some silent segments. In DTX mode, the encoder will stop sending.
  // However, DTX may happen after a while.
  for (int i = 0; i < 30; ++i) {
    EXPECT_EQ(kOpus20msFrameSamples,
              EncodeDecode(opus_encoder_, silence,
                           kOpus20msFrameSamples, opus_decoder_,
                           output_data_decode, &audio_type));
    if (!dtx) {
      EXPECT_GT(encoded_bytes_, 1);
      EXPECT_EQ(0, opus_encoder_->in_dtx_mode);
      EXPECT_EQ(0, opus_decoder_->in_dtx_mode);
      EXPECT_EQ(0, audio_type);  // Speech.
    } else if (1 == encoded_bytes_) {
      EXPECT_EQ(1, opus_encoder_->in_dtx_mode);
      EXPECT_EQ(1, opus_decoder_->in_dtx_mode);
      EXPECT_EQ(2, audio_type);  // Comfort noise.
      break;
    }
  }

  // DTX mode is maintained 400 ms.
  for (int i = 0; i < 19; ++i) {
    EXPECT_EQ(kOpus20msFrameSamples,
              EncodeDecode(opus_encoder_, silence,
                           kOpus20msFrameSamples, opus_decoder_,
                           output_data_decode, &audio_type));
    if (dtx) {
      EXPECT_EQ(0, encoded_bytes_)  // Send 0 byte.
          << "Opus should have entered DTX mode.";
      EXPECT_EQ(1, opus_encoder_->in_dtx_mode);
      EXPECT_EQ(1, opus_decoder_->in_dtx_mode);
      EXPECT_EQ(2, audio_type);  // Comfort noise.
    } else {
      EXPECT_GT(encoded_bytes_, 1);
      EXPECT_EQ(0, opus_encoder_->in_dtx_mode);
      EXPECT_EQ(0, opus_decoder_->in_dtx_mode);
      EXPECT_EQ(0, audio_type);  // Speech.
    }
  }

  // Quit DTX after 400 ms
  EXPECT_EQ(kOpus20msFrameSamples,
            EncodeDecode(opus_encoder_, silence,
                         kOpus20msFrameSamples, opus_decoder_,
                         output_data_decode, &audio_type));

  EXPECT_GT(encoded_bytes_, 1);
  EXPECT_EQ(0, opus_encoder_->in_dtx_mode);
  EXPECT_EQ(0, opus_decoder_->in_dtx_mode);
  EXPECT_EQ(0, audio_type);  // Speech.

  // Enters DTX again immediately.
  EXPECT_EQ(kOpus20msFrameSamples,
            EncodeDecode(opus_encoder_, silence,
                         kOpus20msFrameSamples, opus_decoder_,
                         output_data_decode, &audio_type));
  if (dtx) {
    EXPECT_EQ(1, encoded_bytes_);  // Send 1 byte.
    EXPECT_EQ(1, opus_encoder_->in_dtx_mode);
    EXPECT_EQ(1, opus_decoder_->in_dtx_mode);
    EXPECT_EQ(2, audio_type);  // Comfort noise.
  } else {
    EXPECT_GT(encoded_bytes_, 1);
    EXPECT_EQ(0, opus_encoder_->in_dtx_mode);
    EXPECT_EQ(0, opus_decoder_->in_dtx_mode);
    EXPECT_EQ(0, audio_type);  // Speech.
  }

  silence[0] = 10000;
  if (dtx) {
    // Verify that encoder/decoder can jump out from DTX mode.
    EXPECT_EQ(kOpus20msFrameSamples,
              EncodeDecode(opus_encoder_, silence,
                           kOpus20msFrameSamples, opus_decoder_,
                           output_data_decode, &audio_type));
    EXPECT_GT(encoded_bytes_, 1);
    EXPECT_EQ(0, opus_encoder_->in_dtx_mode);
    EXPECT_EQ(0, opus_decoder_->in_dtx_mode);
    EXPECT_EQ(0, audio_type);  // Speech.
  }

  // Free memory.
  delete[] output_data_decode;
  delete[] silence;
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_encoder_));
  EXPECT_EQ(0, WebRtcOpus_DecoderFree(opus_decoder_));
}

// Test failing Create.
TEST(OpusTest, OpusCreateFail) {
  WebRtcOpusEncInst* opus_encoder;
  WebRtcOpusDecInst* opus_decoder;

  // Test to see that an invalid pointer is caught.
  EXPECT_EQ(-1, WebRtcOpus_EncoderCreate(NULL, 1, 0));
  // Invalid channel number.
  EXPECT_EQ(-1, WebRtcOpus_EncoderCreate(&opus_encoder, 3, 0));
  // Invalid applciation mode.
  EXPECT_EQ(-1, WebRtcOpus_EncoderCreate(&opus_encoder, 1, 2));

  EXPECT_EQ(-1, WebRtcOpus_DecoderCreate(NULL, 1));
  // Invalid channel number.
  EXPECT_EQ(-1, WebRtcOpus_DecoderCreate(&opus_decoder, 3));
}

// Test failing Free.
TEST(OpusTest, OpusFreeFail) {
  // Test to see that an invalid pointer is caught.
  EXPECT_EQ(-1, WebRtcOpus_EncoderFree(NULL));
  EXPECT_EQ(-1, WebRtcOpus_DecoderFree(NULL));
}

// Test normal Create and Free.
TEST_P(OpusTest, OpusCreateFree) {
  EXPECT_EQ(0, WebRtcOpus_EncoderCreate(&opus_encoder_,
                                        channels_,
                                        application_));
  EXPECT_EQ(0, WebRtcOpus_DecoderCreate(&opus_decoder_, channels_));
  EXPECT_TRUE(opus_encoder_ != NULL);
  EXPECT_TRUE(opus_decoder_ != NULL);
  // Free encoder and decoder memory.
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_encoder_));
  EXPECT_EQ(0, WebRtcOpus_DecoderFree(opus_decoder_));
}

TEST_P(OpusTest, OpusEncodeDecode) {
  PrepareSpeechData(channels_, 20, 20);

  // Create encoder memory.
  EXPECT_EQ(0, WebRtcOpus_EncoderCreate(&opus_encoder_,
                                        channels_,
                                        application_));
  EXPECT_EQ(0, WebRtcOpus_DecoderCreate(&opus_decoder_,
                                        channels_));

  // Set bitrate.
  EXPECT_EQ(0, WebRtcOpus_SetBitRate(opus_encoder_,
                                     channels_ == 1 ? 32000 : 64000));

  // Check number of channels for decoder.
  EXPECT_EQ(channels_, WebRtcOpus_DecoderChannels(opus_decoder_));

  // Check application mode.
  opus_int32 app;
  opus_encoder_ctl(opus_encoder_->encoder,
                   OPUS_GET_APPLICATION(&app));
  EXPECT_EQ(application_ == 0 ? OPUS_APPLICATION_VOIP : OPUS_APPLICATION_AUDIO,
            app);

  // Encode & decode.
  int16_t audio_type;
  int16_t* output_data_decode = new int16_t[kOpus20msFrameSamples * channels_];
  EXPECT_EQ(kOpus20msFrameSamples,
            EncodeDecode(opus_encoder_, speech_data_.GetNextBlock(),
                         kOpus20msFrameSamples, opus_decoder_,
                         output_data_decode, &audio_type));

  // Free memory.
  delete[] output_data_decode;
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_encoder_));
  EXPECT_EQ(0, WebRtcOpus_DecoderFree(opus_decoder_));
}

TEST_P(OpusTest, OpusSetBitRate) {
  // Test without creating encoder memory.
  EXPECT_EQ(-1, WebRtcOpus_SetBitRate(opus_encoder_, 60000));

  // Create encoder memory, try with different bitrates.
  EXPECT_EQ(0, WebRtcOpus_EncoderCreate(&opus_encoder_,
                                        channels_,
                                        application_));
  EXPECT_EQ(0, WebRtcOpus_SetBitRate(opus_encoder_, 30000));
  EXPECT_EQ(0, WebRtcOpus_SetBitRate(opus_encoder_, 60000));
  EXPECT_EQ(0, WebRtcOpus_SetBitRate(opus_encoder_, 300000));
  EXPECT_EQ(0, WebRtcOpus_SetBitRate(opus_encoder_, 600000));

  // Free memory.
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_encoder_));
}

TEST_P(OpusTest, OpusSetComplexity) {
  // Test without creating encoder memory.
  EXPECT_EQ(-1, WebRtcOpus_SetComplexity(opus_encoder_, 9));

  // Create encoder memory, try with different complexities.
  EXPECT_EQ(0, WebRtcOpus_EncoderCreate(&opus_encoder_,
                                        channels_,
                                        application_));

  EXPECT_EQ(0, WebRtcOpus_SetComplexity(opus_encoder_, 0));
  EXPECT_EQ(0, WebRtcOpus_SetComplexity(opus_encoder_, 10));
  EXPECT_EQ(-1, WebRtcOpus_SetComplexity(opus_encoder_, 11));

  // Free memory.
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_encoder_));
}

// Encode and decode one frame, initialize the decoder and
// decode once more.
TEST_P(OpusTest, OpusDecodeInit) {
  PrepareSpeechData(channels_, 20, 20);

  // Create encoder memory.
  EXPECT_EQ(0, WebRtcOpus_EncoderCreate(&opus_encoder_,
                                        channels_,
                                        application_));
  EXPECT_EQ(0, WebRtcOpus_DecoderCreate(&opus_decoder_, channels_));

  // Encode & decode.
  int16_t audio_type;
  int16_t* output_data_decode = new int16_t[kOpus20msFrameSamples * channels_];
  EXPECT_EQ(kOpus20msFrameSamples,
            EncodeDecode(opus_encoder_, speech_data_.GetNextBlock(),
                         kOpus20msFrameSamples, opus_decoder_,
                         output_data_decode, &audio_type));

  EXPECT_EQ(0, WebRtcOpus_DecoderInit(opus_decoder_));

  EXPECT_EQ(kOpus20msFrameSamples,
            WebRtcOpus_Decode(opus_decoder_, bitstream_,
                              encoded_bytes_, output_data_decode,
                              &audio_type));

  // Free memory.
  delete[] output_data_decode;
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_encoder_));
  EXPECT_EQ(0, WebRtcOpus_DecoderFree(opus_decoder_));
}

TEST_P(OpusTest, OpusEnableDisableFec) {
  // Test without creating encoder memory.
  EXPECT_EQ(-1, WebRtcOpus_EnableFec(opus_encoder_));
  EXPECT_EQ(-1, WebRtcOpus_DisableFec(opus_encoder_));

  // Create encoder memory.
  EXPECT_EQ(0, WebRtcOpus_EncoderCreate(&opus_encoder_,
                                        channels_,
                                        application_));

  EXPECT_EQ(0, WebRtcOpus_EnableFec(opus_encoder_));
  EXPECT_EQ(0, WebRtcOpus_DisableFec(opus_encoder_));

  // Free memory.
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_encoder_));
}

TEST_P(OpusTest, OpusEnableDisableDtx) {
  // Test without creating encoder memory.
  EXPECT_EQ(-1, WebRtcOpus_EnableDtx(opus_encoder_));
  EXPECT_EQ(-1, WebRtcOpus_DisableDtx(opus_encoder_));

  // Create encoder memory.
  EXPECT_EQ(0, WebRtcOpus_EncoderCreate(&opus_encoder_,
                                        channels_,
                                        application_));

  opus_int32 dtx;

  // DTX is off by default.
  opus_encoder_ctl(opus_encoder_->encoder,
                   OPUS_GET_DTX(&dtx));
  EXPECT_EQ(0, dtx);

  // Test to enable DTX.
  EXPECT_EQ(0, WebRtcOpus_EnableDtx(opus_encoder_));
  opus_encoder_ctl(opus_encoder_->encoder,
                   OPUS_GET_DTX(&dtx));
  EXPECT_EQ(1, dtx);

  // Test to disable DTX.
  EXPECT_EQ(0, WebRtcOpus_DisableDtx(opus_encoder_));
  opus_encoder_ctl(opus_encoder_->encoder,
                   OPUS_GET_DTX(&dtx));
  EXPECT_EQ(0, dtx);


  // Free memory.
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_encoder_));
}

TEST_P(OpusTest, OpusDtxOff) {
  TestDtxEffect(false);
}

TEST_P(OpusTest, OpusDtxOn) {
  if (application_ == 1) {
    // We do not check DTX under OPUS_APPLICATION_AUDIO mode.
    return;
  }
  TestDtxEffect(true);
}

TEST_P(OpusTest, OpusSetPacketLossRate) {
  // Test without creating encoder memory.
  EXPECT_EQ(-1, WebRtcOpus_SetPacketLossRate(opus_encoder_, 50));

  // Create encoder memory.
  EXPECT_EQ(0, WebRtcOpus_EncoderCreate(&opus_encoder_,
                                        channels_,
                                        application_));

  EXPECT_EQ(0, WebRtcOpus_SetPacketLossRate(opus_encoder_, 50));
  EXPECT_EQ(-1, WebRtcOpus_SetPacketLossRate(opus_encoder_, -1));
  EXPECT_EQ(-1, WebRtcOpus_SetPacketLossRate(opus_encoder_, 101));

  // Free memory.
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_encoder_));
}

TEST_P(OpusTest, OpusSetMaxPlaybackRate) {
  // Test without creating encoder memory.
  EXPECT_EQ(-1, WebRtcOpus_SetMaxPlaybackRate(opus_encoder_, 20000));

  // Create encoder memory.
  EXPECT_EQ(0, WebRtcOpus_EncoderCreate(&opus_encoder_,
                                        channels_,
                                        application_));

  SetMaxPlaybackRate(opus_encoder_, OPUS_BANDWIDTH_FULLBAND, 48000);
  SetMaxPlaybackRate(opus_encoder_, OPUS_BANDWIDTH_FULLBAND, 24001);
  SetMaxPlaybackRate(opus_encoder_, OPUS_BANDWIDTH_SUPERWIDEBAND, 24000);
  SetMaxPlaybackRate(opus_encoder_, OPUS_BANDWIDTH_SUPERWIDEBAND, 16001);
  SetMaxPlaybackRate(opus_encoder_, OPUS_BANDWIDTH_WIDEBAND, 16000);
  SetMaxPlaybackRate(opus_encoder_, OPUS_BANDWIDTH_WIDEBAND, 12001);
  SetMaxPlaybackRate(opus_encoder_, OPUS_BANDWIDTH_MEDIUMBAND, 12000);
  SetMaxPlaybackRate(opus_encoder_, OPUS_BANDWIDTH_MEDIUMBAND, 8001);
  SetMaxPlaybackRate(opus_encoder_, OPUS_BANDWIDTH_NARROWBAND, 8000);
  SetMaxPlaybackRate(opus_encoder_, OPUS_BANDWIDTH_NARROWBAND, 4000);

  // Free memory.
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_encoder_));
}

// Test PLC.
TEST_P(OpusTest, OpusDecodePlc) {
  PrepareSpeechData(channels_, 20, 20);

  // Create encoder memory.
  EXPECT_EQ(0, WebRtcOpus_EncoderCreate(&opus_encoder_,
                                        channels_,
                                        application_));
  EXPECT_EQ(0, WebRtcOpus_DecoderCreate(&opus_decoder_, channels_));

  // Set bitrate.
  EXPECT_EQ(0, WebRtcOpus_SetBitRate(opus_encoder_,
                                     channels_== 1 ? 32000 : 64000));

  // Check number of channels for decoder.
  EXPECT_EQ(channels_, WebRtcOpus_DecoderChannels(opus_decoder_));

  // Encode & decode.
  int16_t audio_type;
  int16_t* output_data_decode = new int16_t[kOpus20msFrameSamples * channels_];
  EXPECT_EQ(kOpus20msFrameSamples,
            EncodeDecode(opus_encoder_, speech_data_.GetNextBlock(),
                         kOpus20msFrameSamples, opus_decoder_,
                         output_data_decode, &audio_type));

  // Call decoder PLC.
  int16_t* plc_buffer = new int16_t[kOpus20msFrameSamples * channels_];
  EXPECT_EQ(kOpus20msFrameSamples,
            WebRtcOpus_DecodePlc(opus_decoder_, plc_buffer, 1));

  // Free memory.
  delete[] plc_buffer;
  delete[] output_data_decode;
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_encoder_));
  EXPECT_EQ(0, WebRtcOpus_DecoderFree(opus_decoder_));
}

// Duration estimation.
TEST_P(OpusTest, OpusDurationEstimation) {
  PrepareSpeechData(channels_, 20, 20);

  // Create.
  EXPECT_EQ(0, WebRtcOpus_EncoderCreate(&opus_encoder_,
                                        channels_,
                                        application_));
  EXPECT_EQ(0, WebRtcOpus_DecoderCreate(&opus_decoder_, channels_));

  // 10 ms. We use only first 10 ms of a 20 ms block.
  encoded_bytes_ = WebRtcOpus_Encode(opus_encoder_,
                                     speech_data_.GetNextBlock(),
                                     kOpus10msFrameSamples, kMaxBytes,
                                     bitstream_);
  EXPECT_EQ(kOpus10msFrameSamples,
            WebRtcOpus_DurationEst(opus_decoder_, bitstream_,
                                   encoded_bytes_));

  // 20 ms
  encoded_bytes_ = WebRtcOpus_Encode(opus_encoder_,
                                     speech_data_.GetNextBlock(),
                                     kOpus20msFrameSamples, kMaxBytes,
                                     bitstream_);
  EXPECT_EQ(kOpus20msFrameSamples,
            WebRtcOpus_DurationEst(opus_decoder_, bitstream_,
                                   encoded_bytes_));

  // Free memory.
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_encoder_));
  EXPECT_EQ(0, WebRtcOpus_DecoderFree(opus_decoder_));
}

TEST_P(OpusTest, OpusDecodeRepacketized) {
  const int kPackets = 6;

  PrepareSpeechData(channels_, 20, 20 * kPackets);

  // Create encoder memory.
  ASSERT_EQ(0, WebRtcOpus_EncoderCreate(&opus_encoder_,
                                        channels_,
                                        application_));
  ASSERT_EQ(0, WebRtcOpus_DecoderCreate(&opus_decoder_,
                                        channels_));

  // Set bitrate.
  EXPECT_EQ(0, WebRtcOpus_SetBitRate(opus_encoder_,
                                     channels_ == 1 ? 32000 : 64000));

  // Check number of channels for decoder.
  EXPECT_EQ(channels_, WebRtcOpus_DecoderChannels(opus_decoder_));

  // Encode & decode.
  int16_t audio_type;
  rtc::scoped_ptr<int16_t[]> output_data_decode(
      new int16_t[kPackets * kOpus20msFrameSamples * channels_]);
  OpusRepacketizer* rp = opus_repacketizer_create();

  for (int idx = 0; idx < kPackets; idx++) {
    encoded_bytes_ = WebRtcOpus_Encode(opus_encoder_,
                                       speech_data_.GetNextBlock(),
                                       kOpus20msFrameSamples, kMaxBytes,
                                       bitstream_);
    EXPECT_EQ(OPUS_OK, opus_repacketizer_cat(rp, bitstream_, encoded_bytes_));
  }

  encoded_bytes_ = opus_repacketizer_out(rp, bitstream_, kMaxBytes);

  EXPECT_EQ(kOpus20msFrameSamples * kPackets,
            WebRtcOpus_DurationEst(opus_decoder_, bitstream_, encoded_bytes_));

  EXPECT_EQ(kOpus20msFrameSamples * kPackets,
            WebRtcOpus_Decode(opus_decoder_, bitstream_, encoded_bytes_,
                              output_data_decode.get(), &audio_type));

  // Free memory.
  opus_repacketizer_destroy(rp);
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_encoder_));
  EXPECT_EQ(0, WebRtcOpus_DecoderFree(opus_decoder_));
}

INSTANTIATE_TEST_CASE_P(VariousMode,
                        OpusTest,
                        Combine(Values(1, 2), Values(0, 1)));


}  // namespace webrtc
