/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

#include "gflags/gflags.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/format_macros.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/common_audio/channel_buffer.h"
#include "webrtc/common_audio/wav_file.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/audio_processing/test/audio_file_processor.h"
#include "webrtc/modules/audio_processing/test/protobuf_utils.h"
#include "webrtc/modules/audio_processing/test/test_utils.h"
#include "webrtc/system_wrappers/include/tick_util.h"
#include "webrtc/test/testsupport/trace_to_stderr.h"

namespace {

bool ValidateOutChannels(const char* flagname, int32_t value) {
  return value >= 0;
}

}  // namespace

DEFINE_string(dump, "", "Name of the aecdump debug file to read from.");
DEFINE_string(i, "", "Name of the capture input stream file to read from.");
DEFINE_string(
    o,
    "out.wav",
    "Name of the output file to write the processed capture stream to.");
DEFINE_int32(out_channels, 1, "Number of output channels.");
const bool out_channels_dummy =
    google::RegisterFlagValidator(&FLAGS_out_channels, &ValidateOutChannels);
DEFINE_int32(out_sample_rate, 48000, "Output sample rate in Hz.");
DEFINE_string(mic_positions, "",
    "Space delimited cartesian coordinates of microphones in meters. "
    "The coordinates of each point are contiguous. "
    "For a two element array: \"x1 y1 z1 x2 y2 z2\"");
DEFINE_double(
    target_angle_degrees,
    90,
    "The azimuth of the target in degrees. Only applies to beamforming.");

DEFINE_bool(aec, false, "Enable echo cancellation.");
DEFINE_bool(agc, false, "Enable automatic gain control.");
DEFINE_bool(hpf, false, "Enable high-pass filtering.");
DEFINE_bool(ns, false, "Enable noise suppression.");
DEFINE_bool(ts, false, "Enable transient suppression.");
DEFINE_bool(bf, false, "Enable beamforming.");
DEFINE_bool(ie, false, "Enable intelligibility enhancer.");
DEFINE_bool(all, false, "Enable all components.");

DEFINE_int32(ns_level, -1, "Noise suppression level [0 - 3].");

DEFINE_bool(perf, false, "Enable performance tests.");

namespace webrtc {
namespace {

const int kChunksPerSecond = 100;
const char kUsage[] =
    "Command-line tool to run audio processing on WAV files. Accepts either\n"
    "an input capture WAV file or protobuf debug dump and writes to an output\n"
    "WAV file.\n"
    "\n"
    "All components are disabled by default. If any bi-directional components\n"
    "are enabled, only debug dump files are permitted.";

}  // namespace

int main(int argc, char* argv[]) {
  google::SetUsageMessage(kUsage);
  google::ParseCommandLineFlags(&argc, &argv, true);

  if (!((FLAGS_i.empty()) ^ (FLAGS_dump.empty()))) {
    fprintf(stderr,
            "An input file must be specified with either -i or -dump.\n");
    return 1;
  }
  if (FLAGS_dump.empty() && (FLAGS_aec || FLAGS_ie)) {
    fprintf(stderr, "-aec and -ie require a -dump file.\n");
    return 1;
  }
  if (FLAGS_ie) {
    fprintf(stderr,
            "FIXME(ajm): The intelligibility enhancer output is not dumped.\n");
    return 1;
  }

  test::TraceToStderr trace_to_stderr(true);
  Config config;
  if (FLAGS_bf || FLAGS_all) {
    if (FLAGS_mic_positions.empty()) {
      fprintf(stderr, "-mic_positions must be specified when -bf is used.\n");
      return 1;
    }
    config.Set<Beamforming>(new Beamforming(
        true, ParseArrayGeometry(FLAGS_mic_positions),
        SphericalPointf(DegreesToRadians(FLAGS_target_angle_degrees), 0.f,
                        1.f)));
  }
  config.Set<ExperimentalNs>(new ExperimentalNs(FLAGS_ts || FLAGS_all));
  config.Set<Intelligibility>(new Intelligibility(FLAGS_ie || FLAGS_all));

  rtc::scoped_ptr<AudioProcessing> ap(AudioProcessing::Create(config));
  RTC_CHECK_EQ(kNoErr, ap->echo_cancellation()->Enable(FLAGS_aec || FLAGS_all));
  RTC_CHECK_EQ(kNoErr, ap->gain_control()->Enable(FLAGS_agc || FLAGS_all));
  RTC_CHECK_EQ(kNoErr, ap->high_pass_filter()->Enable(FLAGS_hpf || FLAGS_all));
  RTC_CHECK_EQ(kNoErr, ap->noise_suppression()->Enable(FLAGS_ns || FLAGS_all));
  if (FLAGS_ns_level != -1) {
    RTC_CHECK_EQ(kNoErr,
                 ap->noise_suppression()->set_level(
                     static_cast<NoiseSuppression::Level>(FLAGS_ns_level)));
  }
  ap->set_stream_key_pressed(FLAGS_ts);

  rtc::scoped_ptr<AudioFileProcessor> processor;
  auto out_file = rtc_make_scoped_ptr(new WavWriter(
      FLAGS_o, FLAGS_out_sample_rate, static_cast<size_t>(FLAGS_out_channels)));
  std::cout << FLAGS_o << ": " << out_file->FormatAsString() << std::endl;
  if (FLAGS_dump.empty()) {
    auto in_file = rtc_make_scoped_ptr(new WavReader(FLAGS_i));
    std::cout << FLAGS_i << ": " << in_file->FormatAsString() << std::endl;
    processor.reset(new WavFileProcessor(std::move(ap), std::move(in_file),
                                         std::move(out_file)));

  } else {
    processor.reset(new AecDumpFileProcessor(
        std::move(ap), fopen(FLAGS_dump.c_str(), "rb"), std::move(out_file)));
  }

  int num_chunks = 0;
  while (processor->ProcessChunk()) {
    trace_to_stderr.SetTimeSeconds(num_chunks * 1.f / kChunksPerSecond);
    ++num_chunks;
  }

  if (FLAGS_perf) {
    const auto& proc_time = processor->proc_time();
    int64_t exec_time_us = proc_time.sum.Microseconds();
    printf(
        "\nExecution time: %.3f s, File time: %.2f s\n"
        "Time per chunk (mean, max, min):\n%.0f us, %.0f us, %.0f us\n",
        exec_time_us * 1e-6, num_chunks * 1.f / kChunksPerSecond,
        exec_time_us * 1.f / num_chunks, 1.f * proc_time.max.Microseconds(),
        1.f * proc_time.min.Microseconds());
  }

  return 0;
}

}  // namespace webrtc

int main(int argc, char* argv[]) {
  return webrtc::main(argc, argv);
}
