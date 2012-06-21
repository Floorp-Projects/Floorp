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
 * denoising.h
 */
#ifndef VPM_DENOISING_H
#define VPM_DENOISING_H

#include "typedefs.h"
#include "video_processing.h"

namespace webrtc {

class VPMDenoising
{
public:
    VPMDenoising();
    ~VPMDenoising();

    WebRtc_Word32 ChangeUniqueId(WebRtc_Word32 id);

    void Reset();

    WebRtc_Word32 ProcessFrame(WebRtc_UWord8* frame,
                             WebRtc_UWord32 width,
                             WebRtc_UWord32 height);

private:
    WebRtc_Word32 _id;

    WebRtc_UWord32*   _moment1;           // (Q8) First order moment (mean)
    WebRtc_UWord32*   _moment2;           // (Q8) Second order moment
    WebRtc_UWord32    _frameSize;         // Size (# of pixels) of frame
    WebRtc_Word32     _denoiseFrameCnt;   // Counter for subsampling in time
};

} //namespace

#endif // VPM_DENOISING_H
  
