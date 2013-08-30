/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_TEST_COMMON_FLAGS_H_
#define WEBRTC_VIDEO_ENGINE_TEST_COMMON_FLAGS_H_

#include <stddef.h>

namespace webrtc {
namespace test {
namespace flags {

void Init(int* argc, char ***argv);

size_t Width();
size_t Height();
int Fps();
size_t MinBitrate();
size_t StartBitrate();
size_t MaxBitrate();
}  // flags
}  // test
}  // webrtc

#endif  // WEBRTC_VIDEO_ENGINE_TEST_COMMON_FLAGS_H_
