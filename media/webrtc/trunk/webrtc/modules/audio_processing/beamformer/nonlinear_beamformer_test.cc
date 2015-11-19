/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <iostream>
#include <vector>

#include "gflags/gflags.h"
#include "webrtc/modules/audio_processing/beamformer/nonlinear_beamformer.h"
#include "webrtc/modules/audio_processing/beamformer/pcm_utils.h"

DEFINE_int32(sample_rate,
             48000,
             "The sample rate of the input file. The output"
             "file will be of the same sample rate.");
DEFINE_int32(num_input_channels,
             2,
             "The number of channels in the input file.");
DEFINE_double(mic_spacing,
              0.05,
              "The spacing between microphones on the chromebook which "
              "recorded the input file.");
DEFINE_string(input_file_path,
              "input.wav",
              "The absolute path to the input file.");
DEFINE_string(output_file_path,
              "beamformer_test_output.wav",
              "The absolute path to the output file.");

using webrtc::ChannelBuffer;

int main(int argc, char* argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);

  const float kChunkTimeMilliseconds = 10;
  const int kChunkSize = FLAGS_sample_rate / (1000.f / kChunkTimeMilliseconds);
  const int kInputSamplesPerChunk = kChunkSize * FLAGS_num_input_channels;

  ChannelBuffer<float> captured_audio_cb(kChunkSize, FLAGS_num_input_channels);

  FILE* read_file = fopen(FLAGS_input_file_path.c_str(), "rb");
  if (!read_file) {
    std::cerr << "Input file '" << FLAGS_input_file_path << "' not found."
              << std::endl;
    return -1;
  }

  // Skipping the .wav header. TODO: Add .wav header parsing.
  fseek(read_file, 44, SEEK_SET);

  FILE* write_file = fopen(FLAGS_output_file_path.c_str(), "wb");

  std::vector<webrtc::Point> array_geometry;
  for (int i = 0; i < FLAGS_num_input_channels; ++i) {
    array_geometry.push_back(webrtc::Point(i * FLAGS_mic_spacing, 0.f, 0.f));
  }
  webrtc::NonlinearBeamformer bf(array_geometry);
  bf.Initialize(kChunkTimeMilliseconds, FLAGS_sample_rate);
  while (true) {
    size_t samples_read = webrtc::PcmReadToFloat(read_file,
                                                 kInputSamplesPerChunk,
                                                 FLAGS_num_input_channels,
                                                 captured_audio_cb.channels());

    if (static_cast<int>(samples_read) != kInputSamplesPerChunk) {
      break;
    }

    bf.ProcessChunk(captured_audio_cb, &captured_audio_cb);
    webrtc::PcmWriteFromFloat(
        write_file, kChunkSize, 1, captured_audio_cb.channels());
  }
  fclose(read_file);
  fclose(write_file);
  return 0;
}
