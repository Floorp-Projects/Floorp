/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/shared_data.h"

#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"
#include "webrtc/system_wrappers/include/trace.h"
#include "webrtc/voice_engine/channel.h"
#include "webrtc/voice_engine/output_mixer.h"
#include "webrtc/voice_engine/transmit_mixer.h"

namespace webrtc {

namespace voe {

static int32_t _gInstanceCounter = 0;

SharedData::SharedData(const Config& config)
    : _instanceId(++_gInstanceCounter),
      _apiCritPtr(CriticalSectionWrapper::CreateCriticalSection()),
      _channelManager(_gInstanceCounter, config),
      _engineStatistics(_gInstanceCounter),
      _audioDevicePtr(NULL),
      _moduleProcessThreadPtr(ProcessThread::Create("VoiceProcessThread")),
      _externalRecording(false),
      _externalPlayout(false) {
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
    _moduleProcessThreadPtr->Stop();
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

int SharedData::NumOfSendingChannels() {
  ChannelManager::Iterator it(&_channelManager);
  int sending_channels = 0;

  for (ChannelManager::Iterator it(&_channelManager); it.IsValid();
       it.Increment()) {
    if (it.GetChannel()->Sending())
      ++sending_channels;
  }

  return sending_channels;
}

int SharedData::NumOfPlayingChannels() {
  ChannelManager::Iterator it(&_channelManager);
  int playout_channels = 0;

  for (ChannelManager::Iterator it(&_channelManager); it.IsValid();
       it.Increment()) {
    if (it.GetChannel()->Playing())
      ++playout_channels;
  }

  return playout_channels;
}

void SharedData::SetLastError(int32_t error) const {
  _engineStatistics.SetLastError(error);
}

void SharedData::SetLastError(int32_t error,
                              TraceLevel level) const {
  _engineStatistics.SetLastError(error, level);
}

void SharedData::SetLastError(int32_t error, TraceLevel level,
                              const char* msg) const {
  _engineStatistics.SetLastError(error, level, msg);
}

}  // namespace voe

}  // namespace webrtc
