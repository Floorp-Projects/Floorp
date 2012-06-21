/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "color_enhancement.h"
#include "color_enhancement_private.h"
#include "trace.h"
#include <cstdlib>  // NULL

namespace webrtc {

namespace VideoProcessing
{ 
    WebRtc_Word32
    ColorEnhancement(WebRtc_UWord8* frame,
                     const WebRtc_UWord32 width,
                     const WebRtc_UWord32 height)
    {
        // pointers to U and V color pixels
        WebRtc_UWord8* ptrU;
        WebRtc_UWord8* ptrV;
        WebRtc_UWord8 tempChroma;
        const WebRtc_UWord32 numPixels = width * height;


        if (frame == NULL)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoPreocessing, -1, "Null frame pointer");
            return VPM_GENERAL_ERROR;
        }

        if (width == 0 || height == 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoPreocessing, -1, "Invalid frame size");
            return VPM_GENERAL_ERROR;
        }
        
        // set pointers to first U and V pixels
        
        // stream format:
        // | numPixels bytes luminance | numPixels/4 bytes chroma U | numPixels/4 chroma V |
        
        ptrU = frame + numPixels;       // skip luminance
        ptrV = ptrU + (numPixels>>2);

        // loop through all chrominance pixels and modify color
        for (WebRtc_UWord32 ix = 0; ix < (numPixels>>2); ix++)
        {
            tempChroma = colorTable[*ptrU][*ptrV];
            *ptrV = colorTable[*ptrV][*ptrU];
            *ptrU = tempChroma;
            
            // increment pointers
            ptrU++;
            ptrV++;
        }
        return VPM_OK;
    }

} //namespace

} //namespace webrtc
