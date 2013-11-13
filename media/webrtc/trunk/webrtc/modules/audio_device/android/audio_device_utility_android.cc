/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 *  Android audio device utility implementation
 */

#include "webrtc/modules/audio_device/android/audio_device_utility_android.h"

#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc
{

AudioDeviceUtilityAndroid::AudioDeviceUtilityAndroid(const int32_t id) :
    _critSect(*CriticalSectionWrapper::CreateCriticalSection()), _id(id)
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, id,
                 "%s created", __FUNCTION__);
}

AudioDeviceUtilityAndroid::~AudioDeviceUtilityAndroid()
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id,
                 "%s destroyed", __FUNCTION__);
    {
        CriticalSectionScoped lock(&_critSect);
    }

    delete &_critSect;
}

int32_t AudioDeviceUtilityAndroid::Init()
{

    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id,
                 "  OS info: %s", "Android");

    return 0;
}

}  // namespace webrtc
