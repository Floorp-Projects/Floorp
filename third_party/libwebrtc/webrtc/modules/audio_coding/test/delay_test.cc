/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <math.h>
#include <string.h>

#include <iostream>
#include <memory>

#include "common_types.h"  // NOLINT(build/include)
#include "modules/audio_coding/codecs/audio_format_conversion.h"
#include "modules/audio_coding/include/audio_coding_module.h"
#include "modules/audio_coding/include/audio_coding_module_typedefs.h"
#include "modules/audio_coding/test/Channel.h"
#include "modules/audio_coding/test/PCMFile.h"
#include "modules/audio_coding/test/utility.h"
#include "rtc_base/flags.h"
#include "system_wrappers/include/event_wrapper.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"
#include "typedefs.h"  // NOLINT(build/include)

DEFINE_string(codec, "isac", "Codec Name");
DEFINE_int(sample_rate_hz, 16000, "Sampling rate in Hertz.");
DEFINE_int(num_channels, 1, "Number of Channels.");
DEFINE_string(input_file, "", "Input file, PCM16 32 kHz, optional.");
DEFINE_int(delay, 0, "Delay in millisecond.");
DEFINE_bool(dtx, false, "Enable DTX at the sender side.");
DEFINE_bool(packet_loss, false, "Apply packet loss, c.f. Channel{.cc, .h}.");
DEFINE_bool(fec, false, "Use Forward Error Correction (FEC).");
DEFINE_bool(help, false, "Print this message.");

namespace webrtc {

namespace {

struct CodecSettings {
  char name[50];
  int sample_rate_hz;
  int num_channels;
};

struct AcmSettings {
  bool dtx;
  bool fec;
};

struct TestSettings {
  CodecSettings codec;
  AcmSettings acm;
  bool packet_loss;
};

}  // namespace

class DelayTest {
 public:
  DelayTest()
      : acm_a_(AudioCodingModule::Create()),
        acm_b_(AudioCodingModule::Create()),
        channel_a2b_(new Channel),
        test_cntr_(0),
        encoding_sample_rate_hz_(8000) {}

  ~DelayTest() {
    if (channel_a2b_ != NULL) {
      delete channel_a2b_;
      channel_a2b_ = NULL;
    }
    in_file_a_.Close();
  }

  void Initialize() {
    test_cntr_ = 0;
    std::string file_name = webrtc::test::ResourcePath(
        "audio_coding/testfile32kHz", "pcm");
    if (strlen(FLAG_input_file) > 0)
      file_name = FLAG_input_file;
    in_file_a_.Open(file_name, 32000, "rb");
    ASSERT_EQ(0, acm_a_->InitializeReceiver()) <<
        "Couldn't initialize receiver.\n";
    ASSERT_EQ(0, acm_b_->InitializeReceiver()) <<
        "Couldn't initialize receiver.\n";

    if (FLAG_delay > 0) {
      ASSERT_EQ(0, acm_b_->SetMinimumPlayoutDelay(FLAG_delay)) <<
          "Failed to set minimum delay.\n";
    }

    int num_encoders = acm_a_->NumberOfCodecs();
    CodecInst my_codec_param;
    for (int n = 0; n < num_encoders; n++) {
      EXPECT_EQ(0, acm_b_->Codec(n, &my_codec_param)) <<
          "Failed to get codec.";
      if (STR_CASE_CMP(my_codec_param.plname, "opus") == 0)
        my_codec_param.channels = 1;
      else if (my_codec_param.channels > 1)
        continue;
      if (STR_CASE_CMP(my_codec_param.plname, "CN") == 0 &&
          my_codec_param.plfreq == 48000)
        continue;
      if (STR_CASE_CMP(my_codec_param.plname, "telephone-event") == 0)
        continue;
      ASSERT_EQ(true,
                acm_b_->RegisterReceiveCodec(my_codec_param.pltype,
                                             CodecInstToSdp(my_codec_param)));
    }

    // Create and connect the channel
    ASSERT_EQ(0, acm_a_->RegisterTransportCallback(channel_a2b_)) <<
        "Couldn't register Transport callback.\n";
    channel_a2b_->RegisterReceiverACM(acm_b_.get());
  }

  void Perform(const TestSettings* config, size_t num_tests, int duration_sec,
               const char* output_prefix) {
    for (size_t n = 0; n < num_tests; ++n) {
      ApplyConfig(config[n]);
      Run(duration_sec, output_prefix);
    }
  }

 private:
  void ApplyConfig(const TestSettings& config) {
    printf("====================================\n");
    printf("Test %d \n"
           "Codec: %s, %d kHz, %d channel(s)\n"
           "ACM: DTX %s, FEC %s\n"
           "Channel: %s\n",
           ++test_cntr_, config.codec.name, config.codec.sample_rate_hz,
           config.codec.num_channels, config.acm.dtx ? "on" : "off",
           config.acm.fec ? "on" : "off",
           config.packet_loss ? "with packet-loss" : "no packet-loss");
    SendCodec(config.codec);
    ConfigAcm(config.acm);
    ConfigChannel(config.packet_loss);
  }

  void SendCodec(const CodecSettings& config) {
    CodecInst my_codec_param;
    ASSERT_EQ(0, AudioCodingModule::Codec(
              config.name, &my_codec_param, config.sample_rate_hz,
              config.num_channels)) << "Specified codec is not supported.\n";

    encoding_sample_rate_hz_ = my_codec_param.plfreq;
    ASSERT_EQ(0, acm_a_->RegisterSendCodec(my_codec_param)) <<
        "Failed to register send-codec.\n";
  }

  void ConfigAcm(const AcmSettings& config) {
    ASSERT_EQ(0, acm_a_->SetVAD(config.dtx, config.dtx, VADAggr)) <<
        "Failed to set VAD.\n";
    ASSERT_EQ(0, acm_a_->SetREDStatus(config.fec)) <<
        "Failed to set RED.\n";
  }

  void ConfigChannel(bool packet_loss) {
    channel_a2b_->SetFECTestWithPacketLoss(packet_loss);
  }

  void OpenOutFile(const char* output_id) {
    std::stringstream file_stream;
    file_stream << "delay_test_" << FLAG_codec << "_" << FLAG_sample_rate_hz
        << "Hz" << "_" << FLAG_delay << "ms.pcm";
    std::cout << "Output file: " << file_stream.str() << std::endl << std::endl;
    std::string file_name = webrtc::test::OutputPath() + file_stream.str();
    out_file_b_.Open(file_name.c_str(), 32000, "wb");
  }

  void Run(int duration_sec, const char* output_prefix) {
    OpenOutFile(output_prefix);
    AudioFrame audio_frame;
    uint32_t out_freq_hz_b = out_file_b_.SamplingFrequency();

    int num_frames = 0;
    int in_file_frames = 0;
    uint32_t received_ts;
    double average_delay = 0;
    double inst_delay_sec = 0;
    while (num_frames < (duration_sec * 100)) {
      if (in_file_a_.EndOfFile()) {
        in_file_a_.Rewind();
      }

      // Print delay information every 16 frame
      if ((num_frames & 0x3F) == 0x3F) {
        NetworkStatistics statistics;
        acm_b_->GetNetworkStatistics(&statistics);
        fprintf(stdout, "delay: min=%3d  max=%3d  mean=%3d  median=%3d"
                " ts-based average = %6.3f, "
                "curr buff-lev = %4u opt buff-lev = %4u \n",
                statistics.minWaitingTimeMs, statistics.maxWaitingTimeMs,
                statistics.meanWaitingTimeMs, statistics.medianWaitingTimeMs,
                average_delay, statistics.currentBufferSize,
                statistics.preferredBufferSize);
        fflush (stdout);
      }

      in_file_a_.Read10MsData(audio_frame);
      ASSERT_GE(acm_a_->Add10MsData(audio_frame), 0);
      bool muted;
      ASSERT_EQ(0,
                acm_b_->PlayoutData10Ms(out_freq_hz_b, &audio_frame, &muted));
      RTC_DCHECK(!muted);
      out_file_b_.Write10MsData(
          audio_frame.data(),
          audio_frame.samples_per_channel_ * audio_frame.num_channels_);
      received_ts = channel_a2b_->LastInTimestamp();
      rtc::Optional<uint32_t> playout_timestamp = acm_b_->PlayoutTimestamp();
      ASSERT_TRUE(playout_timestamp);
      inst_delay_sec = static_cast<uint32_t>(received_ts - *playout_timestamp) /
                       static_cast<double>(encoding_sample_rate_hz_);

      if (num_frames > 10)
        average_delay = 0.95 * average_delay + 0.05 * inst_delay_sec;

      ++num_frames;
      ++in_file_frames;
    }
    out_file_b_.Close();
  }

  std::unique_ptr<AudioCodingModule> acm_a_;
  std::unique_ptr<AudioCodingModule> acm_b_;

  Channel* channel_a2b_;

  PCMFile in_file_a_;
  PCMFile out_file_b_;
  int test_cntr_;
  int encoding_sample_rate_hz_;
};

}  // namespace webrtc

int main(int argc, char* argv[]) {
  if (rtc::FlagList::SetFlagsFromCommandLine(&argc, argv, true)) {
    return 1;
  }
  if (FLAG_help) {
    rtc::FlagList::Print(nullptr, false);
    return 0;
  }

  webrtc::TestSettings test_setting;
  strcpy(test_setting.codec.name, FLAG_codec);

  if (FLAG_sample_rate_hz != 8000 &&
      FLAG_sample_rate_hz != 16000 &&
      FLAG_sample_rate_hz != 32000 &&
      FLAG_sample_rate_hz != 48000) {
    std::cout << "Invalid sampling rate.\n";
    return 1;
  }
  test_setting.codec.sample_rate_hz = FLAG_sample_rate_hz;
  if (FLAG_num_channels < 1 || FLAG_num_channels > 2) {
    std::cout << "Only mono and stereo are supported.\n";
    return 1;
  }
  test_setting.codec.num_channels = FLAG_num_channels;
  test_setting.acm.dtx = FLAG_dtx;
  test_setting.acm.fec = FLAG_fec;
  test_setting.packet_loss = FLAG_packet_loss;

  webrtc::DelayTest delay_test;
  delay_test.Initialize();
  delay_test.Perform(&test_setting, 1, 240, "delay_test");
  return 0;
}
