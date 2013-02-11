/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_processing/main/source/brighten.h"

#include <cstdlib>

#include "system_wrappers/interface/trace.h"

namespace webrtc {
namespace VideoProcessing {

WebRtc_Word32 Brighten(I420VideoFrame* frame, int delta) {
  assert(frame);
  if (frame->IsZeroSize()) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoPreocessing, -1,
                 "zero size frame");
    return VPM_PARAMETER_ERROR;
  }

  if (frame->width() <= 0 || frame->height() <= 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoPreocessing, -1,
                 "Invalid frame size");
    return VPM_PARAMETER_ERROR;
  }

  int numPixels = frame->width() * frame->height();

  int lookUp[256];
  for (int i = 0; i < 256; i++) {
    int val = i + delta;
    lookUp[i] = ((((val < 0) ? 0 : val) > 255) ? 255 : val);
  }

  WebRtc_UWord8* tempPtr = frame->buffer(kYPlane);

  for (int i = 0; i < numPixels; i++) {
    *tempPtr = static_cast<WebRtc_UWord8>(lookUp[*tempPtr]);
    tempPtr++;
  }
  return VPM_OK;
}

}  // namespace VideoProcessing
}  // namespace webrtc
