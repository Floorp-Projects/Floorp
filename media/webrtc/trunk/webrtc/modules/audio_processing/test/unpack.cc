/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Commandline tool to unpack audioproc debug files.
//
// The debug files are dumped as protobuf blobs. For analysis, it's necessary
// to unpack the file into its component parts: audio and other data.

#include <stdio.h>

#include "gflags/gflags.h"
#include "webrtc/audio_processing/debug.pb.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/typedefs.h"

using webrtc::scoped_array;

using webrtc::audioproc::Event;
using webrtc::audioproc::ReverseStream;
using webrtc::audioproc::Stream;
using webrtc::audioproc::Init;

// TODO(andrew): unpack more of the data.
DEFINE_string(input_file, "input.pcm", "The name of the input stream file.");
DEFINE_string(output_file, "ref_out.pcm",
              "The name of the reference output stream file.");
DEFINE_string(reverse_file, "reverse.pcm",
              "The name of the reverse input stream file.");
DEFINE_string(delay_file, "delay.int32", "The name of the delay file.");
DEFINE_string(drift_file, "drift.int32", "The name of the drift file.");
DEFINE_string(level_file, "level.int32", "The name of the level file.");
DEFINE_string(keypress_file, "keypress.bool", "The name of the keypress file.");
DEFINE_string(settings_file, "settings.txt", "The name of the settings file.");
DEFINE_bool(full, false,
            "Unpack the full set of files (normally not needed).");

// TODO(andrew): move this to a helper class to share with process_test.cc?
// Returns true on success, false on error or end-of-file.
bool ReadMessageFromFile(FILE* file,
                        ::google::protobuf::MessageLite* msg) {
  // The "wire format" for the size is little-endian.
  // Assume process_test is running on a little-endian machine.
  int32_t size = 0;
  if (fread(&size, sizeof(int32_t), 1, file) != 1) {
    return false;
  }
  if (size <= 0) {
    return false;
  }
  const size_t usize = static_cast<size_t>(size);

  scoped_array<char> array(new char[usize]);
  if (fread(array.get(), sizeof(char), usize, file) != usize) {
    return false;
  }

  msg->Clear();
  return msg->ParseFromArray(array.get(), usize);
}

int main(int argc, char* argv[]) {
  std::string program_name = argv[0];
  std::string usage = "Commandline tool to unpack audioproc debug files.\n"
    "Example usage:\n" + program_name + " debug_dump.pb\n";
  google::SetUsageMessage(usage);
  google::ParseCommandLineFlags(&argc, &argv, true);

  if (argc < 2) {
    printf("%s", google::ProgramUsage());
    return 1;
  }

  FILE* debug_file = fopen(argv[1], "rb");
  if (debug_file == NULL) {
    printf("Unable to open %s\n", argv[1]);
    return 1;
  }
  FILE* input_file = fopen(FLAGS_input_file.c_str(), "wb");
  if (input_file == NULL) {
    printf("Unable to open %s\n", FLAGS_input_file.c_str());
    return 1;
  }
  FILE* output_file = fopen(FLAGS_output_file.c_str(), "wb");
  if (output_file == NULL) {
    printf("Unable to open %s\n", FLAGS_output_file.c_str());
    return 1;
  }
  FILE* reverse_file = fopen(FLAGS_reverse_file.c_str(), "wb");
  if (reverse_file == NULL) {
    printf("Unable to open %s\n", FLAGS_reverse_file.c_str());
    return 1;
  }
  FILE* settings_file = fopen(FLAGS_settings_file.c_str(), "wb");
  if (settings_file == NULL) {
    printf("Unable to open %s\n", FLAGS_settings_file.c_str());
    return 1;
  }

  FILE* delay_file = NULL;
  FILE* drift_file = NULL;
  FILE* level_file = NULL;
  FILE* keypress_file = NULL;
  if (FLAGS_full) {
    delay_file = fopen(FLAGS_delay_file.c_str(), "wb");
    if (delay_file == NULL) {
      printf("Unable to open %s\n", FLAGS_delay_file.c_str());
      return 1;
    }
    drift_file = fopen(FLAGS_drift_file.c_str(), "wb");
    if (drift_file == NULL) {
      printf("Unable to open %s\n", FLAGS_drift_file.c_str());
      return 1;
    }
    level_file = fopen(FLAGS_level_file.c_str(), "wb");
    if (level_file == NULL) {
      printf("Unable to open %s\n", FLAGS_level_file.c_str());
      return 1;
    }
    keypress_file = fopen(FLAGS_keypress_file.c_str(), "wb");
    if (keypress_file == NULL) {
      printf("Unable to open %s\n", FLAGS_keypress_file.c_str());
      return 1;
    }
  }

  Event event_msg;
  int frame_count = 0;
  while (ReadMessageFromFile(debug_file, &event_msg)) {
    if (event_msg.type() == Event::REVERSE_STREAM) {
      if (!event_msg.has_reverse_stream()) {
        printf("Corrupted input file: ReverseStream missing.\n");
        return 1;
      }

      const ReverseStream msg = event_msg.reverse_stream();
      if (msg.has_data()) {
        if (fwrite(msg.data().data(), msg.data().size(), 1, reverse_file) !=
            1) {
          printf("Error when writing to %s\n", FLAGS_reverse_file.c_str());
          return 1;
        }
      }
    } else if (event_msg.type() == Event::STREAM) {
      frame_count++;
      if (!event_msg.has_stream()) {
        printf("Corrupted input file: Stream missing.\n");
        return 1;
      }

      const Stream msg = event_msg.stream();
      if (msg.has_input_data()) {
        if (fwrite(msg.input_data().data(), msg.input_data().size(), 1,
                   input_file) != 1) {
          printf("Error when writing to %s\n", FLAGS_input_file.c_str());
          return 1;
        }
      }

      if (msg.has_output_data()) {
        if (fwrite(msg.output_data().data(), msg.output_data().size(), 1,
                   output_file) != 1) {
          printf("Error when writing to %s\n", FLAGS_output_file.c_str());
          return 1;
        }
      }

      if (FLAGS_full) {
        if (msg.has_delay()) {
          int32_t delay = msg.delay();
          if (fwrite(&delay, sizeof(int32_t), 1, delay_file) != 1) {
            printf("Error when writing to %s\n", FLAGS_delay_file.c_str());
            return 1;
          }
        }

        if (msg.has_drift()) {
          int32_t drift = msg.drift();
          if (fwrite(&drift, sizeof(int32_t), 1, drift_file) != 1) {
            printf("Error when writing to %s\n", FLAGS_drift_file.c_str());
            return 1;
          }
        }

        if (msg.has_level()) {
          int32_t level = msg.level();
          if (fwrite(&level, sizeof(int32_t), 1, level_file) != 1) {
            printf("Error when writing to %s\n", FLAGS_level_file.c_str());
            return 1;
          }
        }

        if (msg.has_keypress()) {
          bool keypress = msg.keypress();
          if (fwrite(&keypress, sizeof(bool), 1, keypress_file) != 1) {
            printf("Error when writing to %s\n", FLAGS_keypress_file.c_str());
            return 1;
          }
        }
      }
    } else if (event_msg.type() == Event::INIT) {
      if (!event_msg.has_init()) {
        printf("Corrupted input file: Init missing.\n");
        return 1;
      }

      const Init msg = event_msg.init();
      // These should print out zeros if they're missing.
      fprintf(settings_file, "Init at frame: %d\n", frame_count);
      fprintf(settings_file, "  Sample rate: %d\n", msg.sample_rate());
      fprintf(settings_file, "  Device sample rate: %d\n",
              msg.device_sample_rate());
      fprintf(settings_file, "  Input channels: %d\n",
              msg.num_input_channels());
      fprintf(settings_file, "  Output channels: %d\n",
              msg.num_output_channels());
      fprintf(settings_file, "  Reverse channels: %d\n",
              msg.num_reverse_channels());

      fprintf(settings_file, "\n");
    }
  }

  return 0;
}
