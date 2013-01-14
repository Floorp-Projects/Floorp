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
 * deflickering.h
 */

#ifndef VPM_DEFLICKERING_H
#define VPM_DEFLICKERING_H

#include "typedefs.h"
#include "video_processing.h"

#include <cstring>  // NULL

namespace webrtc {

class VPMDeflickering
{
public:
    VPMDeflickering();
    ~VPMDeflickering();

    WebRtc_Word32 ChangeUniqueId(WebRtc_Word32 id);

    void Reset();

    WebRtc_Word32 ProcessFrame(I420VideoFrame* frame,
                               VideoProcessingModule::FrameStats* stats);
private:
    WebRtc_Word32 PreDetection(WebRtc_UWord32 timestamp,
                               const VideoProcessingModule::FrameStats& stats);

    WebRtc_Word32 DetectFlicker();

    enum { kMeanBufferLength = 32 };
    enum { kFrameHistorySize = 15 };
    enum { kNumProbs = 12 };
    enum { kNumQuants = kNumProbs + 2 };
    enum { kMaxOnlyLength = 5 };

    WebRtc_Word32 _id;

    WebRtc_UWord32  _meanBufferLength;
    WebRtc_UWord8   _detectionState;    // 0: No flickering
                                      // 1: Flickering detected
                                      // 2: In flickering
    WebRtc_Word32    _meanBuffer[kMeanBufferLength];
    WebRtc_UWord32   _timestampBuffer[kMeanBufferLength];
    WebRtc_UWord32   _frameRate;
    static const WebRtc_UWord16 _probUW16[kNumProbs];
    static const WebRtc_UWord16 _weightUW16[kNumQuants - kMaxOnlyLength];
    WebRtc_UWord8 _quantHistUW8[kFrameHistorySize][kNumQuants];
};

} //namespace

#endif // VPM_DEFLICKERING_H

