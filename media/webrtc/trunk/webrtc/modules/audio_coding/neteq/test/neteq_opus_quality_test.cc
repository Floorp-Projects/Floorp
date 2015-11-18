/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/codecs/opus/interface/opus_interface.h"
#include "webrtc/modules/audio_coding/codecs/opus/opus_inst.h"
#include "webrtc/modules/audio_coding/neteq/tools/neteq_quality_test.h"
#include "webrtc/test/testsupport/fileutils.h"

using google::RegisterFlagValidator;
using google::ParseCommandLineFlags;
using std::string;
using testing::InitGoogleTest;

namespace webrtc {
namespace test {

static const int kOpusBlockDurationMs = 20;
static const int kOpusSamplingKhz = 48;

// Define switch for input file name.
static bool ValidateInFilename(const char* flagname, const string& value) {
  FILE* fid = fopen(value.c_str(), "rb");
  if (fid != NULL) {
    fclose(fid);
    return true;
  }
  printf("Invalid input filename.");
  return false;
}

DEFINE_string(in_filename,
              ResourcePath("audio_coding/speech_mono_32_48kHz", "pcm"),
              "Filename for input audio (should be 48 kHz sampled raw data).");

static const bool in_filename_dummy =
    RegisterFlagValidator(&FLAGS_in_filename, &ValidateInFilename);

// Define switch for output file name.
static bool ValidateOutFilename(const char* flagname, const string& value) {
  FILE* fid = fopen(value.c_str(), "wb");
  if (fid != NULL) {
    fclose(fid);
    return true;
  }
  printf("Invalid output filename.");
  return false;
}

DEFINE_string(out_filename, OutputPath() + "neteq_opus_quality_test.pcm",
              "Name of output audio file.");

static const bool out_filename_dummy =
    RegisterFlagValidator(&FLAGS_out_filename, &ValidateOutFilename);

// Define switch for channels.
static bool ValidateChannels(const char* flagname, int32_t value) {
  if (value == 1 || value == 2)
    return true;
  printf("Invalid number of channels, should be either 1 or 2.");
  return false;
}

DEFINE_int32(channels, 1, "Number of channels in input audio.");

static const bool channels_dummy =
    RegisterFlagValidator(&FLAGS_channels, &ValidateChannels);

// Define switch for bit rate.
static bool ValidateBitRate(const char* flagname, int32_t value) {
  if (value >= 6 && value <= 510)
    return true;
  printf("Invalid bit rate, should be between 6 and 510 kbps.");
  return false;
}

DEFINE_int32(bit_rate_kbps, 32, "Target bit rate (kbps).");

static const bool bit_rate_dummy =
    RegisterFlagValidator(&FLAGS_bit_rate_kbps, &ValidateBitRate);

// Define switch for reported packet loss rate.
static bool ValidatePacketLossRate(const char* flagname, int32_t value) {
  if (value >= 0 && value <= 100)
    return true;
  printf("Invalid packet loss percentile, should be between 0 and 100.");
  return false;
}

DEFINE_int32(reported_loss_rate, 10, "Reported percentile of packet loss.");

static const bool reported_loss_rate_dummy =
    RegisterFlagValidator(&FLAGS_reported_loss_rate, &ValidatePacketLossRate);

// Define switch for runtime.
static bool ValidateRuntime(const char* flagname, int32_t value) {
  if (value > 0)
    return true;
  printf("Invalid runtime, should be greater than 0.");
  return false;
}

DEFINE_int32(runtime_ms, 10000, "Simulated runtime (milliseconds).");
static const bool runtime_dummy =
    RegisterFlagValidator(&FLAGS_runtime_ms, &ValidateRuntime);

DEFINE_bool(fec, true, "Whether to enable FEC for encoding.");

DEFINE_bool(dtx, true, "Whether to enable DTX for encoding.");

// Define switch for number of sub packets to repacketize.
static bool ValidateSubPackets(const char* flagname, int32_t value) {
  if (value >= 1 && value <= 3)
    return true;
  printf("Invalid number of sub packets, should be between 1 and 3.");
  return false;
}
DEFINE_int32(sub_packets, 1, "Number of sub packets to repacketize.");
static const bool sub_packets_dummy =
    RegisterFlagValidator(&FLAGS_sub_packets, &ValidateSubPackets);

class NetEqOpusQualityTest : public NetEqQualityTest {
 protected:
  NetEqOpusQualityTest();
  void SetUp() override;
  void TearDown() override;
  virtual int EncodeBlock(int16_t* in_data, int block_size_samples,
                          uint8_t* payload, int max_bytes);
 private:
  WebRtcOpusEncInst* opus_encoder_;
  OpusRepacketizer* repacketizer_;
  int sub_block_size_samples_;
  int channels_;
  int bit_rate_kbps_;
  bool fec_;
  bool dtx_;
  int target_loss_rate_;
  int sub_packets_;
};

NetEqOpusQualityTest::NetEqOpusQualityTest()
    : NetEqQualityTest(kOpusBlockDurationMs * FLAGS_sub_packets,
                       kOpusSamplingKhz,
                       kOpusSamplingKhz,
                       (FLAGS_channels == 1) ? kDecoderOpus : kDecoderOpus_2ch,
                       FLAGS_channels,
                       FLAGS_in_filename,
                       FLAGS_out_filename),
      opus_encoder_(NULL),
      repacketizer_(NULL),
      sub_block_size_samples_(kOpusBlockDurationMs * kOpusSamplingKhz),
      channels_(FLAGS_channels),
      bit_rate_kbps_(FLAGS_bit_rate_kbps),
      fec_(FLAGS_fec),
      dtx_(FLAGS_dtx),
      target_loss_rate_(FLAGS_reported_loss_rate),
      sub_packets_(FLAGS_sub_packets) {
}

void NetEqOpusQualityTest::SetUp() {
  // If channels_ == 1, use Opus VOIP mode, otherwise, audio mode.
  int app = channels_ == 1 ? 0 : 1;
  // Create encoder memory.
  WebRtcOpus_EncoderCreate(&opus_encoder_, channels_, app);
  ASSERT_TRUE(opus_encoder_);

  // Create repacketizer.
  repacketizer_ = opus_repacketizer_create();
  ASSERT_TRUE(repacketizer_);

  // Set bitrate.
  EXPECT_EQ(0, WebRtcOpus_SetBitRate(opus_encoder_, bit_rate_kbps_ * 1000));
  if (fec_) {
    EXPECT_EQ(0, WebRtcOpus_EnableFec(opus_encoder_));
  }
  if (dtx_) {
    EXPECT_EQ(0, WebRtcOpus_EnableDtx(opus_encoder_));
  }
  EXPECT_EQ(0, WebRtcOpus_SetPacketLossRate(opus_encoder_,
                                            target_loss_rate_));
  NetEqQualityTest::SetUp();
}

void NetEqOpusQualityTest::TearDown() {
  // Free memory.
  EXPECT_EQ(0, WebRtcOpus_EncoderFree(opus_encoder_));
  opus_repacketizer_destroy(repacketizer_);
  NetEqQualityTest::TearDown();
}

int NetEqOpusQualityTest::EncodeBlock(int16_t* in_data,
                                      int block_size_samples,
                                      uint8_t* payload, int max_bytes) {
  EXPECT_EQ(block_size_samples, sub_block_size_samples_ * sub_packets_);
  int16_t* pointer = in_data;
  int value;
  opus_repacketizer_init(repacketizer_);
  for (int idx = 0; idx < sub_packets_; idx++) {
    value = WebRtcOpus_Encode(opus_encoder_, pointer, sub_block_size_samples_,
                              max_bytes, payload);
    if (OPUS_OK != opus_repacketizer_cat(repacketizer_, payload, value)) {
      opus_repacketizer_init(repacketizer_);
      // If the repacketization fails, we discard this frame.
      return 0;
    }
    pointer += sub_block_size_samples_ * channels_;
  }
  value = opus_repacketizer_out(repacketizer_, payload, max_bytes);
  EXPECT_GE(value, 0);
  return value;
}

TEST_F(NetEqOpusQualityTest, Test) {
  Simulate(FLAGS_runtime_ms);
}

}  // namespace test
}  // namespace webrtc
