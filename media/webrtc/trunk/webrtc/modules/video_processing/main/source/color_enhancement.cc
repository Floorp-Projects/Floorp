/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdlib.h>  // NULL

#include "webrtc/modules/video_processing/main/source/color_enhancement.h"
#include "webrtc/modules/video_processing/main/source/color_enhancement_private.h"

namespace webrtc {
namespace VideoProcessing {

int32_t ColorEnhancement(I420VideoFrame* frame) {
  assert(frame);
  // Pointers to U and V color pixels.
  uint8_t* ptr_u;
  uint8_t* ptr_v;
  uint8_t temp_chroma;
  if (frame->IsZeroSize()) {
    return VPM_GENERAL_ERROR;
  }
  if (frame->width() == 0 || frame->height() == 0) {
    return VPM_GENERAL_ERROR;
  }

  // Set pointers to first U and V pixels (skip luminance).
  ptr_u = frame->buffer(kUPlane);
  ptr_v = frame->buffer(kVPlane);
  int size_uv = ((frame->width() + 1) / 2) * ((frame->height() + 1) / 2);

  // Loop through all chrominance pixels and modify color.
  for (int ix = 0; ix < size_uv; ix++) {
    temp_chroma = colorTable[*ptr_u][*ptr_v];
    *ptr_v = colorTable[*ptr_v][*ptr_u];
    *ptr_u = temp_chroma;

    ptr_u++;
    ptr_v++;
  }
  return VPM_OK;
}

}  // namespace VideoProcessing
}  // namespace webrtc
