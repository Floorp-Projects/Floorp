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
#include <sstream>
#include <string>

#include "gflags/gflags.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/common_audio/channel_buffer.h"
#include "webrtc/common_audio/wav_file.h"
#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/modules/audio_processing/test/test_utils.h"

DEFINE_string(dump, "", "The name of the debug dump file to read from.");
DEFINE_string(c, "", "The name of the capture input file to read from.");
DEFINE_string(o, "out.wav", "Name of the capture output file to write to.");
DEFINE_int32(o_channels, 0, "Number of output channels. Defaults to input.");
DEFINE_int32(o_sample_rate, 0, "Output sample rate in Hz. Defaults to input.");
DEFINE_double(mic_spacing, 0.0,
    "Alternate way to specify mic_positions. "
    "Assumes uniform linear array with specified spacings.");
DEFINE_string(mic_positions, "",
    "Space delimited cartesian coordinates of microphones in meters. "
    "The coordinates of each point are contiguous. "
    "For a two element array: \"x1 y1 z1 x2 y2 z2\"");

DEFINE_bool(aec, false, "Enable echo cancellation.");
DEFINE_bool(agc, false, "Enable automatic gain control.");
DEFINE_bool(hpf, false, "Enable high-pass filtering.");
DEFINE_bool(ns, false, "Enable noise suppression.");
DEFINE_bool(ts, false, "Enable transient suppression.");
DEFINE_bool(bf, false, "Enable beamforming.");
DEFINE_bool(all, false, "Enable all components.");

DEFINE_int32(ns_level, -1, "Noise suppression level [0 - 3].");

static const int kChunksPerSecond = 100;
static const char kUsage[] =
    "Command-line tool to run audio processing on WAV files. Accepts either\n"
    "an input capture WAV file or protobuf debug dump and writes to an output\n"
    "WAV file.\n"
    "\n"
    "All components are disabled by default. If any bi-directional components\n"
    "are enabled, only debug dump files are permitted.";

namespace webrtc {

namespace {

// Returns a vector<T> parsed from whitespace delimited values in to_parse,
// or an empty vector if the string could not be parsed.
template<typename T>
std::vector<T> parse_list(std::string to_parse) {
  std::vector<T> values;

  std::istringstream str(to_parse);
  std::copy(
      std::istream_iterator<T>(str),
      std::istream_iterator<T>(),
      std::back_inserter(values));

  return values;
}

// Parses the array geometry from the command line.
//
// If a vector with size != num_mics is returned, an error has occurred and an
// appropriate error message has been printed to stdout.
std::vector<Point> get_array_geometry(size_t num_mics) {
  std::vector<Point> result;
  result.reserve(num_mics);

  if (FLAGS_mic_positions.length()) {
    CHECK(FLAGS_mic_spacing == 0.0 &&
        "mic_positions and mic_spacing should not both be specified");

    const std::vector<float> values = parse_list<float>(FLAGS_mic_positions);
    if (values.size() != 3 * num_mics) {
      fprintf(stderr,
          "Could not parse mic_positions or incorrect number of points.\n");
    } else {
      for (size_t i = 0; i < values.size(); i += 3) {
        double x = values[i + 0];
        double y = values[i + 1];
        double z = values[i + 2];
        result.push_back(Point(x, y, z));
      }
    }
  } else {
    if (FLAGS_mic_spacing <= 0) {
      fprintf(stderr,
          "mic_spacing must a positive value when beamforming is enabled.\n");
    } else {
      for (size_t i = 0; i < num_mics; ++i) {
        result.push_back(Point(i * FLAGS_mic_spacing, 0.f, 0.f));
      }
    }
  }

  return result;
}

}  // namespace

int main(int argc, char* argv[]) {
  {
    const std::string program_name = argv[0];
    const std::string usage = kUsage;
    google::SetUsageMessage(usage);
  }
  google::ParseCommandLineFlags(&argc, &argv, true);

  if (!((FLAGS_c == "") ^ (FLAGS_dump == ""))) {
    fprintf(stderr,
            "An input file must be specified with either -c or -dump.\n");
    return 1;
  }
  if (FLAGS_dump != "") {
    fprintf(stderr, "FIXME: the -dump option is not yet implemented.\n");
    return 1;
  }

  WavReader c_file(FLAGS_c);
  // If the output format is uninitialized, use the input format.
  int o_channels = FLAGS_o_channels;
  if (!o_channels)
    o_channels = c_file.num_channels();
  int o_sample_rate = FLAGS_o_sample_rate;
  if (!o_sample_rate)
    o_sample_rate = c_file.sample_rate();
  WavWriter o_file(FLAGS_o, o_sample_rate, o_channels);

  Config config;
  config.Set<ExperimentalNs>(new ExperimentalNs(FLAGS_ts || FLAGS_all));

  if (FLAGS_bf || FLAGS_all) {
    const size_t num_mics = c_file.num_channels();
    const std::vector<Point> array_geometry = get_array_geometry(num_mics);
    if (array_geometry.size() != num_mics) {
      return 1;
    }

    config.Set<Beamforming>(new Beamforming(true, array_geometry));
  }

  rtc::scoped_ptr<AudioProcessing> ap(AudioProcessing::Create(config));
  if (FLAGS_dump != "") {
    CHECK_EQ(kNoErr, ap->echo_cancellation()->Enable(FLAGS_aec || FLAGS_all));
  } else if (FLAGS_aec) {
    fprintf(stderr, "-aec requires a -dump file.\n");
    return -1;
  }
  CHECK_EQ(kNoErr, ap->gain_control()->Enable(FLAGS_agc || FLAGS_all));
  CHECK_EQ(kNoErr, ap->gain_control()->set_mode(GainControl::kFixedDigital));
  CHECK_EQ(kNoErr, ap->high_pass_filter()->Enable(FLAGS_hpf || FLAGS_all));
  CHECK_EQ(kNoErr, ap->noise_suppression()->Enable(FLAGS_ns || FLAGS_all));
  if (FLAGS_ns_level != -1)
    CHECK_EQ(kNoErr, ap->noise_suppression()->set_level(
        static_cast<NoiseSuppression::Level>(FLAGS_ns_level)));

  printf("Input file: %s\nChannels: %d, Sample rate: %d Hz\n\n",
         FLAGS_c.c_str(), c_file.num_channels(), c_file.sample_rate());
  printf("Output file: %s\nChannels: %d, Sample rate: %d Hz\n\n",
         FLAGS_o.c_str(), o_file.num_channels(), o_file.sample_rate());

  ChannelBuffer<float> c_buf(c_file.sample_rate() / kChunksPerSecond,
                             c_file.num_channels());
  ChannelBuffer<float> o_buf(o_file.sample_rate() / kChunksPerSecond,
                             o_file.num_channels());

  const size_t c_length =
      static_cast<size_t>(c_buf.num_channels() * c_buf.num_frames());
  const size_t o_length =
      static_cast<size_t>(o_buf.num_channels() * o_buf.num_frames());
  rtc::scoped_ptr<float[]> c_interleaved(new float[c_length]);
  rtc::scoped_ptr<float[]> o_interleaved(new float[o_length]);
  while (c_file.ReadSamples(c_length, c_interleaved.get()) == c_length) {
    FloatS16ToFloat(c_interleaved.get(), c_length, c_interleaved.get());
    Deinterleave(c_interleaved.get(), c_buf.num_frames(),
                 c_buf.num_channels(), c_buf.channels());

    CHECK_EQ(kNoErr,
        ap->ProcessStream(c_buf.channels(),
                          c_buf.num_frames(),
                          c_file.sample_rate(),
                          LayoutFromChannels(c_buf.num_channels()),
                          o_file.sample_rate(),
                          LayoutFromChannels(o_buf.num_channels()),
                          o_buf.channels()));

    Interleave(o_buf.channels(), o_buf.num_frames(),
               o_buf.num_channels(), o_interleaved.get());
    FloatToFloatS16(o_interleaved.get(), o_length, o_interleaved.get());
    o_file.WriteSamples(o_interleaved.get(), o_length);
  }

  return 0;
}

}  // namespace webrtc

int main(int argc, char* argv[]) {
  return webrtc::main(argc, argv);
}
