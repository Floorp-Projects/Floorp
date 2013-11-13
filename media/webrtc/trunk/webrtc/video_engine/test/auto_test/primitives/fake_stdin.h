/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef FAKE_STDIN_H_
#define FAKE_STDIN_H_

#include <stdio.h>

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace webrtc {

// Creates a fake stdin-like FILE* for unit test usage.
FILE* FakeStdin(const std::string& input);

}  // namespace webrtc

#endif  // FAKE_STDIN_H_
