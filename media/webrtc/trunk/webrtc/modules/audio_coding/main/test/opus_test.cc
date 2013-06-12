/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/main/test/opus_test.h"

#include <cassert>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/common_types.h"
#include "webrtc/engine_configurations.h"
#include "webrtc/modules/audio_coding/codecs/opus/interface/opus_interface.h"
#include "webrtc/modules/audio_coding/main/interface/audio_coding_module_typedefs.h"
#include "webrtc/modules/audio_coding/main/source/acm_codec_database.h"
#include "webrtc/modules/audio_coding/main/source/acm_opus.h"
#include "webrtc/modules/audio_coding/main/test/TestStereo.h"
#include "webrtc/modules/audio_coding/main/test/utility.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace webrtc {

OpusTest::OpusTest()
    : acm_receiver_(NULL),
      channel_a2b_(NULL),
      counter_(0),
      payload_type_(255),
      rtp_timestamp_(0) {
}

OpusTest::~OpusTest() {
  if (acm_receiver_ != NULL) {
    AudioCodingModule::Destroy(acm_receiver_);
    acm_receiver_ = NULL;
  }
  if (channel_a2b_ != NULL) {
    delete channel_a2b_;
    channel_a2b_ = NULL;
  }
  if (opus_mono_encoder_ != NULL) {
    WebRtcOpus_EncoderFree(opus_mono_encoder_);
    opus_mono_encoder_ = NULL;
  }
  if (opus_stereo_encoder_ != NULL) {
    WebRtcOpus_EncoderFree(opus_stereo_encoder_);
    opus_stereo_encoder_ = NULL;
  }
}

void OpusTest::Perform() {
#ifndef WEBRTC_CODEC_OPUS
  // Opus isn't defined, exit.
  return;
#else
  uint16_t frequency_hz;
  int audio_channels;
  int16_t test_cntr = 0;

  // Open both mono and stereo test files in 32 kHz.
  const std::string file_name_stereo =
      webrtc::test::ResourcePath("audio_coding/teststereo32kHz", "pcm");
  const std::string file_name_mono =
      webrtc::test::ResourcePath("audio_coding/testfile32kHz", "pcm");
  frequency_hz = 32000;
  in_file_stereo_.Open(file_name_stereo, frequency_hz, "rb");
  in_file_stereo_.ReadStereo(true);
  in_file_mono_.Open(file_name_mono, frequency_hz, "rb");
  in_file_mono_.ReadStereo(false);

  // Create Opus encoders for mono and stereo.
  ASSERT_GT(WebRtcOpus_EncoderCreate(&opus_mono_encoder_, 1), -1);
  ASSERT_GT(WebRtcOpus_EncoderCreate(&opus_stereo_encoder_, 2), -1);

  // Create and initialize one ACM, to be used as receiver.
  acm_receiver_ = AudioCodingModule::Create(0);
  ASSERT_TRUE(acm_receiver_ != NULL);
  EXPECT_EQ(0, acm_receiver_->InitializeReceiver());

  // Register Opus stereo as receiving codec.
  CodecInst opus_codec_param;
  int codec_id = acm_receiver_->Codec("opus", 48000, 2);
  EXPECT_EQ(0, acm_receiver_->Codec(codec_id, &opus_codec_param));
  payload_type_ = opus_codec_param.pltype;
  EXPECT_EQ(0, acm_receiver_->RegisterReceiveCodec(opus_codec_param));

  // Create and connect the channel.
  channel_a2b_ = new TestPackStereo;
  channel_a2b_->RegisterReceiverACM(acm_receiver_);

  //
  // Test Stereo.
  //

  channel_a2b_->set_codec_mode(kStereo);
  audio_channels = 2;
  test_cntr++;
  OpenOutFile(test_cntr);

  // Run Opus with 2.5 ms frame size.
  Run(channel_a2b_, audio_channels, 64000, 120);

  // Run Opus with 5 ms frame size.
  Run(channel_a2b_, audio_channels, 64000, 240);

  // Run Opus with 10 ms frame size.
  Run(channel_a2b_, audio_channels, 64000, 480);

  // Run Opus with 20 ms frame size.
  Run(channel_a2b_, audio_channels, 64000, 960);

  // Run Opus with 40 ms frame size.
  Run(channel_a2b_, audio_channels, 64000, 1920);

  // Run Opus with 60 ms frame size.
  Run(channel_a2b_, audio_channels, 64000, 2880);

  out_file_.Close();

  //
  // Test Mono.
  //
  channel_a2b_->set_codec_mode(kMono);
  audio_channels = 1;
  test_cntr++;
  OpenOutFile(test_cntr);

  // Register Opus mono as receiving codec.
  opus_codec_param.channels = 1;
  EXPECT_EQ(0, acm_receiver_->RegisterReceiveCodec(opus_codec_param));

  // Run Opus with 2.5 ms frame size.
  Run(channel_a2b_, audio_channels, 32000, 120);

  // Run Opus with 5 ms frame size.
  Run(channel_a2b_, audio_channels, 32000, 240);

  // Run Opus with 10 ms frame size.
  Run(channel_a2b_, audio_channels, 32000, 480);

  // Run Opus with 20 ms frame size.
  Run(channel_a2b_, audio_channels, 32000, 960);

  // Run Opus with 40 ms frame size.
  Run(channel_a2b_, audio_channels, 32000, 1920);

  // Run Opus with 60 ms frame size.
  Run(channel_a2b_, audio_channels, 32000, 2880);

  // Close the files.
  in_file_stereo_.Close();
  in_file_mono_.Close();
  out_file_.Close();
#endif
}

void OpusTest::Run(TestPackStereo* channel, int channels, int bitrate,
                   int frame_length, int percent_loss) {
  AudioFrame audio_frame;
  int32_t out_freq_hz_b = out_file_.SamplingFrequency();
  int16_t audio[480 * 12 * 2];  // Can hold 120 ms stereo audio.
  int written_samples = 0;
  int read_samples = 0;
  channel->reset_payload_size();

  // Set encoder rate.
  EXPECT_EQ(0, WebRtcOpus_SetBitRate(opus_mono_encoder_, bitrate));
  EXPECT_EQ(0, WebRtcOpus_SetBitRate(opus_stereo_encoder_, bitrate));

  while (1) {
    // Simulate packet loss by setting |packet_loss_| to "true" in
    // |percent_loss| percent of the loops.
    // TODO(tlegrand): Move handling of loss simulation to TestPackStereo.
    if (percent_loss > 0) {
      if (counter_ == floor((100 / percent_loss) + 0.5)) {
        counter_ = 0;
        channel->set_lost_packet(true);
      } else {
        channel->set_lost_packet(false);
      }
      counter_++;
    }

    // Get 10 msec of audio.
    if (channels == 1) {
      if (in_file_mono_.EndOfFile()) {
        break;
      }
      in_file_mono_.Read10MsData(audio_frame);
    } else {
      if (in_file_stereo_.EndOfFile()) {
        break;
      }
      in_file_stereo_.Read10MsData(audio_frame);
    }

    // Input audio is sampled at 32 kHz, but Opus operates at 48 kHz.
    // Resampling is required.
    EXPECT_EQ(480, resampler_.Resample10Msec(audio_frame.data_, 32000,
                                             &audio[written_samples], 48000,
                                             channels));
    written_samples += 480 * channels;

    // Sometimes we need to loop over the audio vector to produce the right
    // number of packets.
    int loop_encode = (written_samples - read_samples) /
        (channels * frame_length);

    if (loop_encode > 0) {
      const int kMaxBytes = 1000;  // Maximum number of bytes for one packet.
      int16_t bitstream_len_byte;
      uint8_t bitstream[kMaxBytes];
      for (int i = 0; i < loop_encode; i++) {
        if (channels == 1) {
          bitstream_len_byte = WebRtcOpus_Encode(
              opus_mono_encoder_, &audio[read_samples],
              frame_length, kMaxBytes, bitstream);
          ASSERT_GT(bitstream_len_byte, -1);
        } else {
          bitstream_len_byte = WebRtcOpus_Encode(
              opus_stereo_encoder_, &audio[read_samples],
              frame_length, kMaxBytes, bitstream);
          ASSERT_GT(bitstream_len_byte, -1);
        }
        channel->SendData(kAudioFrameSpeech, payload_type_, rtp_timestamp_,
                          bitstream, bitstream_len_byte, NULL);
        rtp_timestamp_ += frame_length;
        read_samples += frame_length * channels;
      }
      if (read_samples == written_samples) {
        read_samples = 0;
        written_samples = 0;
      }
    }

    // Run received side of ACM.
    CHECK_ERROR(acm_receiver_->PlayoutData10Ms(out_freq_hz_b, &audio_frame));

    // Write output speech to file.
    out_file_.Write10MsData(
        audio_frame.data_,
        audio_frame.samples_per_channel_ * audio_frame.num_channels_);
  }

  if (in_file_mono_.EndOfFile()) {
    in_file_mono_.Rewind();
  }
  if (in_file_stereo_.EndOfFile()) {
    in_file_stereo_.Rewind();
  }
  // Reset in case we ended with a lost packet.
  channel->set_lost_packet(false);
}

void OpusTest::OpenOutFile(int test_number) {
  std::string file_name;
  std::stringstream file_stream;
  file_stream << webrtc::test::OutputPath() << "opustest_out_"
      << test_number << ".pcm";
  file_name = file_stream.str();
  out_file_.Open(file_name, 32000, "wb");
}

}  // namespace webrtc
