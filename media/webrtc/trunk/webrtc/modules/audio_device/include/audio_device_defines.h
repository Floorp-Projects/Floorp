/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_DEFINES_H
#define WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_DEFINES_H

#include "typedefs.h"

namespace webrtc {

static const int kAdmMaxDeviceNameSize = 128;
static const int kAdmMaxFileNameSize = 512;
static const int kAdmMaxGuidSize = 128;

static const int kAdmMinPlayoutBufferSizeMs = 10;
static const int kAdmMaxPlayoutBufferSizeMs = 250;

// ----------------------------------------------------------------------------
//  AudioDeviceObserver
// ----------------------------------------------------------------------------

class AudioDeviceObserver
{
public:
    enum ErrorCode
    {
        kRecordingError = 0,
        kPlayoutError = 1
    };
    enum WarningCode
    {
        kRecordingWarning = 0,
        kPlayoutWarning = 1
    };

    virtual void OnErrorIsReported(const ErrorCode error) = 0;
    virtual void OnWarningIsReported(const WarningCode warning) = 0;

protected:
    virtual ~AudioDeviceObserver() {}
};

// ----------------------------------------------------------------------------
//  AudioTransport
// ----------------------------------------------------------------------------

class AudioTransport
{
public:
    virtual int32_t RecordedDataIsAvailable(const void* audioSamples,
                                            const uint32_t nSamples,
                                            const uint8_t nBytesPerSample,
                                            const uint8_t nChannels,
                                            const uint32_t samplesPerSec,
                                            const uint32_t totalDelayMS,
                                            const int32_t clockDrift,
                                            const uint32_t currentMicLevel,
                                            const bool keyPressed,
                                            uint32_t& newMicLevel) = 0;   

    virtual int32_t NeedMorePlayData(const uint32_t nSamples,
                                     const uint8_t nBytesPerSample,
                                     const uint8_t nChannels,
                                     const uint32_t samplesPerSec,
                                     void* audioSamples,
                                     uint32_t& nSamplesOut) = 0;

protected:
    virtual ~AudioTransport() {}
};

}  // namespace webrtc

#endif  // WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_DEFINES_H
