/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_BUFFER_H
#define WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_BUFFER_H

#include "webrtc/modules/audio_device/include/audio_device.h"
#include "webrtc/system_wrappers/interface/file_wrapper.h"
#include "webrtc/typedefs.h"

namespace webrtc {
class CriticalSectionWrapper;

const uint32_t kPulsePeriodMs = 1000;
const uint32_t kMaxBufferSizeBytes = 3840; // 10ms in stereo @ 96kHz

class AudioDeviceObserver;
class MediaFile;

class AudioDeviceBuffer
{
public:
    AudioDeviceBuffer();
    virtual ~AudioDeviceBuffer();

    void SetId(uint32_t id);
    int32_t RegisterAudioCallback(AudioTransport* audioCallback);

    int32_t InitPlayout();
    int32_t InitRecording();

    virtual int32_t SetRecordingSampleRate(uint32_t fsHz);
    virtual int32_t SetPlayoutSampleRate(uint32_t fsHz);
    int32_t RecordingSampleRate() const;
    int32_t PlayoutSampleRate() const;

    virtual int32_t SetRecordingChannels(uint8_t channels);
    virtual int32_t SetPlayoutChannels(uint8_t channels);
    uint8_t RecordingChannels() const;
    uint8_t PlayoutChannels() const;
    int32_t SetRecordingChannel(
        const AudioDeviceModule::ChannelType channel);
    int32_t RecordingChannel(
        AudioDeviceModule::ChannelType& channel) const;

    virtual int32_t SetRecordedBuffer(const void* audioBuffer,
                                      uint32_t nSamples);
    int32_t SetCurrentMicLevel(uint32_t level);
    virtual void SetVQEData(int playDelayMS,
                            int recDelayMS,
                            int clockDrift);
    virtual int32_t DeliverRecordedData();
    uint32_t NewMicLevel() const;

    virtual int32_t RequestPlayoutData(uint32_t nSamples);
    virtual int32_t GetPlayoutData(void* audioBuffer);

    int32_t StartInputFileRecording(
        const char fileName[kAdmMaxFileNameSize]);
    int32_t StopInputFileRecording();
    int32_t StartOutputFileRecording(
        const char fileName[kAdmMaxFileNameSize]);
    int32_t StopOutputFileRecording();

    int32_t SetTypingStatus(bool typingStatus);

private:
    int32_t                   _id;
    CriticalSectionWrapper&         _critSect;
    CriticalSectionWrapper&         _critSectCb;

    AudioTransport*                 _ptrCbAudioTransport;

    uint32_t                  _recSampleRate;
    uint32_t                  _playSampleRate;

    uint8_t                   _recChannels;
    uint8_t                   _playChannels;

    // selected recording channel (left/right/both)
    AudioDeviceModule::ChannelType _recChannel;

    // 2 or 4 depending on mono or stereo
    uint8_t                   _recBytesPerSample;
    uint8_t                   _playBytesPerSample;

    // 10ms in stereo @ 96kHz
    int8_t                          _recBuffer[kMaxBufferSizeBytes];

    // one sample <=> 2 or 4 bytes
    uint32_t                  _recSamples;
    uint32_t                  _recSize;           // in bytes

    // 10ms in stereo @ 96kHz
    int8_t                          _playBuffer[kMaxBufferSizeBytes];

    // one sample <=> 2 or 4 bytes
    uint32_t                  _playSamples;
    uint32_t                  _playSize;          // in bytes

    FileWrapper&                    _recFile;
    FileWrapper&                    _playFile;

    uint32_t                  _currentMicLevel;
    uint32_t                  _newMicLevel;

    bool                      _typingStatus;

    int _playDelayMS;
    int _recDelayMS;
    int _clockDrift;
    int high_delay_counter_;
};

}  // namespace webrtc

#endif  // WEBRTC_AUDIO_DEVICE_AUDIO_DEVICE_BUFFER_H
