/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "trace.h"
#include "critical_section_wrapper.h"
#include "audio_device_buffer.h"
#include "audio_device_utility.h"
#include "audio_device_config.h"

#include <stdlib.h>
#include <string.h>
#include <cassert>

#include "signal_processing_library.h"

namespace webrtc {

// ----------------------------------------------------------------------------
//  ctor
// ----------------------------------------------------------------------------

AudioDeviceBuffer::AudioDeviceBuffer() :
    _id(-1),
    _critSect(*CriticalSectionWrapper::CreateCriticalSection()),
    _critSectCb(*CriticalSectionWrapper::CreateCriticalSection()),
    _ptrCbAudioTransport(NULL),
    _recSampleRate(0),
    _playSampleRate(0),
    _recChannels(0),
    _playChannels(0),
    _recChannel(AudioDeviceModule::kChannelBoth),
    _recBytesPerSample(0),
    _playBytesPerSample(0),
    _recSamples(0),
    _recSize(0),
    _playSamples(0),
    _playSize(0),
    _recFile(*FileWrapper::Create()),
    _playFile(*FileWrapper::Create()),
    _currentMicLevel(0),
    _newMicLevel(0),
    _playDelayMS(0),
    _recDelayMS(0),
    _clockDrift(0),
    _measureDelay(false),    // should always be 'false' (EXPERIMENTAL)
    _pulseList(),
    _lastPulseTime(AudioDeviceUtility::GetTimeInMS())
{
    // valid ID will be set later by SetId, use -1 for now
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id, "%s created", __FUNCTION__);
    memset(_recBuffer, 0, kMaxBufferSizeBytes);
    memset(_playBuffer, 0, kMaxBufferSizeBytes);
}

// ----------------------------------------------------------------------------
//  dtor
// ----------------------------------------------------------------------------

AudioDeviceBuffer::~AudioDeviceBuffer()
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id, "%s destroyed", __FUNCTION__);
    {
        CriticalSectionScoped lock(&_critSect);

        _recFile.Flush();
        _recFile.CloseFile();
        delete &_recFile;

        _playFile.Flush();
        _playFile.CloseFile();
        delete &_playFile;

        _EmptyList();
    }

    delete &_critSect;
    delete &_critSectCb;
}

// ----------------------------------------------------------------------------
//  SetId
// ----------------------------------------------------------------------------

void AudioDeviceBuffer::SetId(WebRtc_UWord32 id)
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, id, "AudioDeviceBuffer::SetId(id=%d)", id);
    _id = id;
}

// ----------------------------------------------------------------------------
//  RegisterAudioCallback
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceBuffer::RegisterAudioCallback(AudioTransport* audioCallback)
{
    CriticalSectionScoped lock(&_critSectCb);
    _ptrCbAudioTransport = audioCallback;

    return 0;
}

// ----------------------------------------------------------------------------
//  InitPlayout
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceBuffer::InitPlayout()
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    CriticalSectionScoped lock(&_critSect);

    if (_measureDelay)
    {
        _EmptyList();
        _lastPulseTime = AudioDeviceUtility::GetTimeInMS();
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  InitRecording
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceBuffer::InitRecording()
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    CriticalSectionScoped lock(&_critSect);

    if (_measureDelay)
    {
        _EmptyList();
        _lastPulseTime = AudioDeviceUtility::GetTimeInMS();
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  SetRecordingSampleRate
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceBuffer::SetRecordingSampleRate(WebRtc_UWord32 fsHz)
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id, "AudioDeviceBuffer::SetRecordingSampleRate(fsHz=%u)", fsHz);

    CriticalSectionScoped lock(&_critSect);
    _recSampleRate = fsHz;
    return 0;
}

// ----------------------------------------------------------------------------
//  SetPlayoutSampleRate
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceBuffer::SetPlayoutSampleRate(WebRtc_UWord32 fsHz)
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id, "AudioDeviceBuffer::SetPlayoutSampleRate(fsHz=%u)", fsHz);

    CriticalSectionScoped lock(&_critSect);
    _playSampleRate = fsHz;
    return 0;
}

// ----------------------------------------------------------------------------
//  RecordingSampleRate
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceBuffer::RecordingSampleRate() const
{
    return _recSampleRate;
}

// ----------------------------------------------------------------------------
//  PlayoutSampleRate
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceBuffer::PlayoutSampleRate() const
{
    return _playSampleRate;
}

// ----------------------------------------------------------------------------
//  SetRecordingChannels
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceBuffer::SetRecordingChannels(WebRtc_UWord8 channels)
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id, "AudioDeviceBuffer::SetRecordingChannels(channels=%u)", channels);

    CriticalSectionScoped lock(&_critSect);
    _recChannels = channels;
    _recBytesPerSample = 2*channels;  // 16 bits per sample in mono, 32 bits in stereo
    return 0;
}

// ----------------------------------------------------------------------------
//  SetPlayoutChannels
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceBuffer::SetPlayoutChannels(WebRtc_UWord8 channels)
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id, "AudioDeviceBuffer::SetPlayoutChannels(channels=%u)", channels);

    CriticalSectionScoped lock(&_critSect);
    _playChannels = channels;
    // 16 bits per sample in mono, 32 bits in stereo
    _playBytesPerSample = 2*channels;
    return 0;
}

// ----------------------------------------------------------------------------
//  SetRecordingChannel
//
//  Select which channel to use while recording.
//  This API requires that stereo is enabled.
//
//  Note that, the nChannel parameter in RecordedDataIsAvailable will be
//  set to 2 even for kChannelLeft and kChannelRight. However, nBytesPerSample
//  will be 2 instead of 4 four these cases.
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceBuffer::SetRecordingChannel(const AudioDeviceModule::ChannelType channel)
{
    CriticalSectionScoped lock(&_critSect);

    if (_recChannels == 1)
    {
        return -1;
    }

    if (channel == AudioDeviceModule::kChannelBoth)
    {
        // two bytes per channel
        _recBytesPerSample = 4;
    }
    else
    {
        // only utilize one out of two possible channels (left or right)
        _recBytesPerSample = 2;
    }
    _recChannel = channel;

    return 0;
}

// ----------------------------------------------------------------------------
//  RecordingChannel
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceBuffer::RecordingChannel(AudioDeviceModule::ChannelType& channel) const
{
    channel = _recChannel;
    return 0;
}

// ----------------------------------------------------------------------------
//  RecordingChannels
// ----------------------------------------------------------------------------

WebRtc_UWord8 AudioDeviceBuffer::RecordingChannels() const
{
    return _recChannels;
}

// ----------------------------------------------------------------------------
//  PlayoutChannels
// ----------------------------------------------------------------------------

WebRtc_UWord8 AudioDeviceBuffer::PlayoutChannels() const
{
    return _playChannels;
}

// ----------------------------------------------------------------------------
//  SetCurrentMicLevel
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceBuffer::SetCurrentMicLevel(WebRtc_UWord32 level)
{
    _currentMicLevel = level;
    return 0;
}

// ----------------------------------------------------------------------------
//  NewMicLevel
// ----------------------------------------------------------------------------

WebRtc_UWord32 AudioDeviceBuffer::NewMicLevel() const
{
    return _newMicLevel;
}

// ----------------------------------------------------------------------------
//  SetVQEData
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceBuffer::SetVQEData(WebRtc_UWord32 playDelayMS, WebRtc_UWord32 recDelayMS, WebRtc_Word32 clockDrift)
{
    if ((playDelayMS + recDelayMS) > 300)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceUtility, _id, "too long delay (play:%i rec:%i)", playDelayMS, recDelayMS, clockDrift);
    }

    _playDelayMS = playDelayMS;
    _recDelayMS = recDelayMS;
    _clockDrift = clockDrift;

    return 0;
}

// ----------------------------------------------------------------------------
//  StartInputFileRecording
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceBuffer::StartInputFileRecording(
    const char fileName[kAdmMaxFileNameSize])
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    CriticalSectionScoped lock(&_critSect);

    _recFile.Flush();
    _recFile.CloseFile();

    return (_recFile.OpenFile(fileName, false, false, false));
}

// ----------------------------------------------------------------------------
//  StopInputFileRecording
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceBuffer::StopInputFileRecording()
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    CriticalSectionScoped lock(&_critSect);

    _recFile.Flush();
    _recFile.CloseFile();

    return 0;
}

// ----------------------------------------------------------------------------
//  StartOutputFileRecording
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceBuffer::StartOutputFileRecording(
    const char fileName[kAdmMaxFileNameSize])
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    CriticalSectionScoped lock(&_critSect);

    _playFile.Flush();
    _playFile.CloseFile();

    return (_playFile.OpenFile(fileName, false, false, false));
}

// ----------------------------------------------------------------------------
//  StopOutputFileRecording
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceBuffer::StopOutputFileRecording()
{
    WEBRTC_TRACE(kTraceMemory, kTraceAudioDevice, _id, "%s", __FUNCTION__);

    CriticalSectionScoped lock(&_critSect);

    _playFile.Flush();
    _playFile.CloseFile();

    return 0;
}

// ----------------------------------------------------------------------------
//  SetRecordedBuffer
//
//  Store recorded audio buffer in local memory ready for the actual
//  "delivery" using a callback.
//
//  This method can also parse out left or right channel from a stereo
//  input signal, i.e., emulate mono.
//
//  Examples:
//
//  16-bit,48kHz mono,  10ms => nSamples=480 => _recSize=2*480=960 bytes
//  16-bit,48kHz stereo,10ms => nSamples=480 => _recSize=4*480=1920 bytes
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceBuffer::SetRecordedBuffer(const void* audioBuffer,
                                                   WebRtc_UWord32 nSamples)
{
    CriticalSectionScoped lock(&_critSect);

    if (_recBytesPerSample == 0)
    {
        assert(false);
        return -1;
    }

    _recSamples = nSamples;
    _recSize = _recBytesPerSample*nSamples; // {2,4}*nSamples
    if (_recSize > kMaxBufferSizeBytes)
    {
        assert(false);
        return -1;
    }

    if (nSamples != _recSamples)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "invalid number of recorded samples (%d)", nSamples);
        return -1;
    }

    if (_recChannel == AudioDeviceModule::kChannelBoth)
    {
        // (default) copy the complete input buffer to the local buffer
        memcpy(&_recBuffer[0], audioBuffer, _recSize);
    }
    else
    {
        WebRtc_Word16* ptr16In = (WebRtc_Word16*)audioBuffer;
        WebRtc_Word16* ptr16Out = (WebRtc_Word16*)&_recBuffer[0];

        if (AudioDeviceModule::kChannelRight == _recChannel)
        {
            ptr16In++;
        }

        // exctract left or right channel from input buffer to the local buffer
        for (WebRtc_UWord32 i = 0; i < _recSamples; i++)
        {
            *ptr16Out = *ptr16In;
            ptr16Out++;
            ptr16In++;
            ptr16In++;
        }
    }

    if (_recFile.Open())
    {
        // write to binary file in mono or stereo (interleaved)
        _recFile.Write(&_recBuffer[0], _recSize);
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  DeliverRecordedData
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceBuffer::DeliverRecordedData()
{
    CriticalSectionScoped lock(&_critSectCb);

    // Ensure that user has initialized all essential members
    if ((_recSampleRate == 0)     ||
        (_recSamples == 0)        ||
        (_recBytesPerSample == 0) ||
        (_recChannels == 0))
    {
        assert(false);
        return -1;
    }

    if (_ptrCbAudioTransport == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "failed to deliver recorded data (AudioTransport does not exist)");
        return 0;
    }

    WebRtc_Word32 res(0);
    WebRtc_UWord32 newMicLevel(0);
    WebRtc_UWord32 totalDelayMS = _playDelayMS +_recDelayMS;

    if (_measureDelay)
    {
        CriticalSectionScoped lock(&_critSect);

        memset(&_recBuffer[0], 0, _recSize);
        WebRtc_UWord32 time = AudioDeviceUtility::GetTimeInMS();
        if (time - _lastPulseTime > 500)
        {
            _pulseList.PushBack(time);
            _lastPulseTime = time;

            WebRtc_Word16* ptr16 = (WebRtc_Word16*)&_recBuffer[0];
            *ptr16 = 30000;
        }
    }

    res = _ptrCbAudioTransport->RecordedDataIsAvailable(&_recBuffer[0],
                                                        _recSamples,
                                                        _recBytesPerSample,
                                                        _recChannels,
                                                        _recSampleRate,
                                                        totalDelayMS,
                                                        _clockDrift,
                                                        _currentMicLevel,
                                                        newMicLevel);
    if (res != -1)
    {
        _newMicLevel = newMicLevel;
    }

    return 0;
}

// ----------------------------------------------------------------------------
//  RequestPlayoutData
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceBuffer::RequestPlayoutData(WebRtc_UWord32 nSamples)
{
    {
        CriticalSectionScoped lock(&_critSect);

        // Ensure that user has initialized all essential members
        if ((_playBytesPerSample == 0) ||
            (_playChannels == 0)       ||
            (_playSampleRate == 0))
        {
            assert(false);
            return -1;
        }

        _playSamples = nSamples;
        _playSize = _playBytesPerSample * nSamples;  // {2,4}*nSamples
        if (_playSize > kMaxBufferSizeBytes)
        {
            assert(false);
            return -1;
        }

        if (nSamples != _playSamples)
        {
            WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "invalid number of samples to be played out (%d)", nSamples);
            return -1;
        }
    }

    WebRtc_UWord32 nSamplesOut(0);

    CriticalSectionScoped lock(&_critSectCb);

    if (_ptrCbAudioTransport == NULL)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceAudioDevice, _id, "failed to feed data to playout (AudioTransport does not exist)");
        return 0;
    }

    if (_ptrCbAudioTransport)
    {
        WebRtc_UWord32 res(0);

        res = _ptrCbAudioTransport->NeedMorePlayData(_playSamples,
                                                     _playBytesPerSample,
                                                     _playChannels,
                                                     _playSampleRate,
                                                     &_playBuffer[0],
                                                     nSamplesOut);
        if (res != 0)
        {
            WEBRTC_TRACE(kTraceError, kTraceAudioDevice, _id, "NeedMorePlayData() failed");
        }

        // --- Experimental delay-measurement implementation
        // *** not be used in released code ***

        if (_measureDelay)
        {
            CriticalSectionScoped lock(&_critSect);

            WebRtc_Word16 maxAbs = WebRtcSpl_MaxAbsValueW16((const WebRtc_Word16*)&_playBuffer[0], (WebRtc_Word16)nSamplesOut*_playChannels);
            if (maxAbs > 1000)
            {
                WebRtc_UWord32 nowTime = AudioDeviceUtility::GetTimeInMS();

                if (!_pulseList.Empty())
                {
                    ListItem* item = _pulseList.First();
                    if (item)
                    {
                        WebRtc_Word16 maxIndex = WebRtcSpl_MaxAbsIndexW16((const WebRtc_Word16*)&_playBuffer[0], (WebRtc_Word16)nSamplesOut*_playChannels);
                        WebRtc_UWord32 pulseTime = item->GetUnsignedItem();
                        WebRtc_UWord32 diff = nowTime - pulseTime + (10*maxIndex)/(nSamplesOut*_playChannels);
                        WEBRTC_TRACE(kTraceInfo, kTraceAudioDevice, _id, "diff time in playout delay (%d)", diff);
                    }
                    _pulseList.PopFront();
                }
            }
        }
    }

    return nSamplesOut;
}

// ----------------------------------------------------------------------------
//  GetPlayoutData
// ----------------------------------------------------------------------------

WebRtc_Word32 AudioDeviceBuffer::GetPlayoutData(void* audioBuffer)
{
    CriticalSectionScoped lock(&_critSect);

    if (_playSize > kMaxBufferSizeBytes)
    {
       WEBRTC_TRACE(kTraceError, kTraceUtility, _id, "_playSize %i exceeds "
       "kMaxBufferSizeBytes in AudioDeviceBuffer::GetPlayoutData", _playSize);
       assert(false);
       return -1;
    }

    memcpy(audioBuffer, &_playBuffer[0], _playSize);

    if (_playFile.Open())
    {
        // write to binary file in mono or stereo (interleaved)
        _playFile.Write(&_playBuffer[0], _playSize);
    }

    return _playSamples;
}

// ----------------------------------------------------------------------------
//  _EmptyList
// ----------------------------------------------------------------------------

void AudioDeviceBuffer::_EmptyList()
{
    while (!_pulseList.Empty())
    {
        ListItem* item = _pulseList.First();
        if (item)
        {
            // WebRtc_UWord32 ts = item->GetUnsignedItem();
        }
        _pulseList.PopFront();
    }
}

}  // namespace webrtc
