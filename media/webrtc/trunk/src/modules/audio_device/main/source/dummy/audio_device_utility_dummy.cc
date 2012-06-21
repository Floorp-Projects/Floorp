/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio_device_utility_dummy.h"
#include "audio_device_config.h" // DEBUG_PRINT()
#include "critical_section_wrapper.h"
#include "trace.h"

namespace webrtc
{

AudioDeviceUtilityDummy::AudioDeviceUtilityDummy(const WebRtc_Word32 id) :
    _critSect(*CriticalSectionWrapper::CreateCriticalSection()),
    _id(id),
    _lastError(AudioDeviceModule::kAdmErrNone)
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, id,
                 "%s created", __FUNCTION__);
}

AudioDeviceUtilityDummy::~AudioDeviceUtilityDummy()
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id,
                 "%s destroyed", __FUNCTION__);
    {
        CriticalSectionScoped lock(&_critSect);

        // free stuff here...
    }

    delete &_critSect;
}

// ============================================================================
//                                     API
// ============================================================================


WebRtc_Word32 AudioDeviceUtilityDummy::Init()
{

    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioDevice, _id,
                 "  OS info: %s", "Dummy");

    return 0;
}


} // namespace webrtc
