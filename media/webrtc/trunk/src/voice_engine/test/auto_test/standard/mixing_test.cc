/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#include <string>

#include "after_initialization_fixture.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace {

const int16_t kLimiterHeadroom = 29204;  // == -1 dbFS
const int16_t kInt16Max = 0x7fff;
const int kSampleRateHz = 16000;
const int kTestDurationMs = 3000;
const int kSkipOutputMs = 500;

}  // namespace

class MixingTest : public AfterInitializationFixture {
 protected:
  MixingTest()
    : input_filename_(test::OutputPath() + "mixing_test_input.pcm"),
      output_filename_(test::OutputPath() + "mixing_test_output.pcm") {
  }

  // Creates and mixes |num_remote_streams| which play a file "as microphone"
  // with |num_local_streams| which play a file "locally", using a constant
  // amplitude of |input_value|. The local streams manifest as "anonymous"
  // mixing participants, meaning they will be mixed regardless of the number
  // of participants. (A stream is a VoiceEngine "channel").
  //
  // The mixed output is verified to always fall between |max_output_value| and
  // |min_output_value|, after a startup phase.
  //
  // |num_remote_streams_using_mono| of the remote streams use mono, with the
  // remainder using stereo.
  void RunMixingTest(int num_remote_streams,
                     int num_local_streams,
                     int num_remote_streams_using_mono,
                     int16_t input_value,
                     int16_t max_output_value,
                     int16_t min_output_value) {
    ASSERT_LE(num_remote_streams_using_mono, num_remote_streams);

    GenerateInputFile(input_value);

    std::vector<int> local_streams(num_local_streams);
    for (size_t i = 0; i < local_streams.size(); ++i) {
      local_streams[i] = voe_base_->CreateChannel();
      EXPECT_NE(-1, local_streams[i]);
    }
    StartLocalStreams(local_streams);
    TEST_LOG("Playing %d local streams.\n", num_local_streams);

    std::vector<int> remote_streams(num_remote_streams);
    for (size_t i = 0; i < remote_streams.size(); ++i) {
      remote_streams[i] = voe_base_->CreateChannel();
      EXPECT_NE(-1, remote_streams[i]);
    }
    StartRemoteStreams(remote_streams, num_remote_streams_using_mono);
    TEST_LOG("Playing %d remote streams.\n", num_remote_streams);

    // Start recording the mixed output and wait.
    EXPECT_EQ(0, voe_file_->StartRecordingPlayout(-1 /* record meeting */,
        output_filename_.c_str()));
    Sleep(kTestDurationMs);
    EXPECT_EQ(0, voe_file_->StopRecordingPlayout(-1));

    StopLocalStreams(local_streams);
    StopRemoteStreams(remote_streams);

    VerifyMixedOutput(max_output_value, min_output_value);
  }

 private:
  // Generate input file with constant values equal to |input_value|. The file
  // will be one second longer than the duration of the test.
  void GenerateInputFile(int16_t input_value) {
    FILE* input_file = fopen(input_filename_.c_str(), "wb");
    ASSERT_TRUE(input_file != NULL);
    for (int i = 0; i < kSampleRateHz / 1000 * (kTestDurationMs + 1000); i++) {
      ASSERT_EQ(1u, fwrite(&input_value, sizeof(input_value), 1, input_file));
    }
    ASSERT_EQ(0, fclose(input_file));
  }

  void VerifyMixedOutput(int16_t max_output_value, int16_t min_output_value) {
    // Verify the mixed output.
    FILE* output_file = fopen(output_filename_.c_str(), "rb");
    ASSERT_TRUE(output_file != NULL);
    int16_t output_value = 0;
    // Skip the first segment to avoid initialization and ramping-in effects.
    EXPECT_EQ(0, fseek(output_file, sizeof(output_value) *
                       kSampleRateHz / 1000 * kSkipOutputMs, SEEK_SET));
    int samples_read = 0;
    while (fread(&output_value, sizeof(output_value), 1, output_file) == 1) {
      samples_read++;
      std::ostringstream trace_stream;
      trace_stream << samples_read << " samples read";
      SCOPED_TRACE(trace_stream.str());
      EXPECT_LE(output_value, max_output_value);
      EXPECT_GE(output_value, min_output_value);
    }
    // Ensure the recording length is close to the duration of the test.
    ASSERT_GE((samples_read * 1000.0) / kSampleRateHz,
              0.9 * (kTestDurationMs - kSkipOutputMs));
    // Ensure we read the entire file.
    ASSERT_NE(0, feof(output_file));
    ASSERT_EQ(0, fclose(output_file));
  }

  // Start up local streams ("anonymous" participants).
  void StartLocalStreams(const std::vector<int>& streams) {
    for (size_t i = 0; i < streams.size(); ++i) {
      EXPECT_EQ(0, voe_base_->StartPlayout(streams[i]));
      EXPECT_EQ(0, voe_file_->StartPlayingFileLocally(streams[i],
          input_filename_.c_str(), true));
    }
  }

  void StopLocalStreams(const std::vector<int>& streams) {
    for (size_t i = 0; i < streams.size(); ++i) {
      EXPECT_EQ(0, voe_base_->StopPlayout(streams[i]));
      EXPECT_EQ(0, voe_base_->DeleteChannel(streams[i]));
    }
  }

  // Start up remote streams ("normal" participants).
  void StartRemoteStreams(const std::vector<int>& streams,
                          int num_remote_streams_using_mono) {
    // Use L16 at 16kHz to minimize distortion (file recording is 16kHz and
    // resampling will cause distortion).
    CodecInst codec_inst;
    strcpy(codec_inst.plname, "L16");
    codec_inst.channels = 1;
    codec_inst.plfreq = kSampleRateHz;
    codec_inst.pltype = 105;
    codec_inst.pacsize = codec_inst.plfreq / 100;
    codec_inst.rate = codec_inst.plfreq * sizeof(int16_t) * 8;  // 8 bits/byte.

    for (int i = 0; i < num_remote_streams_using_mono; ++i) {
      StartRemoteStream(streams[i], codec_inst, 1234 + 2 * i);
    }

    // The remainder of the streams will use stereo.
    codec_inst.channels = 2;
    codec_inst.pltype++;
    for (size_t i = num_remote_streams_using_mono; i < streams.size(); ++i) {
      StartRemoteStream(streams[i], codec_inst, 1234 + 2 * i);
    }
  }

  // Start up a single remote stream.
  void StartRemoteStream(int stream, const CodecInst& codec_inst, int port) {
    EXPECT_EQ(0, voe_codec_->SetRecPayloadType(stream, codec_inst));
    EXPECT_EQ(0, voe_base_->SetLocalReceiver(stream, port));
    EXPECT_EQ(0, voe_base_->SetSendDestination(stream, port, "127.0.0.1"));
    EXPECT_EQ(0, voe_base_->StartReceive(stream));
    EXPECT_EQ(0, voe_base_->StartPlayout(stream));
    EXPECT_EQ(0, voe_codec_->SetSendCodec(stream, codec_inst));
    EXPECT_EQ(0, voe_base_->StartSend(stream));
    EXPECT_EQ(0, voe_file_->StartPlayingFileAsMicrophone(stream,
        input_filename_.c_str(), true));
  }

  void StopRemoteStreams(const std::vector<int>& streams) {
    for (size_t i = 0; i < streams.size(); ++i) {
      EXPECT_EQ(0, voe_base_->StopSend(streams[i]));
      EXPECT_EQ(0, voe_base_->StopPlayout(streams[i]));
      EXPECT_EQ(0, voe_base_->StopReceive(streams[i]));
      EXPECT_EQ(0, voe_base_->DeleteChannel(streams[i]));
    }
  }

  const std::string input_filename_;
  const std::string output_filename_;
};

// These tests assume a maximum of three mixed participants. We typically allow
// a +/- 10% range around the expected output level to account for distortion
// from coding and processing in the loopback chain.
TEST_F(MixingTest, FourChannelsWithOnlyThreeMixed) {
  const int16_t kInputValue = 1000;
  const int16_t kExpectedOutput = kInputValue * 3;
  RunMixingTest(4, 0, 4, kInputValue, 1.1 * kExpectedOutput,
                0.9 * kExpectedOutput);
}

// Ensure the mixing saturation protection is working. We can do this because
// the mixing limiter is given some headroom, so the expected output is less
// than full scale.
TEST_F(MixingTest, VerifySaturationProtection) {
  const int16_t kInputValue = 20000;
  const int16_t kExpectedOutput = kLimiterHeadroom;
  // If this isn't satisfied, we're not testing anything.
  ASSERT_GT(kInputValue * 3, kInt16Max);
  ASSERT_LT(1.1 * kExpectedOutput, kInt16Max);
  RunMixingTest(3, 0, 3, kInputValue, 1.1 * kExpectedOutput,
               0.9 * kExpectedOutput);
}

TEST_F(MixingTest, SaturationProtectionHasNoEffectOnOneChannel) {
  const int16_t kInputValue = kInt16Max;
  const int16_t kExpectedOutput = kInt16Max;
  // If this isn't satisfied, we're not testing anything.
  ASSERT_GT(0.95 * kExpectedOutput, kLimiterHeadroom);
  // Tighter constraints are required here to properly test this.
  RunMixingTest(1, 0, 1, kInputValue, kExpectedOutput,
                0.95 * kExpectedOutput);
}

TEST_F(MixingTest, VerifyAnonymousAndNormalParticipantMixing) {
  const int16_t kInputValue = 1000;
  const int16_t kExpectedOutput = kInputValue * 2;
  RunMixingTest(1, 1, 1, kInputValue, 1.1 * kExpectedOutput,
                0.9 * kExpectedOutput);
}

TEST_F(MixingTest, AnonymousParticipantsAreAlwaysMixed) {
  const int16_t kInputValue = 1000;
  const int16_t kExpectedOutput = kInputValue * 4;
  RunMixingTest(3, 1, 3, kInputValue, 1.1 * kExpectedOutput,
                0.9 * kExpectedOutput);
}

TEST_F(MixingTest, VerifyStereoAndMonoMixing) {
  const int16_t kInputValue = 1000;
  const int16_t kExpectedOutput = kInputValue * 2;
  RunMixingTest(2, 0, 1, kInputValue, 1.1 * kExpectedOutput,
                0.9 * kExpectedOutput);
}

}  // namespace webrtc
