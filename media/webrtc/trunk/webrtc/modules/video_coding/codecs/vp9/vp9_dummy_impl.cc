/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

// This file contains an implementation of empty webrtc VP9 encoder/decoder
// factories so it is possible to build webrtc without linking with vp9.
#include "webrtc/modules/video_coding/codecs/vp9/vp9_impl.h"

namespace webrtc {
VP9Encoder* VP9Encoder::Create() { return nullptr; }
VP9Decoder* VP9Decoder::Create() { return nullptr; }
}
