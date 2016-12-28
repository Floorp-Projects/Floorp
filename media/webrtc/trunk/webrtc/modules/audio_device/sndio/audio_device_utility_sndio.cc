/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_device/sndio/audio_device_utility_sndio.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/system_wrappers/include/trace.h"

namespace webrtc
{

AudioDeviceUtilitySndio::AudioDeviceUtilitySndio(const int32_t id) :
    _critSect(*CriticalSectionWrapper::CreateCriticalSection()), _id(id)
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, id,
                 "%s created", __FUNCTION__);
}

AudioDeviceUtilitySndio::~AudioDeviceUtilitySndio()
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id,
                 "%s destroyed", __FUNCTION__);
    {
        CriticalSectionScoped lock(&_critSect);

        // XXX free stuff here...
    }

    delete &_critSect;
}

// ============================================================================
//                                     API
// ============================================================================


int32_t AudioDeviceUtilitySndio::Init()
{

    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id,
                 "  OS info: %s", "POSIX using sndio");

    return 0;
}


}  // namespace webrtc
