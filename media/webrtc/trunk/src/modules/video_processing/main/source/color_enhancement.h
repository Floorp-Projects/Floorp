/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * color_enhancement.h
 */
#ifndef VPM_COLOR_ENHANCEMENT_H
#define VPM_COLOR_ENHANCEMENT_H

#include "typedefs.h"
#include "video_processing.h"

namespace webrtc {

namespace VideoProcessing
{
    WebRtc_Word32 ColorEnhancement(WebRtc_UWord8* frame,
                                 WebRtc_UWord32 width,
                                 WebRtc_UWord32 height);
}

} //namespace

#endif // VPM_COLOR_ENHANCEMENT_H
