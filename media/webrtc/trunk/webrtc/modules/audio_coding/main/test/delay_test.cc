/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/main/interface/audio_coding_module.h"

#include <math.h>

#include <cassert>
#include <iostream>

#include "gflags/gflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/common_types.h"
#include "webrtc/engine_configurations.h"
#include "webrtc/modules/audio_coding/main/interface/audio_coding_module_typedefs.h"
#include "webrtc/modules/audio_coding/main/source/acm_common_defs.h"
#include "webrtc/modules/audio_coding/main/test/Channel.h"
#include "webrtc/modules/audio_coding/main/test/PCMFile.h"
#include "webrtc/modules/audio_coding/main/test/utility.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/test/testsupport/fileutils.h"

DEFINE_string(codec, "isac", "Codec Name");
DEFINE_int32(sample_rate_hz, 16000, "Sampling rate in Hertz.");
DEFINE_int32(num_channels, 1, "Number of Channels.");
DEFINE_string(input_file, "", "Input file, PCM16 32 kHz, optional.");
DEFINE_int32(delay, 0, "Delay in millisecond.");
DEFINE_int32(init_delay, 0, "Initial delay in millisecond.");
DEFINE_bool(dtx, false, "Enable DTX at the sender side.");

namespace webrtc {

namespace {

struct CodecConfig {
  char name[50];
  int sample_rate_hz;
  int num_channels;
};

struct AcmConfig {
  bool dtx;
  bool fec;
};

struct Config {
  CodecConfig codec;
  AcmConfig acm;
  bool packet_loss;
};

}

class DelayTest {
 public:

  DelayTest()
     : acm_a_(NULL),
       acm_b_(NULL),
       channel_a2b_(NULL),
       test_cntr_(0),
       encoding_sample_rate_hz_(8000) {
  }

  ~DelayTest() {}

  void TearDown() {
    if(acm_a_ != NULL) {
      AudioCodingModule::Destroy(acm_a_);
      acm_a_ = NULL;
    }
    if(acm_b_ != NULL) {
      AudioCodingModule::Destroy(acm_b_);
      acm_b_ = NULL;
    }
    if(channel_a2b_ != NULL) {
      delete channel_a2b_;
      channel_a2b_ = NULL;
    }
  }

  void SetUp() {
    test_cntr_ = 0;
    std::string file_name =
        webrtc::test::ResourcePath("audio_coding/testfile32kHz", "pcm");
    if (FLAGS_input_file.size() > 0)
      file_name = FLAGS_input_file;
    in_file_a_.Open(file_name, 32000, "rb");
    acm_a_ = AudioCodingModule::Create(0);
    acm_b_ = AudioCodingModule::Create(1);
    acm_a_->InitializeReceiver();
    acm_b_->InitializeReceiver();
    if (FLAGS_init_delay > 0) {
      ASSERT_EQ(0, acm_b_->SetInitialPlayoutDelay(FLAGS_init_delay));
    }

    if (FLAGS_delay > 0) {
      ASSERT_EQ(0, acm_b_->SetMinimumPlayoutDelay(FLAGS_delay));
    }

    uint8_t num_encoders = acm_a_->NumberOfCodecs();
    CodecInst my_codec_param;
    for(int n = 0; n < num_encoders; n++) {
      acm_b_->Codec(n, &my_codec_param);
      if (STR_CASE_CMP(my_codec_param.plname, "opus") == 0)
        my_codec_param.channels = 1;
      else if  (my_codec_param.channels > 1)
        continue;
      if (STR_CASE_CMP(my_codec_param.plname, "CN") == 0 &&
          my_codec_param.plfreq == 48000)
          continue;
      if (STR_CASE_CMP(my_codec_param.plname, "telephone-event") == 0)
        continue;
      acm_b_->RegisterReceiveCodec(my_codec_param);
    }

    // Create and connect the channel
    channel_a2b_ = new Channel;
    acm_a_->RegisterTransportCallback(channel_a2b_);
    channel_a2b_->RegisterReceiverACM(acm_b_);
  }

  void Perform(const Config* config, size_t num_tests, int duration_sec,
               const char* output_prefix) {
    for (size_t n = 0; n < num_tests; ++n) {
      ApplyConfig(config[n]);
      Run(duration_sec, output_prefix);
    }
  }

 private:

  void ApplyConfig(const Config& config) {
    printf("====================================\n");
    printf("Test %d \n"
        "Codec: %s, %d kHz, %d channel(s)\n"
        "ACM: DTX %s, FEC %s\n"
        "Channel: %s\n",
        ++test_cntr_,
        config.codec.name, config.codec.sample_rate_hz,
        config.codec.num_channels, config.acm.dtx ? "on" : "off",
        config.acm.fec ? "on" : "off",
        config.packet_loss ? "with packet-loss" : "no packet-loss");
    SendCodec(config.codec);
    ConfigAcm(config.acm);
    ConfigChannel(config.packet_loss);
  }

  void SendCodec(const CodecConfig& config) {
    CodecInst my_codec_param;
    ASSERT_EQ(0, AudioCodingModule::Codec(config.name, &my_codec_param,
                                          config.sample_rate_hz,
                                          config.num_channels));
    encoding_sample_rate_hz_ = my_codec_param.plfreq;
    ASSERT_EQ(0, acm_a_->RegisterSendCodec(my_codec_param));
  }

  void ConfigAcm(const AcmConfig& config) {
    ASSERT_EQ(0, acm_a_->SetVAD(config.dtx, config.dtx, VADAggr));
    ASSERT_EQ(0, acm_a_->SetFECStatus(config.fec));
  }

  void ConfigChannel(bool packet_loss) {
    channel_a2b_->SetFECTestWithPacketLoss(packet_loss);
  }

  void OpenOutFile(const char* output_id) {
    std::stringstream file_stream;
    file_stream << "delay_test_" << FLAGS_codec << "_"
        << FLAGS_sample_rate_hz << "Hz" << "_"
        << FLAGS_init_delay << "ms_"
        << FLAGS_delay << "ms.pcm";
    std::cout << "Output file: " << file_stream.str() << std::endl <<std::endl;
    std::string file_name = webrtc::test::OutputPath() + file_stream.str();
    out_file_b_.Open(file_name.c_str(), 32000, "wb");
  }

  void Run(int duration_sec, const char* output_prefix) {
    OpenOutFile(output_prefix);
    AudioFrame audio_frame;
    uint32_t out_freq_hz_b = out_file_b_.SamplingFrequency();

    int num_frames = 0;
    int in_file_frames = 0;
    uint32_t playout_ts;
    uint32_t received_ts;
    double average_delay = 0;
    double inst_delay_sec = 0;
    while(num_frames < (duration_sec * 100)) {
      if (in_file_a_.EndOfFile()) {
        in_file_a_.Rewind();
      }

      // Print delay information every 16 frame
      if ((num_frames & 0x3F) == 0x3F) {
        ACMNetworkStatistics statistics;
        acm_b_->NetworkStatistics(&statistics);
        fprintf(stdout, "delay: min=%3d  max=%3d  mean=%3d  median=%3d"
                " ts-based average = %6.3f, "
                "curr buff-lev = %4u opt buff-lev = %4u \n",
                statistics.minWaitingTimeMs,
                statistics.maxWaitingTimeMs,
                statistics.meanWaitingTimeMs,
                statistics.medianWaitingTimeMs,
                average_delay,
                statistics.currentBufferSize,
                statistics.preferredBufferSize);
        fflush(stdout);
      }

      in_file_a_.Read10MsData(audio_frame);
      ASSERT_EQ(0, acm_a_->Add10MsData(audio_frame));
      ASSERT_LE(0, acm_a_->Process());
      ASSERT_EQ(0, acm_b_->PlayoutData10Ms(out_freq_hz_b, &audio_frame));
      out_file_b_.Write10MsData(audio_frame.data_,
                                audio_frame.samples_per_channel_ *
                                audio_frame.num_channels_);
      acm_b_->PlayoutTimestamp(&playout_ts);
      received_ts = channel_a2b_->LastInTimestamp();
      inst_delay_sec = static_cast<uint32_t>(received_ts - playout_ts) /
          static_cast<double>(encoding_sample_rate_hz_);

      if (num_frames > 10)
        average_delay = 0.95 * average_delay + 0.05 * inst_delay_sec;

      ++num_frames;
      ++in_file_frames;
    }
    out_file_b_.Close();
  }

  AudioCodingModule* acm_a_;
  AudioCodingModule* acm_b_;

  Channel* channel_a2b_;

  PCMFile in_file_a_;
  PCMFile out_file_b_;
  int test_cntr_;
  int encoding_sample_rate_hz_;
};

} // namespace webrtc

int main(int argc, char* argv[]) {

  google::ParseCommandLineFlags(&argc, &argv, true);

  webrtc::Config config;
  strcpy(config.codec.name, FLAGS_codec.c_str());
  config.codec.sample_rate_hz = FLAGS_sample_rate_hz;
  config.codec.num_channels = FLAGS_num_channels;
  config.acm.dtx = FLAGS_dtx;
  config.acm.fec = false;
  config.packet_loss = false;

  webrtc::DelayTest delay_test;
  delay_test.SetUp();
  delay_test.Perform(&config, 1, 240, "delay_test");
  delay_test.TearDown();
}
