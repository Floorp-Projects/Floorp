/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_decimator.h"
#include "tick_util.h"
#include "video_processing.h"

#define VD_MIN(a, b) ((a) < (b)) ? (a) : (b)

namespace webrtc {

VPMVideoDecimator::VPMVideoDecimator()
:
_overShootModifier(0),
_dropCount(0),
_keepCount(0),
_targetFrameRate(30),
_incomingFrameRate(0.0f),
_maxFrameRate(30),
_incomingFrameTimes(),
_enableTemporalDecimation(true)
{
    Reset();
}

VPMVideoDecimator::~VPMVideoDecimator()
{
	//
}

void
VPMVideoDecimator::Reset() 
{
   _overShootModifier = 0;
    _dropCount = 0;
    _keepCount = 0;
    _targetFrameRate = 30;
    _incomingFrameRate = 0.0f;
    _maxFrameRate = 30;
    memset(_incomingFrameTimes, 0, sizeof(_incomingFrameTimes));
    _enableTemporalDecimation = true;
}

void
VPMVideoDecimator::EnableTemporalDecimation(bool enable)
{
    _enableTemporalDecimation = enable;
}
WebRtc_Word32
VPMVideoDecimator::SetMaxFrameRate(WebRtc_UWord32 maxFrameRate)
{
    if (maxFrameRate == 0)
    {
        return VPM_PARAMETER_ERROR;
    }

    _maxFrameRate = maxFrameRate;
    
    if (_targetFrameRate > _maxFrameRate)
    {
        _targetFrameRate = _maxFrameRate;

    }
    return VPM_OK;
}

WebRtc_Word32
VPMVideoDecimator::SetTargetFrameRate(WebRtc_UWord32 frameRate)
{
    if (frameRate == 0)
    {
        return VPM_PARAMETER_ERROR;
    }
    if (frameRate > _maxFrameRate)
    {
        //override
        _targetFrameRate = _maxFrameRate;
    }
    else
    {
        _targetFrameRate = frameRate;
    }
    return VPM_OK;
}

bool
VPMVideoDecimator::DropFrame()
{
    if (!_enableTemporalDecimation)
    {
        return false;
    }

    if (_incomingFrameRate <= 0)
    {
        return false;
    }

    const WebRtc_UWord32 incomingFrameRate = static_cast<WebRtc_UWord32>(_incomingFrameRate + 0.5f);

    if (_targetFrameRate == 0)
    {
        return true;
    }
    
    bool drop = false; 
    if (incomingFrameRate > _targetFrameRate)
    {       
        WebRtc_Word32 overshoot = _overShootModifier + (incomingFrameRate - _targetFrameRate);
        if(overshoot < 0)
        {
            overshoot = 0;
            _overShootModifier = 0;
        }
        
        if (overshoot && 2 * overshoot < (WebRtc_Word32) incomingFrameRate)
        {

            if (_dropCount) // Just got here so drop to be sure.
            {
                _dropCount = 0;         
                return true;
            }                        
            const WebRtc_UWord32 dropVar = incomingFrameRate / overshoot;

            if (_keepCount >= dropVar)
            {
                drop = true;                           
                _overShootModifier = -((WebRtc_Word32) incomingFrameRate % overshoot) / 3;
                _keepCount = 1;
            }
            else
            {                        
                
                _keepCount++;
            }
        }
        else
        {
            _keepCount = 0;         
            const WebRtc_UWord32 dropVar = overshoot / _targetFrameRate;
            if (_dropCount < dropVar)
            {                
                drop = true;
                _dropCount++;                
            }
            else
            {
                _overShootModifier = overshoot % _targetFrameRate;
                drop = false;
                _dropCount = 0;                
            }
        }
    }

    return drop;
}


WebRtc_UWord32
VPMVideoDecimator::DecimatedFrameRate()
{
    ProcessIncomingFrameRate(TickTime::MillisecondTimestamp());
    if (!_enableTemporalDecimation)
    {
        return static_cast<WebRtc_UWord32>(_incomingFrameRate + 0.5f);
    }
    return VD_MIN(_targetFrameRate, static_cast<WebRtc_UWord32>(_incomingFrameRate + 0.5f));
}

WebRtc_UWord32
VPMVideoDecimator::InputFrameRate()
{
    ProcessIncomingFrameRate(TickTime::MillisecondTimestamp());
    return static_cast<WebRtc_UWord32>(_incomingFrameRate + 0.5f);
}

void
VPMVideoDecimator::UpdateIncomingFrameRate()
{
   WebRtc_Word64 now = TickTime::MillisecondTimestamp();
    if(_incomingFrameTimes[0] == 0)
    {
        // first no shift
    } else
    {
        // shift 
        for(int i = (kFrameCountHistorySize - 2); i >= 0 ; i--)
        {
            _incomingFrameTimes[i+1] = _incomingFrameTimes[i];
        }
    }
    _incomingFrameTimes[0] = now;
    ProcessIncomingFrameRate(now);
}

void 
VPMVideoDecimator::ProcessIncomingFrameRate(WebRtc_Word64 now)
{
   WebRtc_Word32 num = 0;
    WebRtc_Word32 nrOfFrames = 0;
    for(num = 1; num < (kFrameCountHistorySize - 1); num++)
    {
        if (_incomingFrameTimes[num] <= 0 ||
            now - _incomingFrameTimes[num] > kFrameHistoryWindowMs) // don't use data older than 2sec
        {
            break;
        } else
        {
            nrOfFrames++;
        }
    }
    if (num > 1)
    {
        WebRtc_Word64 diff = now - _incomingFrameTimes[num-1];
        _incomingFrameRate = 1.0;
        if(diff >0)
        {
            _incomingFrameRate = nrOfFrames * 1000.0f / static_cast<float>(diff);
        }
    }
    else
    {
        _incomingFrameRate = static_cast<float>(nrOfFrames);
    }
}

} //namespace
