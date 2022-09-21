/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_SIMULCAST_STREAM_H_
#define API_VIDEO_CODECS_SIMULCAST_STREAM_H_

#include "api/video_codecs/spatial_layer.h"

namespace webrtc {

// TODO(bugs.webrtc.org/6883): Unify with struct VideoStream, part of
// VideoEncoderConfig.
// TODO(bugs.webrtc.org/11607): Make this a separate type, rather than an alias.
using SimulcastStream = SpatialLayer;

}  // namespace webrtc
#endif  // API_VIDEO_CODECS_SIMULCAST_STREAM_H_
