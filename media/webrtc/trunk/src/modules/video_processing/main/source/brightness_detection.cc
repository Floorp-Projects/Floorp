/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_processing.h"
#include "brightness_detection.h"
#include "trace.h"

#include <math.h>

namespace webrtc {

VPMBrightnessDetection::VPMBrightnessDetection() :
    _id(0)
{
    Reset();
}

VPMBrightnessDetection::~VPMBrightnessDetection()
{
}

WebRtc_Word32
VPMBrightnessDetection::ChangeUniqueId(const WebRtc_Word32 id)
{
    _id = id;
    return VPM_OK;
}

void
VPMBrightnessDetection::Reset()
{
    _frameCntBright = 0;
    _frameCntDark = 0;
}

WebRtc_Word32
VPMBrightnessDetection::ProcessFrame(const WebRtc_UWord8* frame,
                                     const WebRtc_UWord32 width,
                                     const WebRtc_UWord32 height,
                                     const VideoProcessingModule::FrameStats& stats)
{
    if (frame == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoPreocessing, _id, "Null frame pointer");
        return VPM_PARAMETER_ERROR;
    }
    
    if (width == 0 || height == 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoPreocessing, _id, "Invalid frame size");
        return VPM_PARAMETER_ERROR;
    }

    if (!VideoProcessingModule::ValidFrameStats(stats))
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoPreocessing, _id, "Invalid frame stats");
        return VPM_PARAMETER_ERROR;
    }

    const WebRtc_UWord8 frameCntAlarm = 2;

    // Get proportion in lowest bins 
    WebRtc_UWord8 lowTh = 20;
    float propLow = 0;
    for (WebRtc_UWord32 i = 0; i < lowTh; i++)
    {
        propLow += stats.hist[i];
    }
    propLow /= stats.numPixels;

    // Get proportion in highest bins 
    unsigned char highTh = 230;
    float propHigh = 0;
    for (WebRtc_UWord32 i = highTh; i < 256; i++)
    {
        propHigh += stats.hist[i];
    }
    propHigh /= stats.numPixels;

    if(propHigh < 0.4)
    {
        if (stats.mean < 90 || stats.mean > 170)
        {
            // Standard deviation of Y
            float stdY = 0;
            for (WebRtc_UWord32 h = 0; h < height; h += (1 << stats.subSamplHeight))
            {
                WebRtc_UWord32 row = h*width;
                for (WebRtc_UWord32 w = 0; w < width; w += (1 << stats.subSamplWidth))
                {
                    stdY += (frame[w + row] - stats.mean) * (frame[w + row] - stats.mean);
                }
            }           
            stdY = sqrt(stdY / stats.numPixels);

            // Get percentiles
            WebRtc_UWord32 sum = 0;
            WebRtc_UWord32 medianY = 140;
            WebRtc_UWord32 perc05 = 0;
            WebRtc_UWord32 perc95 = 255;
            float posPerc05 = stats.numPixels * 0.05f;
            float posMedian = stats.numPixels * 0.5f;
            float posPerc95 = stats.numPixels * 0.95f;
            for (WebRtc_UWord32 i = 0; i < 256; i++)
            {
                sum += stats.hist[i];

                if (sum < posPerc05)
                {
                    perc05 = i;     // 5th perc
                }
                if (sum < posMedian)
                {
                    medianY = i;    // 50th perc
                }
                if (sum < posPerc95)
                {
                    perc95 = i;     // 95th perc
                }
                else
                {
                    break;
                }
            }

            // Check if image is too dark
            if ((stdY < 55) && (perc05 < 50))
            { 
                if (medianY < 60 || stats.mean < 80 ||  perc95 < 130 || propLow > 0.20)
                {
                    _frameCntDark++;
                }
                else
                {
                    _frameCntDark = 0;
                }
            } 
            else
            {
                _frameCntDark = 0;
            }

            // Check if image is too bright
            if ((stdY < 52) && (perc95 > 200) && (medianY > 160))
            {
                if (medianY > 185 || stats.mean > 185 || perc05 > 140 || propHigh > 0.25)
                {
                    _frameCntBright++;  
                }
                else 
                {
                    _frameCntBright = 0;
                }
            } 
            else
            {
                _frameCntBright = 0;
            }

        } 
        else
        {
            _frameCntDark = 0;
            _frameCntBright = 0;
        }

    } 
    else
    {
        _frameCntBright++;
        _frameCntDark = 0;
    }
    
    if (_frameCntDark > frameCntAlarm)
    {
        return VideoProcessingModule::kDarkWarning;
    }
    else if (_frameCntBright > frameCntAlarm)
    {
        return VideoProcessingModule::kBrightWarning;
    }
    else
    {
        return VideoProcessingModule::kNoWarning;
    }
}

} //namespace
