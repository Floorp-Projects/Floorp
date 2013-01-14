/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/test/auto_test/primitives/fake_stdin.h"

namespace webrtc {

FILE* FakeStdin(const std::string& input) {
  FILE* fake_stdin = tmpfile();

  EXPECT_EQ(input.size(),
      fwrite(input.c_str(), sizeof(char), input.size(), fake_stdin));
  rewind(fake_stdin);

  return fake_stdin;
}

}  // namespace webrtc
