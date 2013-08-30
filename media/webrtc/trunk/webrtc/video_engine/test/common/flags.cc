/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/test/common/flags.h"

#include <assert.h>

#include "gflags/gflags.h"

namespace webrtc {
namespace test {
namespace flags {

bool flags_are_init = false;

void Init(int* argc, char ***argv) {
  assert(!flags_are_init);
  google::ParseCommandLineFlags(argc, argv, true);

//  stuff;
  flags_are_init = true;
}

DEFINE_int32(width, 640, "Video width.");
size_t Width() {
  assert(flags_are_init);
  return static_cast<size_t>(FLAGS_width);
}

DEFINE_int32(height, 480, "Video height.");
size_t Height() {
  assert(flags_are_init);
  return static_cast<size_t>(FLAGS_height);
}

DEFINE_int32(fps, 30, "Frames per second.");
int Fps() {
  assert(flags_are_init);
  return static_cast<int>(FLAGS_fps);
}

DEFINE_int32(min_bitrate, 50, "Minimum video bitrate.");
size_t MinBitrate() {
  assert(flags_are_init);
  return static_cast<size_t>(FLAGS_min_bitrate);
}

DEFINE_int32(start_bitrate, 300, "Video starting bitrate.");
size_t StartBitrate() {
  assert(flags_are_init);
  return static_cast<size_t>(FLAGS_start_bitrate);
}

DEFINE_int32(max_bitrate, 800, "Maximum video bitrate.");
size_t MaxBitrate() {
  assert(flags_are_init);
  return static_cast<size_t>(FLAGS_max_bitrate);
}
}  // flags
}  // test
}  // webrtc
