/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "shared_data.h"

#include "audio_processing.h"
#include "critical_section_wrapper.h"
#include "channel.h"
#include "output_mixer.h"
#include "trace.h"
#include "transmit_mixer.h"

namespace webrtc {

namespace voe {

static int32_t _gInstanceCounter = 0;

SharedData::SharedData() :
    _instanceId(++_gInstanceCounter),
    _apiCritPtr(CriticalSectionWrapper::CreateCriticalSection()),
    _channelManager(_gInstanceCounter),
    _engineStatistics(_gInstanceCounter),
    _audioDevicePtr(NULL),
    audioproc_(NULL),
    _moduleProcessThreadPtr(ProcessThread::CreateProcessThread()),
    _externalRecording(false),
    _externalPlayout(false)
{
    Trace::CreateTrace();
    if (OutputMixer::Create(_outputMixerPtr, _gInstanceCounter) == 0)
    {
        _outputMixerPtr->SetEngineInformation(_engineStatistics);
    }
    if (TransmitMixer::Create(_transmitMixerPtr, _gInstanceCounter) == 0)
    {
        _transmitMixerPtr->SetEngineInformation(*_moduleProcessThreadPtr,
                                                _engineStatistics,
                                                _channelManager);
    }
    _audioDeviceLayer = AudioDeviceModule::kPlatformDefaultAudio;
}

SharedData::~SharedData()
{
    OutputMixer::Destroy(_outputMixerPtr);
    TransmitMixer::Destroy(_transmitMixerPtr);
    if (_audioDevicePtr) {
        _audioDevicePtr->Release();
    }
    delete _apiCritPtr;
    ProcessThread::DestroyProcessThread(_moduleProcessThreadPtr);
    Trace::ReturnTrace();
}

void SharedData::set_audio_device(AudioDeviceModule* audio_device)
{
    // AddRef first in case the pointers are equal.
    if (audio_device)
      audio_device->AddRef();
    if (_audioDevicePtr)
      _audioDevicePtr->Release();
    _audioDevicePtr = audio_device;
}

void SharedData::set_audio_processing(AudioProcessing* audioproc) {
  audioproc_.reset(audioproc);
  _transmitMixerPtr->SetAudioProcessingModule(audioproc);
  _outputMixerPtr->SetAudioProcessingModule(audioproc);
}

uint16_t SharedData::NumOfSendingChannels()
{
    int32_t numOfChannels = _channelManager.NumOfChannels();
    if (numOfChannels <= 0)
    {
        return 0;
    }

    uint16_t nChannelsSending(0);
    int32_t* channelsArray = new int32_t[numOfChannels];

    _channelManager.GetChannelIds(channelsArray, numOfChannels);
    for (int i = 0; i < numOfChannels; i++)
    {
        voe::ScopedChannel sc(_channelManager, channelsArray[i]);
        Channel* chPtr = sc.ChannelPtr();
        if (chPtr)
        {
            if (chPtr->Sending())
            {
                nChannelsSending++;
            }
        }
    }
    delete [] channelsArray;
    return nChannelsSending;
}

void SharedData::SetLastError(const int32_t error) const {
  _engineStatistics.SetLastError(error);
}

void SharedData::SetLastError(const int32_t error,
                              const TraceLevel level) const {
  _engineStatistics.SetLastError(error, level);
}

void SharedData::SetLastError(const int32_t error, const TraceLevel level,
                              const char* msg) const {
  _engineStatistics.SetLastError(error, level, msg);
}

}  //  namespace voe

}  //  namespace webrtc
