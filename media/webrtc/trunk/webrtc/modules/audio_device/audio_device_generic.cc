/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_device/audio_device_generic.h"
#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {

int32_t AudioDeviceGeneric::SetRecordingSampleRate(
    const uint32_t samplesPerSec)
{
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, -1,
        "Set recording sample rate not supported on this platform");
    return -1;
}

int32_t AudioDeviceGeneric::SetPlayoutSampleRate(
    const uint32_t samplesPerSec)
{
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, -1,
        "Set playout sample rate not supported on this platform");
    return -1;
}

int32_t AudioDeviceGeneric::SetLoudspeakerStatus(bool enable)
{
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, -1,
        "Set loudspeaker status not supported on this platform");
    return -1;
}

int32_t AudioDeviceGeneric::GetLoudspeakerStatus(bool& enable) const
{
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, -1,
        "Get loudspeaker status not supported on this platform");
    return -1;
}

int32_t AudioDeviceGeneric::ResetAudioDevice()
{
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, -1,
        "Reset audio device not supported on this platform");
    return -1;
}

int32_t AudioDeviceGeneric::SoundDeviceControl(unsigned int par1,
    unsigned int par2, unsigned int par3, unsigned int par4)
{
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, -1,
        "Sound device control not supported on this platform");
    return -1;
}

bool AudioDeviceGeneric::BuiltInAECIsAvailable() const {
  WEBRTC_TRACE(kTraceError, kTraceAudioDevice, -1,
      "Built-in AEC not supported on this platform");
  return false;
}

int32_t AudioDeviceGeneric::EnableBuiltInAEC(bool enable)
{
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, -1,
        "Built-in AEC not supported on this platform");
    return -1;
}

bool AudioDeviceGeneric::BuiltInAECIsEnabled() const
{
    WEBRTC_TRACE(kTraceError, kTraceAudioDevice, -1,
        "Windows AEC not supported on this platform");
    return false;
}

}  // namespace webrtc
