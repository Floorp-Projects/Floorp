/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cmath>
#include <cstdio>

#include <algorithm>

#include "gflags/gflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/audio_processing/agc/agc.h"
#include "webrtc/modules/audio_processing/agc/utility.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/test/testsupport/trace_to_stderr.h"
#include "webrtc/tools/agc/agc_manager.h"
#include "webrtc/tools/agc/test_utils.h"
#include "webrtc/voice_engine/include/mock/fake_voe_external_media.h"
#include "webrtc/voice_engine/include/mock/mock_voe_volume_control.h"

DEFINE_string(in, "in.pcm", "input filename");
DEFINE_string(out, "out.pcm", "output filename");
DEFINE_int32(rate, 16000, "sample rate in Hz");
DEFINE_int32(channels, 1, "number of channels");
DEFINE_int32(level, -18, "target level in RMS dBFs [-100, 0]");
DEFINE_bool(limiter, true, "enable a limiter for the compression stage");
DEFINE_int32(cmp_level, 2, "target level in dBFs for the compression stage");
DEFINE_int32(mic_gain, 80, "range of gain provided by the virtual mic in dB");
DEFINE_int32(gain_offset, 0,
             "an amount (in dB) to add to every entry in the gain map");
DEFINE_string(gain_file, "",
    "filename providing a mic gain mapping. The file should be text containing "
    "a (floating-point) gain entry in dBFs per line corresponding to levels "
    "from 0 to 255.");

using ::testing::_;
using ::testing::ByRef;
using ::testing::DoAll;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgReferee;

namespace webrtc {
namespace {

const char kUsage[] = "\nProcess an audio file to simulate an analog agc.";

void ReadGainMapFromFile(FILE* file, int offset, int gain_map[256]) {
  for (int i = 0; i < 256; ++i) {
    float gain = 0;
    ASSERT_EQ(1, fscanf(file, "%f", &gain));
    gain_map[i] = std::floor(gain + 0.5);
  }

  // Adjust from dBFs to gain in dB. We assume that level 127 provides 0 dB
  // gain. This corresponds to the interpretation in MicLevel2Gain().
  const int midpoint = gain_map[127];
  printf("Gain map\n");
  for (int i = 0; i < 256; ++i) {
    gain_map[i] += offset - midpoint;
    if (i % 5 == 0) {
      printf("%d: %d dB\n", i, gain_map[i]);
    }
  }
}

void CalculateGainMap(int gain_range_db, int offset, int gain_map[256]) {
  printf("Gain map\n");
  for (int i = 0; i < 256; ++i) {
    gain_map[i] = std::floor(MicLevel2Gain(gain_range_db, i) + 0.5) + offset;
    if (i % 5 == 0) {
      printf("%d: %d dB\n", i, gain_map[i]);
    }
  }
}

void RunAgc() {
  test::TraceToStderr trace_to_stderr(true);
  FILE* in_file = fopen(FLAGS_in.c_str(), "rb");
  ASSERT_TRUE(in_file != NULL);
  FILE* out_file = fopen(FLAGS_out.c_str(), "wb");
  ASSERT_TRUE(out_file != NULL);

  int gain_map[256];
  if (FLAGS_gain_file != "") {
    FILE* gain_file = fopen(FLAGS_gain_file.c_str(), "rt");
    ASSERT_TRUE(gain_file != NULL);
    ReadGainMapFromFile(gain_file, FLAGS_gain_offset, gain_map);
    fclose(gain_file);
  } else {
    CalculateGainMap(FLAGS_mic_gain, FLAGS_gain_offset, gain_map);
  }

  FakeVoEExternalMedia media;
  MockVoEVolumeControl volume;
  Agc* agc = new Agc;
  AudioProcessing* audioproc = AudioProcessing::Create();
  ASSERT_TRUE(audioproc != NULL);
  AgcManager manager(&media, &volume, agc, audioproc);

  int mic_level = 128;
  int last_mic_level = mic_level;
  EXPECT_CALL(volume, GetMicVolume(_))
      .WillRepeatedly(DoAll(SetArgReferee<0>(ByRef(mic_level)), Return(0)));
  EXPECT_CALL(volume, SetMicVolume(_))
      .WillRepeatedly(DoAll(SaveArg<0>(&mic_level), Return(0)));

  manager.Enable(true);
  ASSERT_EQ(0, agc->set_target_level_dbfs(FLAGS_level));
  const AudioProcessing::Error kNoErr = AudioProcessing::kNoError;
  GainControl* gctrl = audioproc->gain_control();
  ASSERT_EQ(kNoErr, gctrl->set_target_level_dbfs(FLAGS_cmp_level));
  ASSERT_EQ(kNoErr, gctrl->enable_limiter(FLAGS_limiter));

  AudioFrame frame;
  frame.num_channels_ = FLAGS_channels;
  frame.sample_rate_hz_ = FLAGS_rate;
  frame.samples_per_channel_ = FLAGS_rate / 100;
  const size_t frame_length = frame.samples_per_channel_ * FLAGS_channels;
  size_t sample_count = 0;
  while (fread(frame.data_, sizeof(int16_t), frame_length, in_file) ==
      frame_length) {
    SimulateMic(gain_map, mic_level, last_mic_level, &frame);
    last_mic_level = mic_level;
    media.CallProcess(kRecordingAllChannelsMixed, frame.data_,
                      frame.samples_per_channel_, FLAGS_rate, FLAGS_channels);
    ASSERT_EQ(frame_length,
              fwrite(frame.data_, sizeof(int16_t), frame_length, out_file));
    sample_count += frame_length;
    trace_to_stderr.SetTimeSeconds(static_cast<float>(sample_count) /
                                   FLAGS_channels / FLAGS_rate);
  }
  fclose(in_file);
  fclose(out_file);
  EXPECT_CALL(volume, Release());
}

}  // namespace
}  // namespace webrtc

int main(int argc, char* argv[]) {
  google::SetUsageMessage(webrtc::kUsage);
  google::ParseCommandLineFlags(&argc, &argv, true);
  webrtc::RunAgc();
  return 0;
}
