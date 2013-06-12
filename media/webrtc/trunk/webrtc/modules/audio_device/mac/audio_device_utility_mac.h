/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_UTILITY_MAC_H
#define WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_UTILITY_MAC_H

#include "audio_device_utility.h"
#include "audio_device.h"

namespace webrtc
{
class CriticalSectionWrapper;

class AudioDeviceUtilityMac: public AudioDeviceUtility
{
public:
    AudioDeviceUtilityMac(const int32_t id);
    ~AudioDeviceUtilityMac();

    virtual int32_t Init();

private:
    CriticalSectionWrapper& _critSect;
    int32_t _id;
};

} //  namespace webrtc

#endif  // MODULES_AUDIO_DEVICE_MAIN_SOURCE_MAC_AUDIO_DEVICE_UTILITY_MAC_H_
