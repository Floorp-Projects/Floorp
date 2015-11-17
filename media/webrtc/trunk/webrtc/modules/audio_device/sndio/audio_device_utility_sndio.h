/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_UTILITY_SNDIO_H
#define WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_UTILITY_SNDIO_H

#include "webrtc/modules/audio_device/audio_device_utility.h"
#include "webrtc/modules/audio_device/include/audio_device.h"

namespace webrtc
{
class CriticalSectionWrapper;

class AudioDeviceUtilitySndio: public AudioDeviceUtility
{
public:
    AudioDeviceUtilitySndio(const int32_t id);
    virtual ~AudioDeviceUtilitySndio();

    virtual int32_t Init() OVERRIDE;

private:
    CriticalSectionWrapper& _critSect;
    int32_t _id;
};

}  // namespace webrtc

#endif
