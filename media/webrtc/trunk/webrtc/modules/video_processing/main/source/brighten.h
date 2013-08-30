/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_PROCESSING_MAIN_SOURCE_BRIGHTEN_H_
#define MODULES_VIDEO_PROCESSING_MAIN_SOURCE_BRIGHTEN_H_

#include "webrtc/modules/video_processing/main/interface/video_processing.h"
#include "webrtc/typedefs.h"

namespace webrtc {
namespace VideoProcessing {

int32_t Brighten(I420VideoFrame* frame, int delta);

}  // namespace VideoProcessing
}  // namespace webrtc

#endif  // MODULES_VIDEO_PROCESSING_MAIN_SOURCE_BRIGHTEN_H_
