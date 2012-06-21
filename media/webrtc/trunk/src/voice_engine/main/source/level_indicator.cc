/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "level_indicator.h"
#include "module_common_types.h"
#include "signal_processing_library.h"

namespace webrtc {

namespace voe {


// Number of bars on the indicator.
// Note that the number of elements is specified because we are indexing it
// in the range of 0-32
const WebRtc_Word8 permutation[33] =
    {0,1,2,3,4,4,5,5,5,5,6,6,6,6,6,7,7,7,7,8,8,8,9,9,9,9,9,9,9,9,9,9,9};


AudioLevel::AudioLevel() :
    _absMax(0),
    _count(0),
    _currentLevel(0),
    _currentLevelFullRange(0)
{
}

AudioLevel::~AudioLevel()
{
}

void
AudioLevel::Clear()
{
    _absMax = 0;
    _count = 0;
    _currentLevel = 0;
    _currentLevelFullRange = 0;
}

void
AudioLevel::ComputeLevel(const AudioFrame& audioFrame)
{
    WebRtc_Word16 absValue(0);

    // Check speech level (works for 2 channels as well)
    absValue = WebRtcSpl_MaxAbsValueW16(
        audioFrame._payloadData,
        audioFrame._payloadDataLengthInSamples*audioFrame._audioChannel);
    if (absValue > _absMax)
    _absMax = absValue;

    // Update level approximately 10 times per second
    if (_count++ == kUpdateFrequency)
    {
        _currentLevelFullRange = _absMax;

        _count = 0;

        // Highest value for a WebRtc_Word16 is 0x7fff = 32767
        // Divide with 1000 to get in the range of 0-32 which is the range of
        // the permutation vector
        WebRtc_Word32 position = _absMax/1000;

        // Make it less likely that the bar stays at position 0. I.e. only if
        // its in the range 0-250 (instead of 0-1000)
        if ((position == 0) && (_absMax > 250))
        {
            position = 1;
        }
        _currentLevel = permutation[position];

        // Decay the absolute maximum (divide by 4)
        _absMax >>= 2;
    }
}

WebRtc_Word8
AudioLevel::Level() const
{
    return _currentLevel;
}

WebRtc_Word16
AudioLevel::LevelFullRange() const
{
    return _currentLevelFullRange;
}

}  // namespace voe

}  //  namespace webrtc
